# CLAUDE.md

CrossLight — a personal fork of CrossPoint ([crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader))
for the **Xteink X4 Pro**. (This file was upstream a symlink to `AGENTS.md`; CrossLight replaces it with
this pointer so the fork's own context loads too.)

## Read these, in order

1. **AGENTS.md** — upstream CrossPoint coding rules: HAL usage, memory discipline, build system,
   platform pitfalls. Still authoritative for *how* to write code in this tree.
2. **PLAN.md** — CrossLight's own architecture map, scope, decisions, and current state. Read before any
   CrossLight-specific work (the Bible app, simulator loop, Phase-2 tooling).

## Target overrides — CrossLight is ESP32-S3, not C3

`AGENTS.md` is written primarily around the **ESP32-C3** X4/X3: it calls the ~380 KB RAM its "primary
constraint," assumes a single 48 KB framebuffer, and no PSRAM. **CrossLight targets the Xteink X4 Pro
specifically:** ESP32-S3, 16 MB flash, **8 MB PSRAM**, GT911 capacitive touch, dual warm/cold
frontlight, native 1-bit SDMMC — build env `[env:x4pro]`. Where `AGENTS.md`'s C3 resource assumptions
conflict with X4 Pro facts, **the X4 Pro constraints win.** Still honor the memory-discipline
*principles* — they keep the firmware lean and the shared reader core also runs on the C3.

## Fork hygiene

Fork of an active upstream. `develop` = untouched upstream mirror (never commit); `crosslight` = main
line + default branch; topic branches off `crosslight`. Keep synced: periodically
`git fetch origin && git merge origin/develop` into `develop`, then merge into `crosslight`.

## Desktop dev loop

`[env:simulator_x4_pro]` (in gitignored `platformio.local.ini`) builds the firmware natively and renders
an X4 Pro SDL window via the CrossPoint Simulator — forked to `flyboy-byte/crosslight-simulator` so its
HAL tracks CrossLight. Lets UI be built/screenshotted without hardware. It does NOT emulate the radio
(BLE/WiFi sniffing) or true e-ink timing — those need the physical device. See PLAN.md.
