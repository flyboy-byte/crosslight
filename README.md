<p align="center">
  <img src="docs/images/cover.jpg" alt="CrossLight running on an Xteink device" width="72%">
</p>

<h1 align="center">CrossLight</h1>

<p align="center">
  <strong>A personal fork of CrossPoint e-reader firmware, for the Xteink X4 Pro.</strong><br>
  The upstream reader, plus a Bible study suite, a few handy utilities,<br>
  and a passive Wi-Fi/BLE network-analysis toolkit — all on one e-ink device.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/board-Xteink%20X4%20Pro-2e8b57" alt="Xteink X4 Pro">
  <img src="https://img.shields.io/badge/chip-ESP32--S3-c51a4a" alt="ESP32-S3">
  <img src="https://img.shields.io/badge/based%20on-CrossPoint-5c4ee5" alt="based on CrossPoint">
  <img src="https://img.shields.io/badge/update-Wi--Fi%20OTA-informational" alt="Wi-Fi OTA">
  <img src="https://img.shields.io/badge/license-MIT-orange" alt="MIT">
</p>

<p align="center">
  <a href="#install">Install</a> •
  <a href="#what-crosslight-adds">What's added</a> •
  <a href="#whats-actually-working">Status</a> •
  <a href="#build-it-yourself">Build</a> •
  <a href="https://github.com/flyboy-byte/crosslight/releases">Downloads</a> •
  <a href="PLAN.md">PLAN.md</a>
</p>

---

> [!NOTE]
> **This is a personal fork, not an upstream project.** CrossLight tracks
> [CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader) and merges from it regularly;
> everything the reader does, CrossPoint built. This fork adds a Bible suite, some utilities, and a
> security toolkit on top, targeting one device (the Xteink X4 Pro) specifically. Most of the code,
> docs, and this README were written with [Claude Code](https://claude.com/claude-code); I direct it,
> test on real hardware, and make the calls. Not affiliated with or endorsed by the CrossPoint project.

## The short version

[CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader) is an open-source e-reader
firmware for Xteink devices — a genuinely good EPUB reader with a library, wireless file transfer,
themes, and 34 languages. **CrossLight** is my fork of it, pinned to the **X4 Pro** (ESP32-S3, 16 MB
flash, 8 MB PSRAM, touch, dual frontlight), that bolts on three things the upstream reader doesn't
have: a **Bible study suite**, a small set of **utilities**, and a **passive Wi-Fi/BLE toolkit** for
personal security research. Updates install over Wi-Fi from this repo's releases.

## What CrossLight adds

Everything in CrossPoint is still here (it's merged in, not replaced). On top of it:

- **Bible reader** — downloadable translations stored on the SD card, full-text search, bookmarks,
  cross-references.
- **Memory Work** — structured verse-memorization courses and lessons.
- **Bible Numbers** — short studies on the numbers in Scripture (7, 12, 40, 666 …), every claim
  tagged by evidence strength (`FACT` / `LITERARY PATTERN` / `TRADITION` / `DEBATE` / `SPECULATION`)
  so interpretation is never dressed up as settled fact.
- **Utilities** — a calculator, a flashlight (drives the frontlight to full), and a unit converter
  (length / mass / temperature / volume / speed).
- **Passive Wi-Fi/BLE toolkit** — Wi-Fi AP scanner, evil-twin / deauth-flood detection, PCAP
  capture to SD, EAPOL/PMKID capture with hashcat-22000 export, and a passive BLE scanner with
  device fingerprinting. See [the toolkit note](#about-the-wi-fible-toolkit) below.

---

## What's actually working

Honest status — what's been run on real hardware, versus what builds clean but hasn't been flashed yet.

| | Feature | Notes |
|---|---|---|
| ✅ | **CrossPoint reader core** | Inherited from upstream and running on the X4 Pro — EPUB/TXT/XTC, library, wireless, themes, OTA. Re-synced to upstream regularly. |
| ✅ | **Bible reader + Memory Work** | Shipped and in use on-device. Translations load from the SD card. |
| ✅ | **Bible Numbers** | Shipped (26.9.3). Study data lives in `/Bible/numbers/` on the SD card. |
| ✅ | **Calculator** | Shipped on-device. (An operator-on-display fix is built and waiting for the next release.) |
| 🚧 | **Flashlight + unit converter** | Built; green on host tests, simulator, and the device build. **Not yet flashed / tested on hardware.** |
| 🚧 | **Passive Wi-Fi/BLE scanners** | Released in 26.10.1; scans run on hardware and the UI is solid. BLE fingerprinting needs `signatures.json` on the SD card to label anything, and full RF/SD-write correctness isn't exhaustively verified yet. |
| 🚧 | **Active/transmit foundation** | The frame-builder + gated transmit primitives are present but **dormant** — compiled out of release builds (flag off), no UI wired to them, transmit path is a no-op. Nothing transmits. |
| ❌ | **Historical Calendar** | Easter computus + a 1611-style reading calendar — planned, not built. |

> [!TIP]
> `PLAN.md` is the living design doc — current state, decisions, and what's next, in far more detail
> than this README.

---

## Install

The X4 Pro checks **this** repo for updates (not upstream's), so once you're on CrossLight you stay on it.

| Method | When | How |
|---|---|---|
| **Wi-Fi OTA** | Already on CrossLight 26.9.2+ | Settings → Check for Updates. Installs the latest [release](https://github.com/flyboy-byte/crosslight/releases). |
| **SD card** | Coming from stock / another firmware | Drop `crosslight-<version>-x4pro.bin` on the SD card, then Settings → SD Card Firmware Update. |
| **Wired flash** | First install, or a partition-table change | `pio run -e x4pro -t upload` over USB (USB-A-to-C cable). |

> [!IMPORTANT]
> Back up your stock firmware before first flashing. Grab the matching `crosslight-<version>-x4pro.bin`
> from [Releases](https://github.com/flyboy-byte/crosslight/releases) — the `x4pro` tag matters, the
> updater refuses an image built for another board.

---

## How it works

CrossLight is layered on top of CrossPoint and the FreeInk SDK — the fork only adds the top band:

```
┌─ CrossLight (this fork) ───────────────────────────────┐
│  Bible reader · Memory Work · Bible Numbers            │
│  Utilities: calculator · flashlight · unit converter   │
│  Wi-Fi/BLE analysis (passive; active = dormant/gated)  │
├─ CrossPoint reader core (upstream, merged & tracked) ──┤
│  EPUB/TXT/XTC · library · wireless · themes · OTA      │
├─ freeink-sdk (HAL · display · radio · TLS · fonts) ────┤
└─ Xteink X4 Pro — ESP32-S3 · 16 MB flash · 8 MB PSRAM ──┘
```

New Home-reachable tools plug into one registry (`src/utilities/UtilityRegistry.cpp`) — a tile is one
include plus one line, so the upstream-hot `HomeActivity` never has to change. Feature data that would
bloat the firmware (Bible translations, number studies, BLE signatures) lives on the SD card, not in
flash.

---

## Build it yourself

<details>
<summary><b>Firmware build (PlatformIO)</b></summary>

<br>

```sh
git clone https://github.com/flyboy-byte/crosslight.git
cd crosslight
git submodule update --init --recursive   # pulls freeink-sdk
pio run -e x4pro                           # build
pio run -e x4pro -t upload                 # flash over USB
```

The release image is a plain `pio run -e x4pro`; the binary lands at
`.pio/build/x4pro/firmware.bin`. Version lives in `platformio.ini` under `[crosslight]`.

</details>

<details>
<summary><b>Desktop simulator (build UI without hardware)</b></summary>

<br>

CrossLight builds natively against the [CrossLight Simulator](https://github.com/flyboy-byte/crosslight-simulator)
(a fork of the CrossPoint simulator) and renders an X4 Pro window via SDL2. It does **not** emulate
the radio (Wi-Fi/BLE) or true e-ink timing — those need the real device.

```sh
sudo pacman -S sdl2 openssl          # or your distro's equivalent
pio run -e simulator_x4_pro          # config lives in gitignored platformio.local.ini
```

</details>

<details>
<summary><b>Host tests</b></summary>

<br>

Pure logic (Bible parsing, number-study loading, Wi-Fi/BLE frame parsing, conversions) is covered by
a GoogleTest suite that runs on the host:

```sh
cd test/build && cmake .. && cmake --build . -j4 && ctest
```

</details>

---

## About the Wi-Fi/BLE toolkit

This is personal security-research tooling for **my own hardware**, in the Hak5 / DEFCON spirit — the
same way an SDR or a Hak5 device is lawful to own and use on gear you control. The shipped tools are
**passive / receive-only** (scanning, fingerprinting, capture) and legally unambiguous. Any
active/transmit capability is gated two ways: compiled out of public release builds entirely, and
scoped to equipment I own — never shared, campus, or other people's networks. Signature/OUI data is
verified against primary sources, never fabricated.

---

## Repo layout

| Path | What |
|---|---|
| `src/activities/bible/` | Bible reader, Memory Work, Numbers UI |
| `src/bible/` | Bible data engines (SD-backed, host-tested) |
| `src/activities/utilities/` + `src/utilities/` | Utility tiles + the registry |
| `src/wifiaudit/`, `src/bleaudit/` | Passive Wi-Fi/BLE toolkit |
| `freeink-sdk/` | Upstream HAL/display/radio SDK (submodule) |
| `PLAN.md` | Living design doc — read this first |

## License & credit

MIT, inherited from [CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader). CrossLight
exists only because CrossPoint is open and hackable — all credit for the reader to the CrossPoint
community. If you're buying an Xteink device, consider a Developer Edition through
[crosspointreader.com](https://crosspointreader.com) to support upstream.
