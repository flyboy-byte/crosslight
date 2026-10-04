# Offensive toolkit — status

What is actually built, versus simulator-checked, versus hardware-tested, versus open. Last updated
**2026-10-04**. Legend: ✅ done/verified · 🚧 built but not at the next bar · ❌ not built / blocked.

> [!IMPORTANT]
> **Paused 2026-10-04.** Development is stopped. The offensive tools below are compiled but hidden
> behind the SD flag `/offensive/enabled` (see [README](README.md)). Only the two general utilities
> (Wi-Fi Analyzer, Bluetooth Scanner) remain in the normal menu. This table is a frozen snapshot of
> where things stood at the pause, kept for whoever resumes.

> The honest distinction is the point of this table. "On the branch, builds clean, simulator-checked"
> is **not** the same as "flash-tested on the X4 Pro." Radio behavior is only ever real on hardware.

## Release history (what's actually on a device via OTA)

| Release | Date | Toolkit content |
|---------|------|-----------------|
| 26.10.1 | 2026-10-01 | **Passive slices 1–5** — shipped, **hardware-tested** (UI + scans confirmed on device). |
| 26.10.2 | 2026-10-01 | (utilities/calc — no toolkit change) |
| 26.10.3 | 2026-10-01 | (Bible — no toolkit change) |
| 26.10.4 | 2026-10-01 | **Active tools 6b / 6c / 6d shipped** (beacon flood, evil twin, BLE spoof). Superseded by 26.10.5 (never flashed). |
| 26.10.5 | 2026-10-01 | Adds **vendor labeling** (Wi-Fi OUI + BLE company-id). Active tools + Compare + vendor labels. |
| 26.10.6 | 2026-10-03 | **Wi-Fi radio-init fix** (`WIFI_MODE_STA` promiscuous bring-up — NULL no-ops when Wi-Fi is off). Fixes Scan/Threats/PMKID "Radio unavailable" on fresh boot. |

**Hardware-verified 2026-10-03 (serial-confirmed on the X4 Pro):** BLE scan loads signatures, opens the
4,041-entry company-id DB, and matches live advertisers → **Bluetooth Scanner + vendor labeling work**.
Wi-Fi promiscuous was failing (`WIFI_NOT_INIT`) → fixed in 26.10.6 (not yet re-flashed at the pause).
PCAP/PMKID also need `/wifiaudit/` on the SD (created 2026-10-03). Camera Scan needs `/flock/signatures.json`
(placeholders copied 2026-10-03).

## Passive tools (ship in every build, no gate)

| | Tool | Activity | State |
|---|------|----------|-------|
| ✅ | Wi-Fi AP scan (SSID/BSSID/channel/encryption/RSSI) | `WifiScanActivity` | Shipped 26.10.1, hardware-tested. |
| ✅ | Threat detection (evil-twin + deauth-flood) | `WifiThreatActivity` | Shipped 26.10.1, hardware-tested. Pattern-based, needs no SD data. |
| ✅ | PCAP capture to SD | `WifiCaptureActivity` | Shipped 26.10.1. 🚧 **owed: confirm the `.pcap` files actually land on the card and radio releases on exit.** |
| ✅ | EAPOL / PMKID + hashcat 22000 export | `PmkidHarvestActivity` | Shipped 26.10.1. Clientless PMKID live; full handshake via captured pcap offline. 🚧 same SD-write confirmation owed. |
| ✅ | Passive BLE scan + fingerprint | `BleScanActivity` | Shipped 26.10.1. `signatures.json` copied to `/bleaudit/` on the card 2026-10-01, so matching labels devices now (pending a hardware confirm that labels actually appear). |
| ✅ | Flock / surveillance-device detector | `CameraScanActivity` | Built + host-tested; deprioritized (real-world Wi-Fi signal is the limiter, not the code). Passive/receive-only. |
| 🚧 | **Vendor labeling** (Wi-Fi OUI + BLE company id) | `WifiScanActivity` / `BleScanActivity` + `src/util/VendorDb` | Built 2026-10-01. AP rows show manufacturer (IEEE OUI, 40,179), BLE shows manufacturer for any advertiser with a company id (BT SIG, 4,041), on top of the curated signatures. On-SD fixed-width binary-search (`/vendordb/*.bin`, staged on the card), ~0 flash/RAM. Pure core host-tested (`test/vendor_lookup`, 7). **On branch, unreleased, NOT flash-tested.** |

## Active tools (gated: compile flag + per-boot `ActiveAuditGate`)

| | Tool (slice) | Activity | State |
|---|------|----------|-------|
| 🚧 | Beacon flood (6b) | `BeaconFloodActivity` | Built, host-tested (`RandomMac`), simulator-checked (no-radio shell). Released 26.10.4; **NOT yet flash-tested.** Commit `aef086b0`. |
| 🚧 | Evil-twin captive portal (6c) | `EvilTwinActivity` | Built, simulator-checked. Clones a typed SSID as an open AP + plain test landing page (deliberately **not** a credential form). Released 26.10.4; **NOT yet flash-tested.** Commit `8c7e86f3`. |
| 🚧 | BLE advertisement spoof (6d) | `BleSpoofActivity` | Built, host-tested (`BleBeacon` iBeacon builder), simulator-checked. Generic test profiles only (named device + example iBeacon), no real-product impersonation. Released 26.10.4; **NOT yet flash-tested.** Commit `9568d65f`. |
| ❌ | Targeted deauth (6a) | `DeauthActivity` | **Code-complete in a git stash, never compiled, build blocked.** See below. |

### Foundation (shared by the active tools)

| | Piece | State |
|---|------|-------|
| ✅ | `FrameBuilder` (deauth/disassoc/beacon byte builders) | Merged, **byte-reviewed against IEEE 802.11** (correct), host-tested. |
| ✅ | `AttackTx` (single gated TX edge) + `ActiveAuditGate` | Merged, host-tested gate. |
| ✅ | `TxRadio` (STA-mode raw-TX bring-up) + `RandomMac` | Built 2026-10-01 for 6b; `RandomMac` host-tested. |
| ✅ | `BleSpoofer` + `BleBeacon` (BLE advertiser + iBeacon builder) | Built 2026-10-01 for 6d; `BleBeacon` host-tested. |

## 6a (targeted deauth)

**Status: code complete, in `git stash` (`stash@{0}`, message "6a deauth ..."), NOT committed, NEVER
compiled, build blocked.**

What the stash holds: `DeauthActivity.{h,cpp}` (scan APs via `ApScanner` → tap a target → broadcast
deauth frames via `FrameBuilder::buildDeauth` + `AttackTx::transmitFrame`), the registry entry, the
`STR_DEAUTH*` strings, and the two build-level pieces deauth needs (a linker flag in `[env:x4pro]` and
the matching `__wrap_` function in `AttackTx.cpp`).

**Why it's parked:** deauth is the one purely-*disruptive* tool (its function is knocking devices off a
network), and per the gating policy it ships in the public release binary. When Claude went to run the
routine i18n codegen step for it, the auto-mode safety classifier denied the command. Claude did **not**
work around the denial (no retry; did not route the build through the user) and stashed the code instead.
See [TOOLING.md](TOOLING.md#i18n-adding-ui-strings).

**For whoever resumes it (Codex or Logan):**
- `git stash list` / `git stash show -p stash@{0}` to inspect it; `git stash pop` to restore it.
- **Treat it as unverified — it was never compiled.** Expect to fix compile errors.
- The deauth-enable mechanism (why raw deauth is dropped by the stock `esp_wifi` blob and the
  `-Wl,-wrap=ieee80211_raw_frame_sanity_check` linker-wrap that lets it through) is carried **in the
  stash itself** — the stash's `AttackTx.cpp` diff and `platformio.ini` diff. The current in-tree
  `src/offensive/AttackTx.cpp` does **not** contain the wrap, and `platformio.ini` does **not** carry
  the wrap flag (both were kept in the stash on purpose, so nothing weaponized compiles by default).
- Fragile part: the linker wrap depends on a closed-blob symbol name and can silently stop working on an
  Arduino-ESP32 SDK bump (deauth frames would then build fine but never transmit). Re-check on SDK updates.
- It needs an on-hardware test like every radio tool (does a client on your own test AP actually drop).

## If the pause is ever lifted — what was owed

Frozen at the pause; do these in order only if resuming:

1. **Enable + flash-test 6b/6c/6d.** Drop `/offensive/enabled` on the SD, reboot, OTA the latest build,
   verify beacon flood / evil twin / BLE spoof actually transmit (needs a 2nd device to observe).
2. **Confirm passive SD behavior** on hardware: PCAP/`.22000` files land in `/wifiaudit/`, radio
   releases on exit, Flock matching works once real fingerprints are in `/flock/signatures.json`.
3. **Resolve 6a** (deauth) — Logan/Codex's call; it's in the stash, never compiled. See above.
4. Optional hardening / new tools — see [ROADMAP.md](ROADMAP.md).
