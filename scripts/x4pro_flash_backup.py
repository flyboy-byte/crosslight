#!/usr/bin/env python3
"""Resumable full-flash backup for the X4 Pro over a flaky USB link.

Run with PlatformIO's Python (it has esptool's deps):
    ~/.platformio/penv/bin/python3 scripts/x4pro_flash_backup.py [port] [out.bin]
"""

import hashlib
import os
import struct
import sys
import time

sys.path.insert(0, os.path.expanduser("~/.platformio/packages/tool-esptoolpy"))

from esptool.cmds import attach_flash, detect_chip, detect_flash_size, reset_chip  # noqa: E402
from esptool.util import flash_size_bytes, hexify  # noqa: E402

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
OUT = sys.argv[2] if len(sys.argv) > 2 else "stock_x4pro_backup.bin"
PARTIAL = OUT + ".partial"
DONE = OUT + ".done"
FLASH_SIZE = 16 * 1024 * 1024
BLOCK = 64 * 1024
# Fallback packet sizes for blocks that fail with the default 4KB packets. A packet whose
# SLIP-escaped frame is an exact multiple of 64 bytes never finishes arriving over the
# S3's USB-Serial/JTAG (found at 0x267000 on the stock image: 4096+62 escapes+2 = 65*64).
FALLBACK_PACKETS = [1024, 1000, 256, 999, 100]
MAX_TRIES_PER_BLOCK = 30


def read_with_packet(esp, offset, length, packet):
    # Same protocol as ESPLoader.read_flash, which hardcodes 4KB packets.
    esp.check_command(
        "read flash", esp.ESP_CMDS["READ_FLASH"], struct.pack("<IIII", offset, length, packet, 1)
    )
    data = b""
    while len(data) < length:
        esp._port.timeout = 3
        data += esp.read()
        esp.write(struct.pack("<I", len(data)))
    digest = hexify(esp.read()).lower()
    if len(data) != length or digest != hashlib.md5(data).hexdigest():
        raise RuntimeError(f"digest mismatch with packet={packet}")
    return data


def connect():
    for attempt in range(1, 11):
        try:
            # default-reset on USB-Serial/JTAG resets straight into download mode,
            # so the stock app never runs between blocks and can't change flash mid-dump.
            esp = detect_chip(PORT, 115200, "default-reset")
            esp = esp.run_stub()
            attach_flash(esp)
            size = detect_flash_size(esp)
            if size != "16MB":
                sys.exit(f"Detected flash size {size}, expected 16MB. Refusing to continue.")
            esp.flash_set_parameters(flash_size_bytes(size))
            return esp
        except Exception as e:  # noqa: BLE001
            print(f"  connect attempt {attempt} failed: {e}", flush=True)
            time.sleep(2)
    sys.exit("Could not connect after 10 attempts.")


def close(esp):
    try:
        esp._port.close()
    except Exception:  # noqa: BLE001
        pass


def load_done():
    if not os.path.exists(DONE):
        return set()
    with open(DONE) as f:
        return {int(line) for line in f if line.strip()}


def main():
    if not os.path.exists(PARTIAL):
        with open(PARTIAL, "wb") as f:
            f.truncate(FLASH_SIZE)
    done = load_done()
    blocks = [a for a in range(0, FLASH_SIZE, BLOCK) if a not in done]
    print(f"{len(done)} blocks already done, {len(blocks)} to read.", flush=True)

    esp = connect()
    failures = 0
    start = time.time()
    with open(PARTIAL, "r+b") as out, open(DONE, "a") as done_log:
        for n, addr in enumerate(blocks, 1):
            tries = 0
            while True:
                tries += 1
                try:
                    if tries == 1:
                        data = esp.read_flash(addr, BLOCK)
                    else:
                        packet = FALLBACK_PACKETS[(tries - 2) % len(FALLBACK_PACKETS)]
                        print(f"  0x{addr:06x} retrying with {packet}-byte packets", flush=True)
                        data = read_with_packet(esp, addr, BLOCK, packet)
                    break
                except Exception as e:  # noqa: BLE001
                    failures += 1
                    print(f"  0x{addr:06x} try {tries} failed: {e}", flush=True)
                    if tries >= MAX_TRIES_PER_BLOCK:
                        sys.exit(f"Block 0x{addr:06x} failed {tries} times. Progress saved; rerun to resume.")
                    close(esp)
                    time.sleep(1)
                    esp = connect()
            out.seek(addr)
            out.write(data)
            out.flush()
            done_log.write(f"{addr}\n")
            done_log.flush()
            pct = 100 * (len(done) + n) / (FLASH_SIZE // BLOCK)
            print(f"0x{addr:06x} ok  {pct:5.1f}%  ({failures} retries so far, {time.time() - start:.0f}s)", flush=True)

    print("All blocks read. Verifying whole image against on-chip MD5...", flush=True)
    with open(PARTIAL, "rb") as f:
        image = f.read()
    local_md5 = hashlib.md5(image).hexdigest()
    for attempt in range(1, 6):
        try:
            chip_md5 = esp.flash_md5sum(0, FLASH_SIZE)
            if isinstance(chip_md5, bytes):
                chip_md5 = chip_md5.hex()
            break
        except Exception as e:  # noqa: BLE001
            print(f"  md5 attempt {attempt} failed: {e}", flush=True)
            close(esp)
            time.sleep(1)
            esp = connect()
    else:
        sys.exit("Could not get on-chip MD5. Image kept as .partial, NOT verified.")

    print(f"  file MD5: {local_md5}")
    print(f"  chip MD5: {chip_md5.lower()}")
    if local_md5 != chip_md5.lower():
        sys.exit("MISMATCH. Image kept as .partial. Delete .partial and .done and rerun.")

    os.replace(PARTIAL, OUT)
    os.remove(DONE)
    print(f"VERIFIED. Backup written to {OUT}")
    print(f"  sha256: {hashlib.sha256(image).hexdigest()}")
    reset_chip(esp, "hard-reset")
    close(esp)


if __name__ == "__main__":
    main()
