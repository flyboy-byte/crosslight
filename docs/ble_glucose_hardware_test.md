# Hardware test checklist: Glucose Monitor on Home + BLE relay (release 26.10.16)

Status: **not yet run.** Rewritten 2026-10-18 for release 26.10.16, replacing an older version that described
bonding and the reader-clock staleness rule (both gone). Everything in 26.10.16 is simulator-verified only; this
is the first time it meets the real device. Part A is the UI; Part B is the BLE write path.

Contract in one paragraph: service `0c0cd14d-6582-46f7-91b6-b4cf2a7cebac`, characteristic
`4e9715ab-ed4d-4a77-b8e7-f70fb5e26fc5`, 8-byte packet (u16 LE value mg/dL, `0xFFFF` = none; u8 trend 0 unknown /
1 falling fast / 2 falling / 3 steady / 4 rising / 5 rising fast; u8 flags bit0 HIGH, bit1 LOW, bit2 UNAVAILABLE;
u32 LE reading_time). **No pairing, no bonding, no encryption** (decided 2026-10-18); both Write and Write Without
Response are accepted. The reader keeps only the latest write and judges freshness by **when the write arrived**
(its own uptime), NOT by `reading_time`. Full contract: `PLAN.md`, "Companion-app contract v2".

## 0. Before you start

1. **Confirm the reader is actually on 26.10.16.** Settings > About, or the boot log line
   `CrossPoint version: 26.10.16-x4pro`. Update via Settings > Check for Updates (needs Wi-Fi) or a wired flash:
   `pio run -e x4pro -t upload --upload-port /dev/ttyACM0`. The previous flash still required encryption on the
   characteristic, so writes would fail against it.
2. **Serial log, done once and done right.** Run ONE monitor that writes to a file for the whole session; do not
   re-open the port with repeated `cat` calls (that dropped output and invented a "device stalled" scare on
   2026-10-18). For example: `pio device monitor -p /dev/ttyACM0 -b 115200 > /tmp/xteink.log` in the background, then
   `grep LIBRE_BLE /tmp/xteink.log`. Release builds log at INFO level, which is enough for the lines below.
3. Lines to look for: `Glucose relay GATT server up, advertising`, `Companion connected`,
   `Companion disconnected (reason=N)`, `onWrite 8 bytes: 6E 00 03 00 ...`, `write: value=110 trend=2 flags(H/L/U)=0/0/0`.
   (`trend=` is the firmware's internal enum, 0-based: steady prints as 2.)

## Part A: UI smoke (no phone needed)

| # | Do this | Expect |
| --- | --- | --- |
| A1 | Look at Home | A **Glucose Monitor** row with a droplet icon sits in File Transfer's old slot (between Library and Settings). No File Transfer row. |
| A2 | Settings > Network | A **File Transfer** row exists and opens the web server screen. Leaving it returns to Settings, not Home. |
| A3 | Utilities | No Glucose Monitor entry. **Watch Face** is the first entry. |
| A4 | Open Watch Face | Time, date, battery %, a divider, and a sample "112 mg/dL ^ / 2 min ago" (sample data, not live). Any button or a tap returns. |
| A5 | Open Glucose Monitor | Opens straight onto the monitor: a Wi-Fi \| Bluetooth switch at the top (selected half filled black), a mode line under it, a **Read** button at the bottom. Back returns to Home. **26.10.16 as released lands on the top row (Browse Files), not on Glucose Monitor** (cosmetic; fixed in commit `89df0f9d`, after the release, so the next release highlights the Glucose row). |
| A6 | Tap the **Bluetooth** half, then **Wi-Fi** half | Selection flips each time; the choice persists after leaving and re-entering. On Bluetooth the line under the switch reads "Hold Select to forget this phone" (a dead leftover, see PLAN.md). |
| A7 | On Wi-Fi: tap the mode line ("Auto 60s" / "Manual"), tap **Read** | Mode flips; Read fetches now. Tapping empty space does nothing (the old whole-screen tap-to-refresh is gone). |
| A8 | Wi-Fi source with the bridge configured (`/libre/url.txt`, `/libre/token.txt` on the SD card) | A real reading with the age shown under the number. This is the regression check that the Wi-Fi path still works. |
| A9 | Fonts | Nothing garbled or stacked (the release build only registers `UI_10`, `UI_12`, `SMALL`, `NOTOSANS_14`; new screens use only those). |

## Part B: BLE writes

**Setup.** Glucose Monitor open, **Bluetooth** selected. The status should read "Waiting for phone..." and the
serial log should show `Glucose relay GATT server up, advertising`. The reader only advertises while this screen is
open on the Bluetooth source; nothing else is needed on the reader.

**Writer.** Use whichever is handy:
- **nRF Connect for Mobile** on the Pixel (package `no.nordicsemi.android.mcp`): scan, connect to "CrossLight"
  (dev unit 7C:0C:5F:41:8E:51), open the unknown service, tap the unknown characteristic's row, set the value type
  to **BYTE ARRAY**, type the hex as one string with no spaces (e.g. `6E000300C651D56A`), SEND. No pairing prompt
  should appear. If the service list looks wrong (duplicates, missing characteristic) it is Android's cache: use the
  overflow menu > **Refresh services**, or disconnect and reconnect.
- **The companion app** (extraction's v0.2.6): add the device; it connects with autoConnect and writes with
  response. Ask the `extraction` session to report connect latency and write behavior.
- `scripts/fake_phone_ble.py` (BlueZ, laptop): **untested end to end.** It no longer needs pairing, but BlueZ must
  have seen the device (`bluetoothctl scan on` for a few seconds) and the script's own error messages still talk
  about bonding (ignore those). Pass `--address 7C:0C:5F:41:8E:51` because the advertised name is not always
  resolved in BlueZ's cache.

**Building a packet** (timestamp is just the current time; the reader ignores it for freshness, but keep it real):

```
python3 scripts/make_ble_glucose_packet.py --value 110 --trend steady
python3 scripts/make_ble_glucose_packet.py --value 260 --trend rising --flags high
```

**Write cases.**

| # | Packet | Expect on screen |
| --- | --- | --- |
| B1 | `--value 110 --trend steady` | Big **110**, `mg/dL`, a `-` trend mark, small age text starting near "0 s ago". Serial shows `onWrite 8 bytes: 6E 00 03 00 ...` then `write: value=110 ...`. **This is the case that previously rendered "Unavailable"; it is the main thing this test settles.** |
| B2 | `--value 260 --trend rising --flags high` | Big text **HIGH**, no number anywhere. |
| B3 | `--value 55 --trend falling --flags low` | Big text **LOW**, no number. |
| B4 | `--flags unavailable` (value anything) | Big text **Unavailable**. |
| B5 | `--no-value` (the 0xFFFF sentinel) | Big text **Unavailable**. |
| B6 | `--value 110 --age 700` (reading_time 11+ minutes old) | A normal **110**. This is the **changed** behavior: `reading_time` is ignored; freshness is by receipt. (The old doc expected Unavailable here.) |
| B7 | Write B1 once, then do nothing for 10+ minutes (re-check the screen after ~10 min, it re-judges on a ~10 s tick) | The number drops to **Unavailable** about 10 minutes after the write. |
| B8 | Write B1, then tap **Read** on the reader | The screen re-judges and redraws immediately; the number stays while fresh. |
| B9 | Disconnect the phone, wait, reconnect | The reader resumes advertising by itself (serial: `Companion disconnected`, then it must be connectable again without leaving the screen). This was the "advertising dies after the first disconnect" bug. |
| B10 | Leave the screen, re-enter it, connect again | Exactly **one** copy of the service in nRF Connect after "Refresh services" (the old build stacked a duplicate each entry). |
| B11 | Extraction's vector `E2 00 03 00 1F A1 CD 6A` | **226** steady. |

## If nothing shows

1. Is the monitor on **Bluetooth** (selected half filled) and does the status say "Waiting for phone..."? "Bluetooth
   unavailable" means `LibreBleServer::begin()` failed; on a real device that is a genuine bug, stop and capture the log.
2. No `Glucose relay GATT server up, advertising` line: the source is not Bluetooth or the screen is not open.
3. Advertising line present but the phone cannot find or connect: check the serial for `Companion connected`. No
   line at all means the connect request never reached the reader (not advertising, or out of range).
4. `onWrite` appears but the screen does not change: wait one ~10 s tick, then check `write: value=...`. If that line
   is present, the packet decoded and the problem is rendering/freshness (note the exact number it printed).
5. The write fails with an authentication/encryption error in the app: the reader is still on an OLD build that
   requires encryption. Reflash 26.10.16.

## Driving nRF Connect from the laptop over adb (what cost time on 2026-10-18)

- Wireless debugging: `adb pair IP:PAIRPORT CODE` once, then `adb connect IP:PORT`. **The connect port changes
  every few minutes and whenever the screen sleeps**; ask Logan for the current `IP:port` shown under Settings >
  Developer options > Wireless debugging.
- `adb shell screencap -p /sdcard/s.png && adb pull /sdcard/s.png` for screenshots. The device is 1080x2400; the
  viewer displays them at 900x2000, so **multiply every tap coordinate read from a screenshot by 1.2**.
- nRF Connect: the write dialog opens from the characteristic row; layout shifts down by a line once a value is
  shown, so re-screenshot before each tap. `adb shell input text` must be a single token (no spaces); clear the
  field first with repeated `input keyevent KEYCODE_DEL`.
- nRF Connect's overflow menu has **Refresh services**, **Delete bond information**, and **Show log**.

## When done

Record results in `PLAN.md` (top anchor) and tell the `extraction` session what you saw: connect latency,
whether writes needed the with-response type, anything surprising on the Android side. Its companion app is
released and waiting on the reader being flashed with its Glucose Monitor screen open on Bluetooth.
