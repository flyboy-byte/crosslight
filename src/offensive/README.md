# src/offensive/

Quarantined Wi-Fi/BLE **offensive and active-audit** tools. Paused 2026-10-04. These are compiled into
the firmware but **hidden from the Utilities menu** unless the SD card holds an (empty) flag file
`/offensive/enabled` (read once per boot by `src/utilities/UtilityRegistry.cpp`).

For Logan's **own hardware only** — his own APs/laptops/BLE devices. Never third-party, shared, dorm, or
university network infrastructure.

## What's here

- **Wi-Fi engines:** `FrameBuilder`, `AttackTx`, `TxRadio`, `RandomMac`, `ActiveAuditGate`,
  `CaptureScanner`, `HarvestScanner`, `Eapol`, `Pcap`, `PcapSink`, `ThreatDetect`.
- **BLE engines:** `BleBeacon`, `BleSpoofer`.
- **`flock/`** — passive surveillance-device (Flock camera) detector.
- **`activities/`** — the screens: beacon flood, evil twin, BLE spoof, PCAP capture, PMKID harvest,
  Wi-Fi threat detection, camera scan.

The **general** receive-only tools (Wi-Fi Analyzer, Bluetooth Scanner) are **not** here — they stay in
`src/activities/utilities/` + `src/wifiaudit/` (ApScanner/WifiFrame) + `src/bleaudit/`.

Targeted **deauth (6a)** is **not in this tree** — it lives in a `git stash`, never compiled.

## Full docs

See **[`docs/crosslight/offensive/`](../../docs/crosslight/offensive/)** — README, STATUS, ARCHITECTURE,
TOOLING, ROADMAP. Start with its README.
