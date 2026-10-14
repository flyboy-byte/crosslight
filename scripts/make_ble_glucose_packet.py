#!/usr/bin/env python3
"""Builds the 8-byte BLE glucose relay packet (extraction DECISIONS.md D-020)
for manual testing with nRF Connect against CrossLight's GATT characteristic.

See docs/ble_glucose_hardware_test.md for the full test checklist this is
for. Quick use:

  python3 scripts/make_ble_glucose_packet.py --value 110 --trend steady
  python3 scripts/make_ble_glucose_packet.py --flags high --value 260
  python3 scripts/make_ble_glucose_packet.py --flags unavailable
  python3 scripts/make_ble_glucose_packet.py --no-value
  python3 scripts/make_ble_glucose_packet.py --value 110 --age 700   # stale

The timestamp is always "now minus --age seconds" at the moment this runs --
run it immediately before writing the bytes in nRF Connect, since a "fresh"
case stops being fresh after ~10 minutes (the same cutoff the firmware
enforces).
"""
import argparse
import struct
import time

TREND_CODES = {
    "unknown": 0,
    "falling_fast": 1,
    "falling": 2,
    "steady": 3,
    "rising": 4,
    "rising_fast": 5,
}

FLAG_BITS = {"high": 0x01, "low": 0x02, "unavailable": 0x04}

NO_VALUE = 0xFFFF


def build_packet(value: int, trend_code: int, flags: int, reading_time: int) -> bytes:
    """The one place this 8-byte layout is encoded -- fake_phone_ble.py imports
    this directly so the two tools can never drift apart on the wire format."""
    return struct.pack("<HBBI", value, trend_code, flags, reading_time)


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--value", type=int, default=100, help="glucose value in mg/dL (default 100)")
    p.add_argument("--no-value", action="store_true", help="send the 0xFFFF sentinel instead of --value")
    p.add_argument("--trend", choices=TREND_CODES, default="steady")
    p.add_argument(
        "--flags",
        default="",
        help="comma-separated: high,low,unavailable (default none)",
    )
    p.add_argument(
        "--age",
        type=int,
        default=0,
        help="seconds before now to timestamp the reading (default 0 = fresh now; "
        "the firmware treats >=600 as stale)",
    )
    args = p.parse_args()

    value = NO_VALUE if args.no_value else args.value
    trend_code = TREND_CODES[args.trend]
    flags = 0
    for name in (f.strip() for f in args.flags.split(",") if f.strip()):
        if name not in FLAG_BITS:
            raise SystemExit(f"unknown flag {name!r}; choose from {list(FLAG_BITS)}")
        flags |= FLAG_BITS[name]
    reading_time = int(time.time()) - args.age

    packet = build_packet(value, trend_code, flags, reading_time)
    hex_bytes = " ".join(f"{b:02X}" for b in packet)

    print(f"value_mgdl   = {value}{' (0xFFFF sentinel)' if value == NO_VALUE else ''}")
    print(f"trend        = {args.trend} ({trend_code})")
    print(f"flags        = 0x{flags:02X} ({args.flags or 'none'})")
    print(f"reading_time = {reading_time} ({'now' if args.age == 0 else f'{args.age}s ago'})")
    print()
    print(f"Paste into nRF Connect's write dialog (HEX mode): {hex_bytes}")


if __name__ == "__main__":
    main()
