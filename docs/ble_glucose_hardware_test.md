# BLE glucose relay: real-hardware test checklist

Status: test not yet run. For verifying `LibreBleServer`/`GlucoseMonitorActivity`'s BLE path
against a real BLE client, independent of extraction's companion app — its BLE writer (Phase 3)
isn't built yet, so this is the only way to exercise CrossLight's GATT server for real right now.
Needs the X4 Pro flashed with 26.10.14+, and either **nRF Connect for Mobile on a phone** (below)
or **the laptop itself** acting as the BLE central (`scripts/fake_phone_ble.py`, further down) —
same reader, same contract, two different ways to write to it.

Service `0c0cd14d-6582-46f7-91b6-b4cf2a7cebac`, characteristic `4e9715ab-ed4d-4a77-b8e7-f70fb5e26fc5`
(frozen, extraction DECISIONS.md D-020). The characteristic requires a bonded link
(`WRITE_ENC`) — an unbonded write will be rejected by the stack before CrossLight's own code
ever sees it, which is itself worth confirming (see step 3).

## Setup

1. On the X4 Pro: open **Utilities**. Tap the **Glucose Source** row (just above Glucose Monitor
   in the list) until its switch shows checked and the subtitle reads **"Bluetooth"** — this is
   persisted (`SETTINGS.glucoseSource`), so it sticks for every future open, not just this one.
   (The in-screen Right-button toggle inside Glucose Monitor still works too and updates the same
   setting, but needs this list-level toggle to avoid being an accidental page-turn press.)
2. Open **Glucose Monitor**. The small text under the header should already read **"Bluetooth"**
   (not "Wi-Fi · ...") — if it doesn't, the toggle in step 1 didn't take; check that first before
   troubleshooting anything else. The GATT server/advertising is foreground-only and only runs
   while this screen is open *and* this source is selected; the device won't even be visible in a
   scan otherwise.
3. In nRF Connect's scanner, look for a device named **"CrossLight"**. If it's not there, go back
   to step 2 before troubleshooting anything else.
4. Tap to connect. nRF Connect may show it as connected but **not yet bonded** — that's expected;
   bonding happens on the first write attempt (see step 3 of the write cases below), not at
   connect time.

## Building a packet

Each write below is 8 bytes: `value_mgdl` (u16 LE), `trend` (u8), `flags` (u8), `reading_time`
(u32 LE, Unix UTC seconds). Use the helper script rather than hand-computing bytes — the
timestamp has to be genuinely recent for anything testing the "fresh" cases, and it goes stale
(by the firmware's own 10-minute rule) if you build it ahead of time and sit on it:

```
python3 scripts/make_ble_glucose_packet.py --value 110 --trend steady
```

Run it **immediately before each write** for every "fresh" case below — it prints the hex string
to paste into nRF Connect's write dialog (set the dialog to HEX, not text/UTF-8).

## Write cases

For each one: run the script command shown, paste the printed hex into nRF Connect, write it to
the characteristic (either write type works — CrossLight's characteristic allows both Write and
Write Without Response; for this test, **prefer plain "Write"**, not "Write Without Response" —
nRF Connect shows an explicit success/failure result for a request, which is more useful for
debugging than a no-response write that fails silently). Allow a few seconds for the screen to
update (BLE is push-driven but staleness is re-checked on a ~10s cadence, so a redraw can lag the
write by up to that long).

| # | Command | Expect on screen |
| --- | --- | --- |
| 1 | `--value 110 --trend steady` | Big number **110**, unit `mg/dL`, a `-` trend mark, small age text ("0 s ago" or similar, counting up) |
| 2 | `--value 260 --trend rising --flags high` | Big text **HIGH** — no number anywhere. This is correct, not a bug: HIGH/LOW/UNAVAILABLE never render a value (agent-api.md's rule, enforced on this path too). |
| 3 | `--value 55 --trend falling --flags low` | Big text **LOW** — again, no number shown. |
| 4 | `--flags unavailable` | Big text **Unavailable**. |
| 5 | `--no-value` (the 0xFFFF sentinel, no flags set) | Big text **Unavailable** — confirms the sentinel alone forces this even with a fresh timestamp and no explicit flag. |
| 6 | `--value 110 --age 700` (reading stamped ~11.5 min in the past) | Big text **Unavailable** — *not* "Stale · Nm ago" the way the Wi-Fi path would show it. This is intentional: the BLE relay's rule is stricter (never show a number once it's too old), see `LibreBlePacket.h`'s comment on `bleReadingFromPacket()`. |
| 7 | (cross-check) Write the literal bytes `E2 00 03 00 1F A1 CD 6A` — extraction's own test vector, matches `LibreBlePacketTest.cpp`'s `MatchesExtractionTestVector` | Same as case 6 (**Unavailable**) by the time you actually run this — that vector's timestamp is 2026-10-13T03:10:23Z, which is already old. If you ever want to see it as a *fresh* 226 mg/dL steady reading instead, use the script: `--value 226 --trend steady` and compare the hex it prints against `E2 00 03 00 1F A1 CD 6A` with the last 4 (timestamp) bytes ignored — the rest should match exactly. |

## If nothing shows at all

Check in this order:

1. **Is the chip under the header actually "Bluetooth"?** If it still says "Wi-Fi", the Right
   button press didn't register, or you're reading an old screenshot/memory of the screen. BLE
   writes are silently ignored while the Wi-Fi source is active — that's by design, not a bug.
2. **Does the small status line say "Waiting for phone..."?** That means advertising is up and
   nothing's connected/written yet — normal before your first connect, a problem if it never
   changes after a successful-looking write.
3. **Does the small status line say "Bluetooth unavailable"?** That means `LibreBleServer::begin()`
   itself failed — this should not happen on real x4pro hardware (it's the expected result only on
   the desktop simulator, which has no BLE radio at all). If you see this on the real device,
   that's a genuine bug — stop and report it rather than continuing the checklist.
4. **Did nRF Connect report "Insufficient Authentication" / "Insufficient Encryption" on the
   write?** The characteristic requires a bonded link and the stack hasn't bonded with your phone
   yet. Back out to Android's own Bluetooth settings, pair with "CrossLight" there directly (no
   PIN should be needed — `sc=true, mitm=false` means "Just Works" pairing), then reconnect in
   nRF Connect and retry the write.
5. **Did the write succeed in nRF Connect (green checkmark / "Success") but the screen never
   changes?** Give it the full ~10 seconds (see "Write cases" above) before concluding anything's
   wrong. If it's still stuck after that, this is the actual bug to report — the GATT
   wiring/decoding has a real problem, not a test-setup mistake.

## Laptop-side "fake phone" (no nRF Connect / no phone app needed)

`scripts/fake_phone_ble.py` is the same idea as the nRF Connect section above, but runs as a BLE
central directly from Logan's Arch laptop — useful for testing the reader before extraction's
real companion app writer (Phase 3) exists, without needing a phone in hand.

**Why BlueZ D-Bus, not `bleak`:** `bleak` was the first choice, but it isn't installed on this
machine (`python3 -c "import bleak"` fails) — `pip list` shows `dbus-fast` (bleak's own Linux
backend) already present, but not the `bleak` package itself. Nothing in this repo installs
anything; the script instead uses `dbus-python`, which **is** already installed system-wide, to
talk to BlueZ's own D-Bus GATT API directly. If `bleak` ever does get installed deliberately,
this script doesn't need to change — it's a reasonable rewrite target then, not a requirement now.

### Evidence tags (matches extraction's `DOCUMENTED`/`INFERRED`/`UNKNOWN` convention)

- `DOCUMENTED`: the BlueZ D-Bus GATT API shape this script uses (`Device1.Connect()`,
  `GattCharacteristic1.WriteValue(value, options)`, `ObjectManager.GetManagedObjects()`) — this is
  BlueZ's own published D-Bus API, not guessed.
- `INFERRED`: that pairing will be "Just Works" (no PIN/passkey prompt) against CrossLight's
  `setSecurityAuth(bonding=true, mitm=false, sc=true)`. That's what those settings are supposed to
  produce, but it hasn't been confirmed against this laptop's actual BlueZ agent behavior yet.
- `UNKNOWN`: whether a `command`-type write (Write Without Response — what the real companion app
  will actually use) surfaces a failure back through BlueZ's D-Bus call at all, or just succeeds
  locally and drops silently on the air. This script defaults to `request`-type writes (`--write-
  type request`) for exactly that reason — same call as nRF Connect's plain "Write," for a clear
  pass/fail. Pass `--write-type command` deliberately if you want to test the production write
  type's actual behavior, understanding that a silent failure is possible and wouldn't be this
  script's fault.
- **Not claimed to work** until Logan runs it against the real e-reader — this is the script as
  written, not a report of a result.

### Bonding the laptop with the e-reader (one-time, `DOCUMENTED` BlueZ flow)

The script does **not** automate pairing — do this once via `bluetoothctl` first (the GATT
characteristic requires a bonded link; an unpaired write will be rejected before CrossLight's own
code ever runs):

```
bluetoothctl
power on
agent on
default-agent
scan on
# wait for a line mentioning "CrossLight" (make sure the e-reader's Glucose
# Monitor screen is open with the source set to Bluetooth first), note its
# address, e.g. AA:BB:CC:DD:EE:FF
scan off
pair AA:BB:CC:DD:EE:FF
trust AA:BB:CC:DD:EE:FF
exit
```

If `pair` fails or hangs:

- Make sure the adapter is pairable: `bluetoothctl pairable on` before `pair`.
- A stale bond from an earlier flash can cause this (NimBLE's bond store lives in the ESP32's NVS
  and gets wiped on some reflashes, but the laptop's half of the bond doesn't know that) — run
  `bluetoothctl remove AA:BB:CC:DD:EE:FF`, then `pair` again from a clean state on both sides.
- Confirm the Bluetooth service is actually running: `systemctl is-active bluetooth`.

### Running it

```
python3 scripts/fake_phone_ble.py --value 140 --trend steady         # one fresh write
python3 scripts/fake_phone_ble.py --value 260 --high                 # HIGH flag
python3 scripts/fake_phone_ble.py --unavailable                      # UNAVAILABLE flag
python3 scripts/fake_phone_ble.py --raw "E2 00 03 00 1F A1 CD 6A"    # extraction's exact test vector
python3 scripts/fake_phone_ble.py --value 110 --loop                 # fresh write every 60s; Ctrl+C, then
                                                                       # watch the reader age it out on its own
```

It looks up the bonded device by name (`CrossLight` by default; `--address` skips that and
connects directly), waits for GATT service discovery to finish, then writes. Error messages are
written to point at the likely fix (not bonded yet, bond gone stale, services not resolved) rather
than just surfacing a raw D-Bus exception.

## When done

Message the `extraction-f0` session with the result — especially anything about connection
parameters, bonding behavior, or write type that surprised you, since Phase 3 (the companion
app's own BLE writer) will be built to match what's real on the actual device, not what was
assumed while scoping this.
