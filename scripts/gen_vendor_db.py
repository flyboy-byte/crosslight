#!/usr/bin/env python3
"""Generate CrossLight's on-SD vendor-lookup databases from primary registries.

Wi-Fi OUIs come from the IEEE registry; BLE company identifiers from the
Bluetooth SIG's own assigned-numbers repo. Both are emitted as a compact,
FIXED-WIDTH, SORTED binary the firmware binary-searches directly on the SD card
(VendorDb) -- no multi-MB RAM load, no flash cost. This is deliberately a desktop
tool (like gen_i18n.py / make_wallpaper.py): the firmware never downloads or
parses the raw registries.

NEVER hand-edit the output or invent entries -- regenerate from the real
registries so the data stays verifiable (CrossLight's standing "verify, don't
assume" rule).

File format (all little-endian, matching the ESP32):
  header (16 bytes): magic 'CLV1' | uint32 count | uint32 record_size | uint32 reserved
  records (count x 36 bytes, sorted ascending by key):
    uint32 key | 32-byte name (UTF-8, NUL-padded, truncated on a char boundary)
  key = OUI as 0x00AABBCC (Wi-Fi) or the 16-bit company id (BLE).

Usage:
  python3 scripts/gen_vendor_db.py \
      --oui /path/oui.csv --btcid /path/company_identifiers.yaml --out /path/to/sd/vendordb
  (either source may be omitted to skip that file.)
"""
import argparse
import csv
import os
import re
import struct
import sys

MAGIC = b"CLV1"
NAME_LEN = 32
RECORD_SIZE = 4 + NAME_LEN  # 36


def encode_name(name: str) -> bytes:
    """UTF-8, truncated to NAME_LEN bytes without splitting a multi-byte char, NUL-padded."""
    raw = name.strip().encode("utf-8")
    if len(raw) > NAME_LEN:
        raw = raw[:NAME_LEN]
        # back off so we don't end mid-codepoint
        while raw and (raw[-1] & 0xC0) == 0x80:
            raw = raw[:-1]
        # also drop a lone lead byte of a now-cut sequence
        if raw and (raw[-1] & 0xC0) == 0xC0:
            raw = raw[:-1]
    return raw.ljust(NAME_LEN, b"\x00")


def write_db(path: str, entries: dict) -> None:
    """entries: {int key: str name}. Writes the sorted fixed-width db."""
    keys = sorted(entries)
    with open(path, "wb") as f:
        f.write(MAGIC)
        f.write(struct.pack("<III", len(keys), RECORD_SIZE, 0))
        for k in keys:
            f.write(struct.pack("<I", k))
            f.write(encode_name(entries[k]))
    print(f"  wrote {path}: {len(keys)} entries, {os.path.getsize(path)} bytes")


def parse_oui(csv_path: str) -> dict:
    """IEEE oui.csv -> {0x00AABBCC: org}. Only MA-L (24-bit) assignments."""
    out = {}
    with open(csv_path, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            if row.get("Registry") != "MA-L":
                continue
            assign = row.get("Assignment", "").strip()
            if len(assign) != 6:
                continue
            try:
                key = int(assign, 16)
            except ValueError:
                continue
            org = row.get("Organization Name", "").strip()
            if org and org.lower() != "private":
                out[key] = org
    return out


def parse_btcid(yaml_path: str) -> dict:
    """Bluetooth SIG company_identifiers.yaml -> {id: name}. Minimal parse, no PyYAML dep."""
    out = {}
    text = open(yaml_path, encoding="utf-8").read()
    # entries are "- value: 0x####" followed by "  name: '...'"
    for m in re.finditer(r"-\s*value:\s*(0x[0-9A-Fa-f]+|\d+)\s*\n\s*name:\s*(.+)", text):
        val = int(m.group(1), 0)
        name = m.group(2).strip().strip("'\"")
        if name:
            out[val] = name
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--oui", help="IEEE oui.csv")
    ap.add_argument("--btcid", help="Bluetooth SIG company_identifiers.yaml")
    ap.add_argument("--out", required=True, help="output dir (its /vendordb files)")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    if args.oui:
        entries = parse_oui(args.oui)
        if not entries:
            print("ERROR: no OUI rows parsed", file=sys.stderr)
            return 1
        write_db(os.path.join(args.out, "oui.bin"), entries)
    if args.btcid:
        entries = parse_btcid(args.btcid)
        if not entries:
            print("ERROR: no BT company ids parsed", file=sys.stderr)
            return 1
        write_db(os.path.join(args.out, "btcid.bin"), entries)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
