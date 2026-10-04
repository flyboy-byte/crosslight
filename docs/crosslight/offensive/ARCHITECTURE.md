# Offensive toolkit — architecture

How the radio tools are wired, so you can understand or extend them without re-reading every file. All
paths are relative to the repo root. Target is the **Xteink X4 Pro** (`[env:x4pro]`): ESP32-S3, 8 MB
PSRAM, 16 MB flash. Flash (not RAM) is the binding constraint; the whole toolkit costs well under 1 MB.

> **Post-pivot layout (2026-10-04).** The code is split into *general* radio utilities (stay in the
> normal menu) and *offensive* tools (quarantined in `src/offensive/`, hidden from the menu). The two
> share passive primitives, so the split is by file, documented below.

## General vs offensive

| | General (always in menu) | Offensive (hidden, `src/offensive/`) |
|--|--------------------------|--------------------------------------|
| Wi-Fi | `src/wifiaudit/` — `ApScanner`, `WifiFrame` | `FrameBuilder`, `AttackTx`, `TxRadio`, `RandomMac`, `ActiveAuditGate`, `CaptureScanner`, `HarvestScanner`, `Eapol`, `Pcap`, `PcapSink`, `ThreatDetect` |
| BLE | `src/bleaudit/` — `BleScanner`, `BleAd`, `BleMatcher`, `BleSignature(s)` | `BleBeacon`, `BleSpoofer` |
| Flock | — | `src/offensive/flock/` (whole module) |
| Vendor DB | `src/util/VendorDb`, `VendorLookup` (shared by both) | — |

Each tool is a **screen** (`Activity`) backed by a reusable engine class. General screens live in
`src/activities/utilities/` (`WifiScanActivity` = Wi-Fi Analyzer, `BleScanActivity` = Bluetooth
Scanner). Offensive screens live in `src/offensive/activities/`.

### `src/wifiaudit/` — general Wi-Fi (kept)

| File | What it is | Pure/host-testable? |
|------|------------|---------------------|
| `WifiFrame.{h,cpp}` | 802.11 management-frame **parser** (beacons, deauth/disassoc classify, encryption, SSID). | yes (`test/wifi_audit/WifiFrameTest.cpp`) |
| `ApScanner.{h,cpp}` | Passive promiscuous monitor: channel-hops 1–13, parses beacons into an AP set. RX callback copies raw frames into a FreeRTOS queue; `drain()` on the UI task runs the parser. **Promiscuous bring-up uses `WIFI_MODE_STA`** (not NULL — NULL no-ops when Wi-Fi is already off; fixed 26.10.6). | engine no, parser yes |

### `src/bleaudit/` — general BLE (kept)

| File | What it is | Pure/host-testable? |
|------|------------|---------------------|
| `BleAd.{h,cpp}` | BLE advertisement **parser**. | yes (`test/ble_audit/BleAdTest.cpp`) |
| `BleScanner.{h,cpp}` | Passive observer scan (NimBLE). Records any advertiser with a company id (for vendor labels) plus curated signature matches. Callback→queue→`drain()`. | engine no |
| `BleMatcher.cpp` / `BleSignature(s).{h,cpp}` | Device-fingerprint matching + the signature table from `/bleaudit/signatures.json`. | matcher yes (`BleMatcherTest.cpp`) |

### `src/offensive/` — Wi-Fi offensive engines

| File | What it is | Pure/host-testable? |
|------|------------|---------------------|
| `FrameBuilder.{h,cpp}` | 802.11 management-frame **builders** (`buildDeauth`/`buildDisassoc`/`buildBeacon`). Pure byte serializers. | **yes** (`FrameBuilderTest.cpp`) — byte-reviewed vs IEEE 802.11 |
| `RandomMac.h` | `makeLocallyAdministered()` — legalizes 6 random bytes into a valid LAA unicast MAC. | **yes** (`RandomMacTest.cpp`) |
| `TxRadio.{h,cpp}` | Minimal STA-mode radio bring-up for raw-frame TX. Never associates. | no (radio) |
| `AttackTx.{h,cpp}` | **The one place a frame goes on the air.** `transmitFrame()` + `buildSupportsActiveAudit()`. Double-gated. | no (radio) |
| `ActiveAuditGate.{h,cpp}` | Per-boot "operator confirmed" bool. | yes (`ActiveAuditGateTest.cpp`) |
| `CaptureScanner.{h,cpp}` + `Pcap.{h,cpp}` / `PcapSink.{h,cpp}` | Promiscuous capture-to-SD + PCAP format/sink. | `Pcap` yes (`PcapTest.cpp`) |
| `HarvestScanner.{h,cpp}` + `Eapol.{h,cpp}` | EAPOL/PMKID extraction + hashcat 22000 export. | `Eapol` yes (`EapolTest.cpp`) |
| `ThreatDetect.{h,cpp}` | Evil-twin + deauth-flood **detection** logic (consumes `ApScanner` output). | yes (`ThreatDetectTest.cpp`) |

### `src/offensive/` — BLE offensive engines

| File | What it is | Pure/host-testable? |
|------|------------|---------------------|
| `BleBeacon.h` | `buildIBeaconManufacturerData()` — pure iBeacon manufacturer-data serializer. | **yes** (`BleBeaconTest.cpp`) |
| `BleSpoofer.{h,cpp}` | Active BLE advertiser (NimBLE). Generic test profiles only. | no (radio) |

### `src/offensive/flock/` — surveillance-device detector

`FlockScanner`/`FlockFrame`/`FlockMatcher`/`FlockSignature(s)` — the promiscuous-capture primitive the
Wi-Fi tools were shaped after, plus its matcher. Passive/receive-only. Signatures load from
`/flock/signatures.json` (ships placeholder OUIs — no fabricated data). Host-tested
(`test/flock_matcher/`).

### Offensive activities (`src/offensive/activities/`)

`WifiThreatActivity`, `WifiCaptureActivity`, `PmkidHarvestActivity`, `CameraScanActivity` (Flock),
`BeaconFloodActivity` (6b), `EvilTwinActivity` (6c), `BleSpoofActivity` (6d).
`DeauthActivity` (6a) is **not in the tree** — it lives in a git stash (see STATUS.md).

## The gating model (how offensive is kept distinct)

General tools are receive-only and always in the menu. Offensive tools are gated **three ways**:

0. **SD menu flag `/offensive/enabled`.** `UtilityRegistry` reads it once per boot; absent → the
   offensive tiles don't appear at all. This is the "forget about it" switch. *(Added 2026-10-04.)*
1. **Compile flag `CROSSLIGHT_ENABLE_ACTIVE_AUDIT`** (in `[env:x4pro]`). When absent,
   `AttackTx::transmitFrame()` compiles to `return false` and the builders dead-strip.
   `buildSupportsActiveAudit()` (a `constexpr`) lets a UI show "disabled in this build" vs "running".
2. **Runtime `ActiveAuditGate` (per-boot bool).** `transmitFrame()` refuses until confirmed —
   independent of the UI, so a missed UI gate still cannot transmit. Active screens confirm it
   automatically on start (no modal — Logan's call) and keep an on-screen "your own gear only" warning.

> [!NOTE]
> Layers 1 and 2 only gate *transmit*. The passive-but-offensive tools (capture, harvest, threat
> detect, Flock) are gated only by layer 0 (the menu flag), since they don't transmit.

## The patterns worth copying

- **Pure builder + host test.** `FrameBuilder`, `RandomMac`, `BleBeacon` are dependency-free byte
  functions with known-answer tests. Anything with a wire format goes here first.
- **Passive scanner: callback → fixed queue → `drain()`.** The radio callback copies raw bytes into a
  fixed FreeRTOS queue and must not allocate; the UI task runs the host-tested parser in `drain()`.
  `ApScanner` and `BleScanner` are the templates.
- **Active tool: pure-build → single gated TX edge.** Build with a pure builder, transmit only through
  `AttackTx::transmitFrame()` (Wi-Fi) or `BleSpoofer` (BLE). Never call `esp_wifi_80211_tx` elsewhere.
- **One include + one registry line per tool.** A tool is added by including its header in
  `src/utilities/UtilityRegistry.cpp` and appending one entry — to `kGeneral` (always shown) or
  `kOffensive` (shown only with the SD flag). `HomeActivity` never changes.
- **"No radio" / "disabled" shells.** Every radio screen renders a truthful state on host/simulator (no
  radio) and on a build without the active flag (disabled), instead of appearing to work.
