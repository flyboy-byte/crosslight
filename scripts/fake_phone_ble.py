#!/usr/bin/env python3
"""Laptop-side "fake phone": a BLE central that writes the frozen 8-byte
glucose relay packet (extraction DECISIONS.md D-020) to CrossLight's GATT
characteristic, for testing the reader without nRF Connect and without
extraction's companion app (its BLE writer, Phase 3, isn't built yet).

Backend: BlueZ's own D-Bus API via dbus-python. `bleak` would be the first
choice (per Logan's instructions) but is NOT installed on this machine, and
nothing here installs it -- see "Why BlueZ D-Bus, not bleak" below. Pairing
is NOT automated by this script; bond the laptop with the reader via
bluetoothctl first (one-time) -- see docs/ble_glucose_hardware_test.md's
"Laptop fake phone" section for the exact steps and what DOCUMENTED /
INFERRED / UNKNOWN means for each part of this tool.

Usage:
  python3 scripts/fake_phone_ble.py --value 140 --trend steady
  python3 scripts/fake_phone_ble.py --value 260 --high
  python3 scripts/fake_phone_ble.py --unavailable
  python3 scripts/fake_phone_ble.py --raw "E2 00 03 00 1F A1 CD 6A"
  python3 scripts/fake_phone_ble.py --value 110 --loop

Never prints or stores secrets -- there are none in this packet -- and never
touches anything under ~/projects/extraction.
"""
import argparse
import sys
import time

import dbus

from make_ble_glucose_packet import FLAG_BITS, NO_VALUE, TREND_CODES, build_packet

BLUEZ_SERVICE = "org.bluez"
DEVICE_IFACE = "org.bluez.Device1"
CHAR_IFACE = "org.bluez.GattCharacteristic1"
SERVICE_UUID = "0c0cd14d-6582-46f7-91b6-b4cf2a7cebac"
CHARACTERISTIC_UUID = "4e9715ab-ed4d-4a77-b8e7-f70fb5e26fc5"
DEFAULT_NAME = "CrossLight"
LOOP_INTERVAL_S = 60  # "once a minute, never faster" -- matches the real companion app's own floor


def parse_raw(raw: str) -> bytes:
    data = bytes.fromhex(raw.replace(" ", ""))
    if len(data) != 8:
        raise SystemExit(f"--raw must be exactly 8 bytes, got {len(data)}")
    return data


def packet_from_args(args) -> bytes:
    if args.raw is not None:
        return parse_raw(args.raw)
    value = NO_VALUE if args.no_value else args.value
    flags = 0
    if args.high:
        flags |= FLAG_BITS["high"]
    if args.low:
        flags |= FLAG_BITS["low"]
    if args.unavailable:
        flags |= FLAG_BITS["unavailable"]
    reading_time = int(time.time()) - args.age_s
    return build_packet(value, TREND_CODES[args.trend], flags, reading_time)


def get_managed_objects(bus):
    manager = dbus.Interface(bus.get_object(BLUEZ_SERVICE, "/"), "org.freedesktop.DBus.ObjectManager")
    return manager.GetManagedObjects()


def find_device_path(bus, name: str, address: str | None):
    objects = get_managed_objects(bus)
    candidates = []
    for path, ifaces in objects.items():
        dev = ifaces.get(DEVICE_IFACE)
        if not dev:
            continue
        if address and str(dev.get("Address", "")).lower() == address.lower():
            return path
        if not address and str(dev.get("Name", "")) == name:
            candidates.append((path, bool(dev.get("Paired", False))))
    if address:
        return None
    if not candidates:
        return None
    # Prefer an already-paired/bonded match; BlueZ keeps unpaired devices
    # around too after a scan, which would otherwise be ambiguous.
    for path, paired in candidates:
        if paired:
            return path
    return candidates[0][0]


def find_characteristic_path(bus, device_path: str, char_uuid: str, timeout_s: float = 10.0):
    """Polls (no GLib mainloop -- this is a handful of blocking calls, not a
    long-running listener) for the characteristic to appear as a child
    object of the device. It only exists once BlueZ has resolved GATT
    services after Connect(), which isn't guaranteed to be immediate."""
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        objects = get_managed_objects(bus)
        for path, ifaces in objects.items():
            chrc = ifaces.get(CHAR_IFACE)
            if chrc and path.startswith(device_path) and str(chrc.get("UUID", "")).lower() == char_uuid.lower():
                return path
        time.sleep(0.3)
    return None


def connect_device(bus, device_path: str) -> None:
    dev = dbus.Interface(bus.get_object(BLUEZ_SERVICE, device_path), DEVICE_IFACE)
    try:
        dev.Connect()
    except dbus.exceptions.DBusException as e:
        if "AlreadyConnected" in str(e):
            return
        raise SystemExit(
            f"Connect() failed: {e}\n"
            "If this says something about authentication or pairing, the laptop and the reader "
            "probably aren't bonded yet -- see the bonding steps in "
            "docs/ble_glucose_hardware_test.md before retrying."
        )


def write_characteristic(bus, char_path: str, data: bytes, write_type: str) -> None:
    chrc = dbus.Interface(bus.get_object(BLUEZ_SERVICE, char_path), CHAR_IFACE)
    value = dbus.Array([dbus.Byte(b) for b in data], signature=dbus.Signature("y"))
    options = dbus.Dictionary({"type": write_type}, signature=dbus.Signature("sv"))
    try:
        chrc.WriteValue(value, options)
    except dbus.exceptions.DBusException as e:
        raise SystemExit(
            f"WriteValue() failed: {e}\n"
            "'Not Authorized' / 'Not Permitted' here almost always means the bond doesn't exist or "
            "went stale (e.g. the e-reader was reflashed, clearing its bond store) -- "
            "`bluetoothctl remove <address>` on the laptop, then re-pair, then retry."
        )


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--value", type=int, default=100)
    p.add_argument("--no-value", action="store_true", help="0xFFFF sentinel instead of --value")
    p.add_argument("--trend", choices=TREND_CODES, default="steady")
    p.add_argument("--high", action="store_true")
    p.add_argument("--low", action="store_true")
    p.add_argument("--unavailable", action="store_true")
    p.add_argument("--age-s", type=int, default=0, dest="age_s", help="reading_time = now - this many seconds")
    p.add_argument("--raw", default=None, help='exactly 8 hex bytes, e.g. "E2 00 03 00 1F A1 CD 6A"; overrides all other packet fields')
    p.add_argument("--loop", action="store_true", help=f"write a fresh reading every {LOOP_INTERVAL_S}s until Ctrl+C (ignores --age-s and --raw)")
    p.add_argument("--name", default=DEFAULT_NAME, help="advertised device name to find (default: CrossLight)")
    p.add_argument("--address", default=None, help="skip name lookup and use this BLE address directly")
    p.add_argument("--write-type", choices=["request", "command"], default="request",
                    help="'request' (Write, with a BlueZ-level result) or 'command' (Write Without Response, "
                         "what the real companion app will use -- see UNKNOWN note in the docs about whether "
                         "failures surface for this type)")
    args = p.parse_args()

    if args.loop and args.raw:
        raise SystemExit("--loop and --raw don't combine -- --loop always sends a fresh reading")

    bus = dbus.SystemBus()
    device_path = find_device_path(bus, args.name, args.address)
    if not device_path:
        raise SystemExit(
            f"No device named {args.name!r}{' / address ' + args.address if args.address else ''} known to "
            "BlueZ. It must be bonded first (see docs/ble_glucose_hardware_test.md) -- an unpaired device "
            "that was never scanned won't show up here at all."
        )

    connect_device(bus, device_path)
    char_path = find_characteristic_path(bus, device_path, CHARACTERISTIC_UUID)
    if not char_path:
        raise SystemExit(
            "Connected, but never found the glucose characteristic. Either GATT service discovery hasn't "
            "finished yet (try again), or the reader isn't advertising this service right now -- on the "
            "e-reader, Glucose Monitor screen must be open with the source switched to Bluetooth."
        )

    if args.loop:
        print(f"Writing a fresh reading every {LOOP_INTERVAL_S}s. Ctrl+C to stop and watch it age out.")
        try:
            while True:
                packet = build_packet(args.value, TREND_CODES[args.trend],
                                       (FLAG_BITS["high"] if args.high else 0) |
                                       (FLAG_BITS["low"] if args.low else 0) |
                                       (FLAG_BITS["unavailable"] if args.unavailable else 0),
                                       int(time.time()))
                write_characteristic(bus, char_path, packet, args.write_type)
                print(f"wrote {' '.join(f'{b:02X}' for b in packet)} at {time.strftime('%H:%M:%S')}")
                time.sleep(LOOP_INTERVAL_S)
        except KeyboardInterrupt:
            print("\nStopped. The reader should show this reading going stale over the next ~10 minutes on its own.")
        return

    packet = packet_from_args(args)
    write_characteristic(bus, char_path, packet, args.write_type)
    print(f"Wrote {' '.join(f'{b:02X}' for b in packet)} ({args.write_type}) to {args.name}.")


if __name__ == "__main__":
    main()
