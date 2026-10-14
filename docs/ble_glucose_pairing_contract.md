# BLE glucose relay: reader-side pairing/bonding contract

Status: written 2026-10-14, for extraction's companion-app re-pair wizard (`docs/companion-app-
onboarding.md` section 4, step 2: "Receiver: clear its stored bond — exact step comes from the
CrossLight session's docs"). Answers from the reader's side only; extraction's own docs/behavior
are out of scope here. Evidence tagged `DOCUMENTED` (read directly from this repo's code or the
BLE spec's own defined behavior) / `INFERRED` (reasoned from documented behavior, not directly
observed) / `UNKNOWN` (needs the real hardware test to settle) — matching extraction's convention.

## a. Clearing the reader's bond and re-advertising (exact steps)

**`DOCUMENTED`** — built 2026-10-14 (platformio.ini version 26.10.15+) specifically so this
section could have a real answer instead of "doesn't exist yet":

1. On the X4 Pro: **Utilities → Glucose Monitor**.
2. Press **Right** until the chip under the header reads **"Bluetooth"**.
3. **Hold Confirm for about 1.5 seconds.** The button hint in that mode reads "Hold: forget".
4. The status line changes to **"Phone forgotten. Advertising for a new pairing."** — the reader
   has cleared every bond it holds (`NimBLEDevice::deleteAllBonds()`, `LibreBleServer::forgetBond()`)
   and dropped any live connection. It's immediately advertising again, ready for a fresh pair.

There's exactly one bond slot in practice (nothing else on this device bonds over BLE today), so
"forget" and "forget the phone specifically" are the same action — worth knowing if extraction's
wizard ever needs to distinguish multiple paired phones, which isn't supported here yet.

## b. What the GATT server does on a write after a one-sided bond mismatch

**Two different cases, with different evidence strength:**

- **A write attempted with no encryption at all** (a stranger, or a central that isn't even trying
  to use a stored key) — **`DOCUMENTED`**, straight from the characteristic's own properties
  (`NIMBLE_PROPERTY::WRITE_ENC` in `LibreBleServer.cpp`): the BLE/ATT spec defines this exact case.
  The stack rejects the Write at the ATT layer with an Error Response — Error Code "Insufficient
  Authentication" or "Insufficient Encryption" — **before `CharCallbacks::onWrite` is ever called**.
  CrossLight's application code has no visibility into this rejection; it's handled entirely by
  NimBLE's stack beneath the GATT server API this codebase uses.
- **A central that believes it still holds a valid LTK from an old bond, but the reader's NVS bond
  store no longer has a matching one** (the "reader forgot, phone didn't" half of extraction's
  described mismatch) — **`INFERRED`, not verified on this hardware**: per the BLE spec, encryption
  re-establishment on reconnect happens at the **link layer**, below GATT/ATT entirely. A peripheral
  that doesn't recognize the central's LTK fails the encryption procedure at that layer (commonly
  surfaces to the central's host stack as a disconnect with a reason like "Pin or Key Missing" /
  "Authentication Failure"), so there is **no ATT-level error code to point the phone's wizard at
  for this specific case** — it'll look like a connection that drops right after connecting, or
  never finishes connecting, not a GATT write failure with a distinguishable status code.
- **The reverse** (phone forgot, reader still thinks it's bonded) — **`UNKNOWN`**: whether the
  phone's BLE identity address stays stable enough for the reader's existing bond entry to even be
  relevant, or whether Android's address rotation means the reader just sees what looks like a new,
  unbonded device and goes through the no-encryption case above. Not reasoned out further here;
  needs the real two-device test.
- **Practical recommendation, since the two mismatch directions don't cleanly produce distinct
  codes:** point extraction's wizard at "connects but every write fails, or never finishes
  connecting" as the trigger condition for *both* directions, rather than trying to distinguish
  them by status code — and use (a) above (hold Confirm to forget) as the universal fix regardless
  of which side was actually stale. Capturing the actual HCI/GATT error Android logs on a real
  mismatch (as extraction's own doc already plans to) is the way to firm this up later, not
  guessing further here.

## c. Pairing method and on-screen behavior

**`DOCUMENTED`**, directly from `LibreBleServer::begin()`:
`NimBLEDevice::setSecurityAuth(/*bonding=*/true, /*mitm=*/false, /*sc=*/true)`.

- `mitm=false` means **Just Works** pairing — no passkey, no numeric comparison, no PIN on either
  side.
- The reader shows **nothing on screen during pairing** — there's no passkey-display code
  anywhere in `LibreBleServer`/`GlucoseMonitorActivity` (no `setSecurityPasskey` callback, no
  confirmation UI). Bonding happens silently from the reader's point of view; the only visible
  change is the status line going from "Waiting for phone..." to a live reading once the phone
  successfully connects and writes.

## d. Exact UI strings (for the phone's wording to match)

**`DOCUMENTED`**, from `lib/I18n/translations/english.yaml` / `GlucoseMonitorActivity.cpp`:

| Reader shows | String | When |
| --- | --- | --- |
| Source chip | `"Bluetooth"` | Always, while BLE source is selected (vs. `"Wi-Fi · ..."`) |
| Status line | `"Waiting for phone..."` | BLE source just selected, advertising, nothing written yet |
| Status line | `"Bluetooth unavailable"` | `LibreBleServer::begin()` failed — no radio. Should not happen on real x4pro hardware; only expected on the no-BLE desktop simulator. |
| Big text (no number) | `"Unavailable"` | Any of: no packet ever received; packet's `flags` has UNAVAILABLE set; `value_mgdl == 0xFFFF`; `reading_time` is ≥10 min old or future-dated; no trusted clock on the reader. The reader does **not** distinguish these with different wording — they all collapse to the same string, intentionally (never show a number in any of these cases). |
| Big text (no number) | `"HIGH"` / `"LOW"` | `flags` HIGH/LOW bit set and the reading isn't stale/unavailable — never a number alongside these. |
| Secondary status line | `"No trusted clock yet"` | Reader has no synced wall clock (`trustedtime::trustedNow() <= 0`) — shown alongside the "Unavailable" big text in that specific case. |
| Status line (after hold-to-forget) | `"Phone forgotten. Advertising for a new pairing."` | Immediately after the forget-bond action (see **a.** above). |

Related: `crosslight/PLAN.md` "Bluetooth glucose relay" section, `src/libre/LibreBleServer.{h,cpp}`,
`src/activities/utilities/GlucoseMonitorActivity.{h,cpp}`.
