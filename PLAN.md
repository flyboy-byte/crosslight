# PLAN.md

Status: Bible reader Phase 1 MVP complete on the simulator (pagination, chapter/book-crossing page
turns, book/chapter picker, Esther 8:9 fixed, reading position persisted, bookmarks, verse-reference
jump — all verified in the simulator, 349 host tests pass); **last updated 2026-09-18. X4 Pro arrived
2026-09-18: stock backed up and verified (step 11), then CrossLight flashed and running on the device
(panel is UC8279). Bible opens in ~15 ms via the new chapter cache (was 10 s) and its menu now opens by
touch — see "First flash and first on-device session".** Use a USB-A-to-C cable, not C-to-C.

**Logan's request list (2026-09-21) — every item he asked for, with status. Keep this current:**

| # | Request | Status |
| --- | --- | --- |
| 1 | Fonts to SD to free flash for utilities | **Built, sim-verified, not yet on device.** `-DOMIT_FONTS` in `[env:x4pro]` (flash 83.8% → 58.6%). SD families `/.fonts/Noto Serif/` + `/.fonts/Noto Sans/` (12-18, from the built-in source TTFs, `--intervals builtin`). Under `OMIT_FONTS`: built-in sizes shrink to {14}; `getReaderFontId()` only ever returns Noto Serif 14 (a missing id rendered blank pages); boot maps a built-in family choice to the same-named SD family (`SdCardFontSystem::begin`); the Font Family list hides built-ins the SD families replace. Next device session: copy both families, delete the `NotoSansSD` test family, flash, verify |
| 2 | Local (PC) tool to prep any image as a wallpaper | **Built:** `scripts/make_wallpaper.py` (0.4s/image; sim sleep screen renders it pixel-identical to `--preview`). New 1-bit Sabaton ready for the card. Was: Planned. Pre-dither to **1-bit** 480×800 on the PC: this UC8279 unit renders sleep images 1-bit, and 4-gray input gets thresholded (posterized) rather than dithered — firmware only error-diffuses high-color BMPs. Also re-do the Sabaton `/sleep.bmp` this way |
| 3 | Wallpaper options from the file browser | **Built, sim-verified (all 3 actions + Delete hand-off), not yet on device.** `ChoiceActivity` (N-option modal) + `util/Wallpaper` (set → `/sleep.bmp`; add → `/.sleep/`, moving an existing `/sleep.bmp` in rather than deleting it); both switch Sleep Screen to Custom. Was: Planned. Long-press on an image → Set as sleep screen / Add to sleep rotation (`/.sleep/`) / Delete (long-press is delete-only today). The image viewer's "Set sleep cover" exists but is Confirm-only and its hint is hidden on touch boards — unreachable on the X4 Pro |
| 4 | Bible: download preset English translations over WiFi | **Built, sim-verified except the network transfer itself (device test).** Bible hub off Home (Continue/Select Book/Go to Verse/Search/Bookmarks/Translations); Translations screen (installed, then the 12 getBible English presets with license); `BibleDownloadActivity` (OTA-style: Wi-Fi picker, `.part` then rename, Back cancels, selects on success, silent restart on exit). Translation choice saved in `bible_state.json`; files at `/Bible/<ABBR>/<abbr>.json`. Was: Planned. Infra exists (`HttpDownloader`, WiFi picker, chapter cache rebuilds per file). Offer public-domain English versions only, show license before download (see translation-downloader scoping below) |
| 5 | Bible full-text search | **Built:** `searchCache()` — one linear pass over the chapter cache, case-insensitive, 100-hit cap, UTF-8-safe snippets; host run over full KJV 175ms (device time not measured yet). Results list jumps to the verse. Was: Planned; cheaper than scoped. Search the chapter cache (plain text, 4.25MB) instead of the JSON; optionally keep it in PSRAM for near-instant repeat searches. Measure the cold SD pass first |
| 6 | Bible: center tap sometimes doesn't open the menu | **Fixed, not yet on device:** the reader menu tap only accepts the center *ninth*; taps above/below it in the center column hit no zone. The Bible now takes the whole center column. Was: To investigate with the serial logger (did the tap register, where did it land vs. the center-third zone) |
| 7 | Lag when page-turn taps queue up | **Investigated, not changed:** render requests already coalesce (`ulTaskNotifyTake(pdTRUE)`), so N taps during a refresh cost one catch-up refresh, not N. Remaining idea: skip the grayscale AA pass when another turn is pending — touches panel-state subtleties, needs on-device iteration. Meanwhile Text Anti-Aliasing off is the lever. Was: To investigate: skip queued intermediate pages and render only the final one. Baseline: ~1.36s/turn, ~1.0s of it panel |
| 8 | Home screen: select with buttons, not only touch | Existing setting: Settings → Controls → Short Power Button Click → Confirm (or Home key tap → Confirm). Consider making it the X4 Pro default |
| 9 | WiFi not set up | No code needed: the flash erase removed stock's saved network; add it once in Settings → System → Wi-Fi Networks |
| 10 | (Found along the way) Bible text uses the UI font (Ubuntu 10), not the reader font/size | Noted; offer the reader font later if wanted |
| 12 | (Found along the way) WEB and other translations carry paragraph indents/double spaces | **Fixed:** verse whitespace normalized in cache and JSON paths (cache format v2 — old caches rebuild once) |
| 11 | Feature architecture: many utilities without hurting battery/speed | After the above: load-on-demand vs. resident, radio power management, and the partition change (+1.63MB) before the first radio feature |

**User data and updates:** all user data lives on SD under `/.crosspoint/` (settings, `wifi.json`,
recents, library index, per-book progress/bookmarks, Bible state, KOReader). `pio run -e x4pro -t upload`
writes only bootloader, partition table, otadata and the app — it never touches the SD card or erases
NVS (NVS holds only a display-detection flag). **Never run `erase-flash` again** unless deliberately
resetting; it was a one-time step for the stock → CrossLight switch. The 2026-09-18 commits on `crosslight` are local-only until pushed to `fork` (`git log fork/crosslight..crosslight`).

**Plan (decided 2026-09-10):** ship the *full* Bible build for the first on-device run, get it working
and documented on hardware, then use this doc as the guide for what to cut when a wireless/security
profile needs the flash. No build flavors or cuts pre-emptively — see "Build strategy" under Path
forward for the flash accounting (short version: cutting the Bible barely dents the wireless-flash
problem; the reader *profile* is the real lever). Next UI step is the Bible hub off Home (designed,
not built — see "Agreed next UI step"). Hardware-gated work waits on the X4 Pro leaving China; when it
arrives, start at the "Device arrival test plan".

The translation downloader is the one genuinely blocked feature — the simulator does not emulate the
radio, so its UI could be built but never exercised. Full-text search is an open question (perf can't
be measured on the simulator; see "Known limitations").

Personal fork of [crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader) for an
Xteink X4 Pro. Fork name **CrossLight**, repo `flyboy-byte/crosslight`. Remotes: `origin` = upstream,
`fork` = CrossLight. Branches: `develop` = untouched upstream mirror (never commit); `crosslight` =
personal main line + fork default branch; topic branches off `crosslight`, deleted after merge.

**Keep this fork current:** upstream is very active. Periodically `git fetch origin && git merge
origin/develop` into `develop`, then merge `develop` into `crosslight`. Small frequent catch-ups, not
one big drift-merge.

**Upstream sync log** (what landed, what conflicted, what broke the sim — so a future sync isn't
surprised by the same class of break):

- **2026-09-16** (`9e7baf2e..0b6bb004`, 35 commits): merged clean apart from three expected
  conflicts — our `HomeMenuItem::BIBLE` vs upstream's new `LIBRARY` (replacing `RECENTS`), the matching
  `HomeActivity.cpp` menu-item list/count/switch, and `test/CMakeLists.txt`'s `add_subdirectory` list —
  all resolved by keeping both sides (Bible stays a menu item alongside the new Library view). Notable
  upstream content: Library view (#3366), AboutActivity (#3563), timezone/DST settings (#3562), Arabic
  keyboard layout, X4 Pro/X4C display-detection fixes, SD SPI batching (#3501, relevant to the
  full-text-search benchmark below), a dropped-input-while-repainting fix, and absolute-plane
  `GrayscaleMode::Direct` for the SSD1677 sleep-cover path. Also hit one **upstream test bug**, not
  ours: `test/library_builder/stubs/HalStorage.h` uses `uint8_t`/`uint32_t`/`uint64_t` without
  `#include <cstdint>` — compiled by luck on whatever toolchain upstream CI uses, failed outright here.
  Fixed with the one-line include (not reported upstream yet). Firmware `x4pro` flash: 82.7% → 83.7%.
  Host tests: 223 → 349 passing. Simulator fork needed six new/updated stubs to relink (see its own
  "Local checkout moved" note above and its 2026-09-16 commit for the full list — `GrayscaleMode::Direct`,
  `AboutActivity`'s board/chip metadata reads, `HalStorage::usbDriveHostSuspended`,
  `HalClock::setTimezone`, `HalFile::modificationTime`).

## Path forward (two phases)

1. **Phase 1 (current focus): solid e-reader + Bible app.** CrossPoint as-is, plus the native Bible
   app. This is the whole scope until it works and is stable on the device.
2. **Phase 2 (later): broader custom firmware**, one capability at a time, measuring flash/heap after
   each (`scripts/firmware_size_history.py`, `ESP.getFreeHeap()`). Sequenced after Phase 1.
   - **"Plain networking" is mostly already built — corrected 2026-09-09.** Earlier drafts of this plan
     treated WiFi connect/HTTP client/file transfer as a Phase 2 item to build. It isn't: CrossPoint
     already ships all of it, in production use by OTA updates and the OPDS browser today —
     `HttpDownloader::fetchUrl()`/`downloadToFile()` (`src/network/HttpDownloader.h/.cpp`, built on
     `esp_http_client`, handles HTTPS CA verification, has a `ProgressCallback` + cancel-flag for
     large transfers, and heap pre-flight guards `MIN_TLS_FREE_HEAP`/`MIN_TLS_MAX_ALLOC` before opening
     a TLS connection) and `WifiSelectionActivity` + the `checkAndConnectWifi()`/`launchWifiSelection()`
     pattern every network feature reuses (see `OpdsBookBrowserActivity` for the reference
     implementation of both). **This is also exactly what the Bible translation downloader needs** —
     see the dedicated scoping section under "Bible data/feature shape" below. Net effect: that
     downloader is Phase-1-adjacent tooling reuse, not a Phase 2 prerequisite to build first. What
     Biscuit's tools actually need that isn't already here is raw 802.11 packet injection/promiscuous
     mode and BLE HID/central — see below.
   - **Flash is the binding constraint, not RAM — measured 2026-09-09.** A real `pio run -e x4pro` build
     is already at 82.4% flash (5.40MB/6.55MB of one OTA slot) but only 30.6% internal RAM (100KB/328KB)
     — the S3's 8MB PSRAM makes RAM comfortable even with WiFi/BLE stacks live (~50-100KB combined).
     Practical effect on sequencing: **budget and measure flash per Phase 2 tile before building it**,
     not heap. A tile with a large const data table (e.g. Flock OUI/IE signatures, if baked in rather
     than SD-loaded) costs more than one with equivalent logic but small static data.
   - **The one architecturally transferable idea from the e-ink firmware survey** (`docs/research/eink-
     firmware-app-models.md`, written 2026-09-09): KOReader's plugin self-registration seam (a plugin
     registers itself into a menu/dispatcher rather than being hand-wired into a central enum). Maps
     onto this repo's existing `HomeMenuItem`/`HomeActivity` index math and the noted-but-not-yet-built
     `App`/`AppRegistry` idea (see "Architecture notes" above, from `crosspoint-reader-apps`) — if Phase
     2 grows past a handful of home-menu entries, that's the point to build it, not before. Confirmed
     the survey's other conclusion is not actionable guidance so much as validation: CrossPoint's
     single-image/one-activity-at-a-time model already matches Plato's (the closest bare-metal
     comparison), so there's no different architecture to move toward here — just the registration seam.
   - **Biscuit-derived tools** (`rayrayrayyyym/biscuit`, MIT, but targets the C3 X4 — porting to S3 is
     real work, not a recompile). Its eight tiles split into: plain networking (already have it, above),
     genuinely dual-use offensive security (deauth, credential-capturing captive portal, AP cloning,
     BLE/USB HID injection), defensive/awareness (tracker detection, rogue-AP/camera sweep, MAC
     rotation, RF kill), comms (ESP-NOW mesh chat, anonymous file drop), and utilities/games (TOTP,
     password manager, cipher tools, calculator, chess, etc.). Offensive tiles are fine for authorized
     testing on own gear; the narrow carve-out is anything aimed at deceiving/attacking people or
     networks not yours. The tiles that need real new HAL work are the ones needing raw 802.11
     (monitor/injection mode) or BLE HID/central (`FREEINK_CAP_BLE_HID_HOST`, NimBLE — see freeink-sdk
     findings above) — utilities/games and comms (ESP-NOW) tiles are comparatively cheap since they sit
     on top of what's already enabled.
   - **Flock camera detector** (idea from `colonelpanichacks/flock-you`, MIT — *inspiration, not a
     direct port*). Passive 2.4GHz promiscuous sniff for Flock camera OUIs + IE fingerprint. Runs on
     the same ESP32-S3, needs no BLE, purely passive → cleanest fit (privacy/awareness, detects cameras
     watching you). Port the detection logic only (not the Flask/GPS wardriving dashboard). Keep OUI +
     IE signatures in an updatable SD file, not baked into flash — both for the "Flock changes behavior
     often" reason already noted and to keep this tile's flash cost near-zero per the budget note above.
     US/Canada only (that's where Flock is deployed). A dedicated scanner screen, never background
     (promiscuous mode monopolizes the radio, so it can't run while reading).
   - **Coupling stance:** OK to strip the fork back somewhat rather than keep upstream's tree pristine;
     deal with upstream-PR conflicts later if that ever happens. Note the tradeoff: stripping raises the
     conflict cost of the periodic upstream merges too, so strip conservatively (things upstream rarely
     touches) and eat conflicts when they land.
   - App flash budget: ~6.25MB per OTA slot (`partitions.csv`: `app0`/`app1` at `0x640000` each), not
     the full 16MB — and already 82.4% (5.40MB) spent before any Phase 2 tile is added (see above).
     Dropped: Bionic Reading / CrossInk typography. Not pursuing.

### Build strategy: full first, then cut — and what a cut actually buys (decided 2026-09-10)

The plan, in Logan's words: **ship the full build for the first on-device run, get it fully working and
documented, and let the documentation itself be the guide for what to cut** when a security/wireless
profile needs the room. Do *not* set up build flavors or cut anything pre-emptively — you can't budget
a trim against wireless tools that don't exist yet, and a cut made blind gets redone.

**The flash accounting that reframes this** (measured 2026-09-10, not estimated):

| Thing | Flash | Where |
| --- | --- | --- |
| Whole Bible app (all 9 `.o`: reader, 3 pickers/lists, stores, parser) | **~74KB** | image |
| — of which bookmarks | ~9KB | image |
| — of which verse jump | ~2.3KB | image |
| `kjv.json` (the 8.86MB translation) | **0** | SD card, not image |
| Free in the OTA slot after item 12 | **~1.09MB** | 5.41MB of 6.55MB used |

The counter-intuitive part, and the reason not to over-invest in a "cut-down Bible": **cutting the
Bible barely helps the wireless-flash problem.** Deleting the whole app reclaims ~74KB (~7% of current
free space); a partial trim (drop bookmarks/search/verse, keep reader + book picker) reclaims ~30-40KB.
Raw 802.11 monitor mode + a BLE stack + packet capture + Flock detection will be *hundreds* of KB to
over a megabyte combined — 74KB against that is a rounding error.

**Corrected 2026-09-18 by measuring (linker map of the `x4pro` build + a real `-DOMIT_FONTS` build) —
the paragraph after this table guessed wrong about where the space is:**

| Component (flash, from `firmware.map`) | KB | Cuttable? |
| --- | --- | --- |
| Built-in fonts (all in `main.cpp.o`): Noto Serif 939, Noto Sans 936, Ubuntu UI 225 | **2,102** | **Mostly, without losing a feature.** `-DOMIT_FONTS` (existing flag) keeps Noto Serif 14 + UI fonts, drops the rest: measured **5,490,294 → 3,842,962 B (−1.57MB, 83.8% → 58.6%)**. Dropped sizes can come back from SD as `.cpfont` (repo's own converter); SD-font render speed on this device not yet measured |
| EPUB engine (`lib/Epub`) + reader activities + expat | ~666 | Only in a no-reading build |
| I18n, 34 UI languages | 361 | English-only would save ~330KB, but `gen_i18n.py` has no language-subset option yet |
| All string literals, merged (map credits them to `Wire.cpp.o`) | 214 | Partly: compiling out `LOG_DBG` text |
| wolfSSL | 199 | **No** — it's the HTTPS/TLS stack (`FREEINK_NET_WOLFSSL=1`: OTA, OPDS, future Bible downloader), not just ContentProtection as said below |
| WiFi/IP stack (net80211, lwip, pp, wpa_supplicant, phy) | ~450 | No — radio features need it |
| Bible app (linker-level) | 35 | Not worth it |

Plus the partition lever: CrossPoint's layout spends 3.4MB on a SPIFFS partition it never mounts;
stock's layout (from the 2026-09-18 dump) uses 7.88MB app slots on this same hardware → **+1.63MB per
slot**, OTA kept. Fonts-to-SD + repartition together: **~1.06MB free → ~4.4MB free**, with no reading
feature removed. Costs: one USB flash for the new table, a `partitions.csv` diff vs upstream, and the
SD-font speed question.

**SD-font speed question answered (measured on the X4 Pro, 2026-09-18):** same Noto Sans source TTFs
converted to `.cpfont` (`fontconvert_sdcard.py --intervals builtin --sizes 12,14,16,18`, family
`NotoSansSD`, 2.05MB on SD in `/.fonts/`), same Mistborn chapter, size 16, 7 page turns each. Per page:
SD `prewarm` 27-31ms / total ~1360ms; built-in `prewarm` 28-31ms / total ~1375ms — **no measurable
difference**. The per-chapter SD cost (advance table + kern classes) is tens of ms at chapter open.
Logan couldn't tell them apart. Not measured: a cold chapter re-layout with an SD font. A PSRAM-resident
font mode (`SdCardFont` has none — it's built for the C3's RAM) is therefore not needed.
Side finding: of ~1.36s per page turn, ~1.0s is panel (687ms page + 330ms grayscale pass) and ~270ms is
the grayscale anti-aliasing CPU work — **Settings → Reader → Text Anti-Aliasing off** should bring page
turns to roughly 0.8s (inferred from the breakdown, not yet measured).

~~**Where the megabytes actually are is the reader *profile*** — EPUB parsing, the font engine, OPDS,
dictionaries, the wolfSSL that `ContentProtection` pulls in. A security-focused build's real lever is
stripping *that*, not trimming the Bible. So the eventual build split is "reading device vs. red-team
device," and in the red-team build a **cut-down Bible is a ~40KB nicety you can keep**, not the thing
that makes room.~~ (superseded by the measured table above.) When it's time, the KOReader-style
registration seam (noted above) is where tiles get gated in/out; that's the seam to build, once there are
tiles to gate.

## Decisions made

- **Base firmware: CrossPoint**, not CrossInk (typography fork) or CrossPlay (apps/games fork).
- **Bible app is an isolated module** — own app/activity/data tree, not woven into `lib/Epub/` or
  reader internals. Keeps upstream merges cheap; door open to PR a generically useful piece later.
- **OpenBible2 is a reference, not a porting target.** Kotlin/Compose/Android, Apache-2.0. No JVM on the
  S3 — native C++ reimplementation of the idea, not a port. Carry Apache-2.0 attribution if code (not
  just concept) is lifted.
- **DRM: deferred until further notice.** Adobe/Kindle aren't goals. Note: the SDK ships a
  `ContentProtection` module (LCP-shaped — `ProtectedBook`/`Credential`/`Rights`/wolfSSL crypto), so if
  DRM is ever un-deferred it's not from-scratch. Still deferred.
- **Manga: untested, not decided.** Test one legal sample on hardware before any comic work.
- **GPL-3 is fine to ship under.** `wolfssl/Arduino-wolfSSL@5.7.2` (statically linked) is GPL-3.0, which
  makes the combined firmware GPL-3. wolfSSL also underpins the SDK's `ContentProtection` crypto. Flag
  the divergence if anything ever goes back to upstream (their `LICENSE` says MIT).
- No dedicated open-source e-ink Bible app exists to build on (checked KOReader plugins + the CrossPoint
  fork ecosystem). The native rewrite is the path.

## Architecture notes (read directly from source)

- `ActivityManager` (`src/activities/ActivityManager.h`) owns a stack of `Activity`-derived screens, one
  shared render task/mutex — see `docs/activity-manager.md`. Replaced an older per-activity-task model.
- New screens use FreeInkUI (`docs/contributing/touch-and-ui.md`): `UiListActivity` (single list),
  `UiTabListActivity` (tabbed), `UiAppHost` (custom layout). One hosting stack.
- **Menu registration isn't declarative** in upstream: hand-maintained `enum class HomeMenuItem` +
  index math in `HomeActivity`. But `zakerytclarke/crosspoint-reader-apps` (MIT) has a real declarative
  `App`/`AppRegistry`; borrow its *shape* but reimplement with function pointers, not its `std::function`
  (AGENTS.md bans `std::function` near the render path). Diff its `ActivityManager` vs upstream first.
- Settings persist as JSON on SD via `HalStorage`/`PersistableStore` (`/.crosspoint/`, SPIFFS not
  mounted). **Decided:** Bible *settings* get their own `PersistableStore` subclass under `/.crosspoint/`;
  Bible *data* (translations) lives as ordinary user content at a `/Bible/` folder on SD root, NOT under
  the cache dir. No single fixed "books" folder exists (browser is freeform; OPDS uses
  `SETTINGS.opdsDownloadFolder` or SD root).
- Storage access only through `HalStorage`/`HalFile`, never raw SdFat (SPI-state crash — see AGENTS.md).
- X4 Pro build: `[env:x4pro]` — ESP32-S3, `dio_opi` PSRAM, native 1-bit SDMMC. `pio run -e x4pro`.

### freeink-sdk findings (submodule, now checked out — `git submodule update --init`)

- **BLE exists and the chip supports it.** ESP32-C3/S3 have Bluetooth *Low Energy* (no Classic). SDK has
  `libs/network/BleKeyboardHost` — a NimBLE central/HID-host, capability-gated behind
  `FREEINK_CAP_BLE_HID_HOST` (pulls in NimBLE-Arduino, else links stubs). So BLE-dependent Biscuit apps
  are *not* a from-scratch HAL: enable the NimBLE cap and write scan/advertise code against
  NimBLE-Arduino, using BleKeyboardHost's integration (spinlock ring, fixed-capacity, host-task) as the
  reference pattern. (Corrects an earlier note claiming the SDK had no BLE.)
- **FreeInkBook** (`libs/book/FreeInkBook`) is a freestanding C++17 book engine (no Arduino/ESP-IDF),
  arena/bump allocator sized up front (in PSRAM on device), high-water tracking. Host-buildable.
- **X4 Pro hardware** (`freeink-sdk/docs/xteink-x4pro-support.md`, a real bench bring-up doc): ESP32-S3,
  16MB flash / 8MB PSRAM, 800×480 B/W, GT911 capacitive touch, dual warm/cold frontlight. Board profile
  `BoardConfig::XTEINK_X4_PRO`. Panel controller **varies by batch** — SSD1677 (original) or UC8179
  (newer) — auto-detected at boot; both drivers + touch + frontlight caps auto-enable. Display confirmed
  working on hardware with the stock X4 waveform.

### Desktop dev loop — CrossPoint Simulator (the real answer)

A full desktop simulator exists: `crosspoint-reader/crosspoint-simulator` (MIT, separate repo). It
compiles the firmware natively and renders an 800×480 X4 Pro **SDL2 window** — `SIMULATOR_DEVICE_X4_PRO`
profile with touch/swipe, capacitive Home key, RTC, display inversion, frontlight state, a simulated SD
filesystem, host-backed networking, scripted TAP/SWIPE/HOME input, screenshot capture, and simulated
heap limits. Far more useful than dumping framebuffers to PNG. (An earlier note here wrongly said no
simulator existed — corrected.) Not cycle-accurate: no real e-ink waveform/ghosting timing, PSRAM
behavior, or power sequencing — and **no radio**, so BLE and WiFi promiscuous sniffing (Biscuit/Flock)
can't be exercised here; those need hardware.

**CrossLight integration (done):** `[env:simulator_x4_pro]` in gitignored `platformio.local.ini` (Linux
flags; `pio run -e simulator_x4_pro`). Points at a fork **`flyboy-byte/crosslight-simulator`** rather
than upstream, because the simulator (separate repo, v1.0.0) lags fast upstream firmware: develop's tip
(`1f3d7458`) added `HalDisplay::supportsAsyncGrayscaleBase()`, which the stock simulator lacked, so
`GfxRenderer` failed to link. The fork stubs it (`return false;`, no async e-ink in sim). Expect a small
stub occasionally whenever upstream adds a HAL method — the cost of tracking develop's tip on the sim.
Two host-toolchain build fixes also live in the env: `-Dmemcpy_P=memcpy` (AnimatedGIF) and `-std=gnu17`
(QRCode's `typedef ... bool` collides with this GCC's default C23).

**Local checkout moved 2026-09-11:** the fork now lives at `crosslight/simulator/` (nested inside this
repo, still its own pushable git repo via its own `.git`; gitignored here so the firmware repo never
tracks its files). `platformio.local.ini` points at it with `symlink://simulator` instead of a git URL
— edits are live, no push-then-`pio pkg install` refetch cycle. (Superseded: it used to live as a
sibling checkout at `~/projects/crosslight-simulator`; if you see that path referenced anywhere old,
it's stale.)

### Scripted simulator QA (how every UI claim in this doc was verified)

The simulator can be driven headlessly, which is what makes "verified in the simulator" mean something
repeatable rather than "I clicked around once". Two env vars, both `;`-separated `<ms>:<what>` lists:

- `CROSSPOINT_SIM_INPUT_SCRIPT` — `<ms>:<ACTION>[:<detail>]` at wall-clock ms. Actions (re-read from the
  sim's `HalGPIO.cpp` 2026-09-21 — the earlier list here was incomplete): `ESCAPE`/`BACK`,
  `RETURN`/`ENTER`/`CONFIRM`, `LEFT`, `RIGHT`, `UP`, `DOWN`, `POWER` (buttons take an optional hold in ms,
  e.g. `3000:RETURN:1200`), `HOME[:holdMs]`, `SLEEP`, `QUIT`, and **touch**: `TAP:x,y[,durationMs]`
  (logical pixels; a long-press is a tap with duration ≥500) and `SWIPE:x1,y1,x2,y2[,durationMs]`. So
  touch-only flows (long-press menus, the Bible center tap) are scriptable.
- `CROSSPOINT_SIM_SCREENSHOTS` — `<ms>:<path>.bmp`. Convert with PIL to view.

A real example — the bookmark toggle run from item 11 (Home → Bible → menu → toggle → screenshot):

```
CROSSPOINT_SIM_INPUT_SCRIPT="1000:DOWN;1200:DOWN;1400:DOWN;1600:DOWN;1800:RETURN;2800:RETURN;3800:DOWN;4000:DOWN;4200:RETURN;5000:QUIT" \
CROSSPOINT_SIM_SCREENSHOTS="4800:/tmp/star.bmp" \
timeout 20 .pio/build/simulator_x4_pro/program
```

**Two traps worth knowing before you debug a scripted run**, both of which cost real time already:

1. **Wipe the state files between runs.** `fs_/.crosspoint/bible_state.json` and
   `bible_bookmarks.json` persist across runs, so a "wrong" chapter or a pre-checked toggle is usually
   last run's state, not a bug. A previous session lost time misdiagnosing exactly this as input-timing
   flakiness — the giveaway was that *slower* timings made results worse, not better. Screenshot after
   every single keypress when a navigation lands somewhere unexpected; guessing at timing is the wrong
   move.
2. **After renaming or moving the repo, `rm -rf .pio/build/simulator_x4_pro` and `build/test`** —
   CMake caches absolute paths and fails with a "current CMakeCache.txt directory is different" error.

`fs_/` is the simulator's SD sandbox (gitignored). `fs_/Bible/KJV/kjv.json` is the real 8.9MB getBible
KJV used for all Bible testing.

## Next steps

**Done:**
1. ~~Inspect `freeink-sdk/`~~ — done (findings above).
2. ~~Prove the desktop dev loop~~ — done: CrossPoint Simulator wired in, X4 Pro window renders and
   screenshots (see "Desktop dev loop" below).
3. ~~Smallest possible Bible menu entry~~ — done: `BibleActivity` reachable from Home, renders real
   parsed scripture (not placeholder text).
4. ~~Sketch the Bible data format~~ — superseded by actually building it: `BibleChapterLoader`
   (`src/bible/`) streams a getBible-format JSON file from SD via the shared `StreamingJsonParser` (no
   DOM-load of the ~9MB file) and extracts one book/chapter's verses. Verified against a real fetched
   KJV file in the simulator's SD sandbox (`fs_/Bible/KJV/kjv.json`, gitignored test data) — correct
   verse text and numbering for all of Genesis 1, confirmed against an independent Python-side parse of
   the same file.
5. Fixed a real, data-confirmed bug in the shared `StreamingJsonParser` while building this: it passed
   `\uXXXX` Unicode escapes through as 6 literal characters instead of decoding them (RFC 8259 requires
   decoding). Verified against live KJV data that this wasn't hypothetical — 2,380 of 31,102 verses
   (7.6%) contain `’`/`–` etc. directly in verse text (e.g. Genesis 3:20's "Adam's" with a
   curly apostrophe). Added proper UTF-8 encoding with surrogate-pair support, plus 4 new host gtest
   cases (ASCII escape, 2-byte curly-quote matching the real KJV case, a surrogate-pair emoji, and a
   chunked-mid-escape split) — all 195 host tests pass, no regressions in the one other real consumer
   (`ReleaseJsonParser`/GitHub release parsing, which contains no `\u` escapes to begin with).
6. Fixed two rendering bugs found by actually running the screen in the simulator (not caught by
   compiling alone): text drawing past the bottom of the screen (`GfxRenderer::getOrientedViewableTRBL`
   returns bezel *insets*, not absolute coordinates — a real mix-up worth remembering) and past the
   right edge (now uses `GfxRenderer::truncatedText`, the existing UTF-8-safe helper other activities
   already use, rather than hand-rolling truncation).
7. Real book/chapter picker: `BibleActivity`'s Confirm button opens `BibleBookSelectionActivity` ->
   `BibleChapterSelectionActivity` (plain `UiListActivity` subclasses, modeled directly on
   `EpubReaderChapterSelectionActivity`'s `startActivityForResult`/`ActivityResult` pattern — no need to
   touch the hand-rolled `HomeMenuItem` pattern for this, since the Bible module doesn't need its own
   home sub-menu, just in-activity pickers). `BibleChapterLoader::loadBookIndex()` adds a second SAX scan
   (book names + chapter counts, no verse text) over the same getBible JSON. Found and fixed a real bug
   while verifying this in the simulator: `loadChapter()` never cleared its output vector, so switching
   books/chapters silently kept showing the *previous* chapter's text while the header updated correctly
   — caught by actually reading the rendered verse text against real KJV data, not just checking the
   title line or that it compiled. Verified end-to-end (Home -> Bible -> book picker -> chapter picker ->
   Leviticus 5, correct text) via a scripted `CROSSPOINT_SIM_INPUT_SCRIPT` run with screenshots; all 195
   host tests still pass.

8. Rebuilt the reading surface on `ReaderActivity` (`BibleReaderActivity`, replacing the raw-`Activity`
   `BibleActivity`) — the same base EPUB/TXT/XTC subclass. Picked up, for free: page-turn button/touch
   handling and the e-ink refresh-batching policy. Built new: real word-wrap + pagination via
   `GfxRenderer::wrappedText` (verses flattened into lines, sliced into screen-sized pages once per
   chapter load), and paging across chapter/book boundaries in both directions so the whole Bible reads
   as one continuous book (verified: Genesis 1 -> Genesis 2 forward, and Genesis 2 page 1 -> Genesis 1's
   *last* page backward, not its first). Deliberately not wired into `ReaderActivity`'s file-book
   plumbing (`APP_STATE.openEpubPath`/`RecentBooksStore`/`EndOfBookOptions` all assume a path
   `ReaderActivity::create()` can re-dispatch by extension, which would misdispatch a `.json` translation
   file to the EPUB branch) — `onEnter`/`onExit` are overridden in full instead. `isAtEndOfBook()` is
   always `false`; Revelation's last page just stops. Found and fixed a real off-by-one while verifying
   in the simulator: the page-builder's content-top offset didn't match the renderer's actual first
   content line by one `lineH`, so a full page's last line silently overflowed the bottom edge — caught
   by `GfxRenderer`'s "Outside range" log during a scripted page-through-Genesis-1 run, not by compiling.
   All 195 host tests still pass.

9. Fixed the Esther 8:9 token-overflow gap: `StreamingJsonParser::TOKEN_BUF_SIZE` was 512, and its
   overflow path (`appendToken`) *drops* (not truncates) any string that hits the cap — silently, no
   error. Checked the data before touching the constant: scanned all 66 books of the actual
   `kjv.json` and confirmed Esther 8:9 (530 UTF-8 *bytes* — Python `len()`/codepoint-counting says 528,
   which undercounts the curly apostrophe in "king's") is the *only* verse in the whole KJV over 511
   bytes — not a stale/guessed claim. `StreamingJsonParser` has exactly one other consumer
   (`ReleaseJsonParser`, for OTA release JSON), and it copies out of `tokenBuf` via a bounds-checked
   `safeCopy` into its own fixed buffers regardless of `tokenBuf`'s size, so growing the shared
   constant doesn't weaken it. Bumped `TOKEN_BUF_SIZE` to 600 for headroom. Added a new
   `BibleChapterLoaderTest` host suite (fixture: Esther 8 sliced from the real `kjv.json`) asserting
   verse 9 loads at its real length and exact start/end text — the first version of that assertion used
   the wrong (codepoint) byte count and caught its own mistake by failing. All 198 host tests pass.
10. Persisted Bible reading position: new `BibleReadingStateStore` (`src/bible/`), a dedicated
    `PersistableStore<T>` at `/.crosspoint/bible_state.json` — deliberately *not* added to
    `CrossPointState`, keeping the same no-shared-state stance as item 8. `BibleReaderActivity` loads
    it once in `onEnter()` (resolves the saved book name back to an index against the freshly-scanned
    `books` list, clamps chapter/page against the real chapter count and page count in case the saved
    values no longer fit), and persists on every position change: intra-chapter page turns, and
    chapter/book loads (covers both boundary-crossing page turns and picker jumps, since both route
    through `loadCurrentChapter()`). Reopening the app now resumes exactly where you left off instead
    of always opening on Genesis 1.
11. Bookmarks (2026-09-10). New `BibleBookmarkStore` (`src/bible/`), a third dedicated
    `PersistableStore<T>` at `/.crosspoint/bible_bookmarks.json`, holding `{book, chapter, page}`
    newest-first and capped at `MAX_BOOKMARKS = 100` so the JSON can't outgrow one ArduinoJson
    document. Same no-shared-state stance as items 8 and 10: it deliberately does *not* reuse the
    shared file-book bookmark plumbing, which keys bookmarks by EPUB spine index / xpath and has no
    meaning for a getBible JSON file.

    **The UI decision worth knowing before touching this:** the reader's four buttons were already
    fully bound (Back / Select / previous page / next page), so there was no binding free for
    bookmarks. Confirm now opens a new `BibleMenuActivity` (Select Book / Bookmarks / Toggle Bookmark)
    instead of jumping straight to the book picker — "Select Book" is the first row, so that path is
    one extra press, and any *future* Bible feature (search, translations) has somewhere to land
    without another button fight. `BibleBookmarkListActivity` is the picker; it returns a new
    `BibleBookmarkResult`. Deletion deliberately lives on the menu as a toggle against the current
    location rather than as a long-press on a list row, so it stays reachable without touch.

    Bookmarks store a *page* index, not just a chapter, so they resolve back to the exact screen. That
    page number is pagination-dependent (a font-size or orientation change renumbers it), so `goTo()`
    clamps it against the freshly-built page count — the same clamp `onEnter()` does, and the reason
    the bookmark list shows `p4` as a disambiguator rather than a promise. Current page being
    bookmarked is shown by a `*` appended to the title row: deliberately ASCII, not a star glyph or
    icon bitmap, so it renders identically under every bundled font and in the simulator (it is the
    only on-screen confirmation a toggle took effect).

    Verified in the simulator by scripted run, clean-slate each time: menu renders with a live
    bookmark count and a switch reflecting current state; toggle-on writes the JSON and shows the `*`;
    two bookmarks in one chapter disambiguate as `p1`/`p4`; selecting one jumps to that exact page;
    toggle-off removes it from the JSON and clears the `*`. All 198 host tests still pass.

    Cost on the real target (`pio run -e x4pro`, measured not estimated): flash 82.4% → **82.5%**
    (5,409,174 B of 6,553,600), RAM unchanged at 30.6%. So two Activities plus a store ran ~9KB of
    flash — a useful unit rate for budgeting the remaining Phase 1/2 tiles against the OTA slot.
12. Verse-reference jump (2026-09-10). New "Go to Verse" row on `BibleMenuActivity` opens
    `KeyboardEntryActivity` (the *existing* keyboard, already linked for WiFi/OPDS — this is why the
    feature is nearly flash-free) and parses free text against the loaded book list. The parser lives
    in its own TU, `src/bible/BibleReference.{h,cpp}`, as pure logic with a host unit test
    (`test/bible_reference/`, 15 cases): normalization keeps only `[a-z0-9:]` so spacing and
    punctuation are irrelevant (`"1 Jn." == "1john"`), book match is exact-first then *prefix*
    (`gen`/`ps`/`matt`/`rev` all resolve; contracted forms like `Jn` deliberately do not, and that's a
    pinned test), and a trailing digit run is unambiguously the chapter because book names never end in
    a digit (`"1john2"` → 1 John ch.2). `buildPages()` now also records `versePages[]` — the page each
    verse *starts* on, parallel to `verses[]` — so a reference with a verse (`John 3:16`) lands on the
    right page, not just the chapter's first. An unparseable entry reopens the keyboard with the text
    intact (no toast facility exists); Cancel is the escape, so it can't loop. Flash 82.5% → **82.6%**
    (5,411,502 B) — ~2.3KB, as predicted, because no new keyboard was added.

    The first draft of the parser's own test asserted `"1 Jn."` should parse, which contradicts the
    prefix-only rule stated in the header — the test caught the contradiction by failing, and the fix
    was to correct the test (and pin the real behavior), not the code. [[verify-dont-assume]] in
    practice.

**Agreed next UI step — Bible hub off Home (designed 2026-09-10, not yet built):**

Logan's structure: **the Home "Bible" entry should open a hub screen — Continue Reading / Select Book /
Go to Verse / Bookmarks — rather than dropping straight into the reader.** Today Home → Bible enters
`BibleReaderActivity` directly (resumes at saved position) and Confirm-in-reader opens `BibleMenuActivity`.
The hub makes those actions the Bible landing instead of a while-reading afterthought.

- **Keep resume fast.** The common case is "open Bible, keep reading," which is one press today. A
  pure hub-first design makes that three presses, which is the clunk regression to avoid. Mitigation:
  make **"Continue Reading" the pre-selected first row** (label it with the saved position, e.g.
  "Continue — John 3"), so resuming is Bible → Confirm, same press count as now.
- **Cheapest implementation, reusing everything:** give `BibleReaderActivity` an optional *initial
  action* (`Resume` default / `BookPicker` / `VerseJump` / `Bookmarks`); the hub is a thin
  `UiListActivity` whose rows open the reader with the matching action, and the reader fires it in
  `onEnter()` after loading (it already has `openBookPicker`/`openVerseJump`/`openBookmarkList`). Keep
  the in-reader Confirm menu (`BibleMenuActivity`) as-is for while-reading — it carries Toggle Bookmark,
  which is inherently a reader-context action the hub can't have.
- **Flash:** the hub is a new `UiListActivity` (~12KB by the item-11 unit rate). Worth it for the
  structure Logan wants; cuttable later, and the first thing a red-team-profile build would drop back to
  the current reader-first flow. Consider folding the hub and `BibleMenuActivity` into one dual-mode
  activity if the ~12KB matters — they differ only in the first row (Continue Reading vs. Toggle Bookmark).

**Known limitations, not yet fixed (flagged, not hidden):**
- Bookmarks are position-only — no label, note, or verse-text preview, so the picker shows
  `Genesis 1  p4` and nothing about what's on that page. Page-number disambiguation is the stopgap.
- Verse-reference jump (item 12) covers the *known-reference* case ("take me to John 3:16"). Full-text
  content search — "find the verse that says X" — is still absent, and is a candidate for a Bible-leaning
  build rather than a definite. Perf is the blocker and the simulator can't measure it. Fully scoped
  under "Bible full-text search — scoping" below (approaches, match semantics, UI reuse, and the
  hardware micro-benchmark that gates the whole thing). Note on verse-ref itself: on an e-ink keyboard,
  typing a reference may well be slower than scroll-picking book+chapter — kept through the first
  hardware run specifically to feel that out, a drop candidate in the post-hardware cut pass if so.
- Still only the hardcoded KJV path (`/Bible/KJV/kjv.json`) — no translation selection or downloader.
  **Priority note (2026-09-09):** low personal priority — one translation is enough for actual use — but
  raised anyway because no other open-source e-ink Bible app exists to defer this to (confirmed, see
  "Decisions made" above): if this project doesn't add other translations, nothing will. Scoped in
  detail (not yet built) under "Bible translation downloader" below, specifically because most of its
  prerequisite infrastructure turned out to already exist in CrossPoint.

**Device arrival test plan — X4 Pro arrived 2026-09-18, ordered direct from xteink.com (not USB-locked,
per the site's own "unlocked firmware" policy and confirmed by `docs/fix-bricked-xteink.md` — normal USB
flashing applies, the SPI-clip procedure in that doc is last-resort only and is written for the older
ESP32-C3 X4/X3 besides):**

**Stock firmware baseline — captured 2026-09-18, screenshots in
`docs/images/stock-firmware-baseline/` (13 photos).** XTEink X4 Pro, firmware `XT V7.2.4`. Top menu:
Read / All Files / USB Mode / Cloud Sync / Settings. Settings: account (Logged In/Bound), Upgrade (OTA),
Network (WiFi 2.4G, saved networks), Bluetooth, Language, Time (manual + NTP + timezone), Startup
Password, System Font (built-in + cloud "Get Fonts" store), About Device. Cloud Sync pulls wallpaper
(`.xth` packages) and even books (saw it grab a Project Gutenberg EPUB) from Xteink's own cloud service.
USB Mode exposes the microSD as a mass-storage drive.

**Feature comparison against CrossLight/CrossPoint (verified against source, not assumed):**

| Feature | Stock | CrossLight/CrossPoint |
| --- | --- | --- |
| WiFi | Yes | Yes — `WifiSelectionActivity`, `HttpDownloader`, production-used by OTA/OPDS |
| USB Mode (mass storage) | Yes | Yes — `FREEINK_CAP_USB_MSC=1` set in the `x4pro` env |
| OTA/firmware upgrade | Yes | Yes |
| Timezone/DST | Yes | Yes — `HalClock::setTimezone`, upstream #3562 |
| Custom fonts | Yes, via cloud "Get Fonts" store | Yes, differently — SD-card-dropped fonts (`OMIT_FONTS`/`builtinFonts`), no cloud store |
| Wallpaper / sleep-screen customization | Yes, via cloud `.xth` packages | **Yes, and arguably more flexible** — `CrossPointSettings::sleepScreen` supports Dark/Light/Custom/Cover/Cover-Custom/Blank/Quick-Resume/Transparent-Custom; `BmpViewerActivity` sets any viewed `.bmp` as the custom sleep screen directly on-device |
| Getting content on wirelessly | Xteink's proprietary cloud account | No cloud account (by design, offline-first) — but the device hosts its own file-transfer web UI (AP-mode hotspot w/ QR, or STA/home WiFi), plus WebDAV, Calibre wireless connect, and an OPDS browser with saved servers |
| About Device | Yes | Yes — `AboutActivity`, upstream #3563 |
| Bluetooth | Yes (settings toggle shown) | **Gap** — BLE stack exists in freeink-sdk (`BleKeyboardHost`/NimBLE) but `FREEINK_CAP_BLE_HID_HOST` is not set in any `platformio.ini` env, including `x4pro`. Not a priority (Logan doesn't need it). |
| Startup Password / lock screen | Yes | **Gap, not yet built.** No PIN/lock activity exists. Buildable without new subsystems though: `KeyboardEntryActivity` already exists (reused for WiFi passwords and the Bible verse-jump feature) for input, `CrossPointSettings`/`SettingsList.h`/`PersistableStore` already exist for the toggle+stored PIN, and `SleepActivity`'s wake-intercept is the pattern for gating boot/wake before `ActivityManager` reaches Home. Estimated similar size to the verse-jump feature (a few KB flash, roughly an afternoon), not a big lift — low personal priority per Logan ("isn't very important depending on how hard it is to add"), candidate for Phase 2 if it starts to matter. |

Net: Phase 1 as built already matches or exceeds stock on reading, WiFi, OTA, USB transfer, fonts, and
wallpaper. The only real gaps are BLE (declined, not needed) and startup password (deferred, cheap to
add later if wanted).

**Stock backup — mandatory before any USB flash, do this first:**

11. ~~Dump a full stock-flash backup over USB before flashing anything else.~~ — **DONE 2026-09-18,
    verified.** No public X4 Pro (ESP32-S3) stock dump exists online the way the older C3 X4/X3 backup
    does (`docs/fix-bricked-xteink.md`), so it had to come from this unit.

    **Result:** `stock_x4pro_backup.bin` in the repo root (gitignored as `/stock_x4pro_backup.bin*` —
    it holds NVS, i.e. saved WiFi passwords and the Xteink account token; never commit or share it,
    and it's only valid for *this* unit). Move a copy off-device. 16,777,216 bytes,
    MD5 `2ace56c58de8425fc3c406336c995110`,
    SHA256 `e2c81fcf83f62c675573429c52144e781bed9720d4681b889cf1a163dca444cc`.
    Verified two independent ways: the whole-image MD5 computed *on the chip* (`flash_md5sum` over all
    16MB) matched the file's, and every app/bootloader image inside passes esptool's own checksum +
    SHA256 validation (see "Stock image anatomy" below).

    **How to re-dump (tested):** `~/.platformio/penv/bin/python3 scripts/x4pro_flash_backup.py`
    (resumable, ~3.5 min). **How to revert to stock:**

    ```bash
    ~/.platformio/penv/bin/python3 ~/.platformio/packages/tool-esptoolpy/esptool.py \
      --chip esp32s3 -p /dev/ttyACM0 write-flash 0x0 stock_x4pro_backup.bin
    ```

    That rewrites bootloader, partition table, NVS, otadata and both app slots back to byte-identical
    stock. Not yet actually exercised — only the read path has been tested on hardware.

**What the dump took to get working (2026-09-18) — read before touching USB on this device again:**

| Attempt | Result | Actual cause |
| --- | --- | --- |
| Magnetic pogo adapter + USB-C-to-C cable | Never enumerates: `device descriptor read/64, error -71`, `unable to enumerate`; once took the whole xHCI controller down (`HC died`), survived a reboot | C-to-C path through the magnetic adapter. Unverified *why* (CC negotiation through the adapter is the leading guess, INFERRED). The pogo adapter is the device's only port, so "use another cable" isn't an option on the device side |
| Same adapter + **USB-A-to-C** cable | Enumerates immediately as `303a:1001 Espressif USB JTAG/serial debug unit` → `/dev/ttyACM0` | This is the working setup. Logan is in `uucp`, no sudo needed |
| `pio pkg exec -p tool-esptoolpy -- esptool.py ...` | `ModuleNotFoundError: rich_click` | `pio pkg exec` runs the script with a Python that lacks esptool's deps. Call PlatformIO's own Python directly: `~/.platformio/penv/bin/python3 ~/.platformio/packages/tool-esptoolpy/esptool.py` |
| Single 16MB `read-flash`, then 1MB/4MB/512KB chunked retries, then lower baud | Always died with `Packet content transfer stopped`, looked random | **Not** the link. Baud rate is ignored on the S3's native USB-Serial/JTAG (a 64KB read at "115200" ran 1344 kbit/s), so lowering it never did anything. See next row |
| Per-sector probe of 0x260000–0x26F000 | Exactly one sector, **0x267000**, fails every time; on-chip MD5 of it works fine; reading it with 1024-byte packets works and matches | **esptool stub bug, data-dependent.** That sector has 57×`0xC0` + 5×`0xDB`; SLIP-escaped it's 4096+62+2 = **4160 bytes = 65×64** — an exact multiple of the USB full-speed packet size. A frame ending exactly on a 64-byte boundary never completes (missing short/zero-length packet is the likely mechanism, INFERRED). Predicted rate for dense data ≈1/64 sectors ≈22% of 64KB blocks; observed 24 retries over the dump, matching. Blank `0xFF` regions can't trigger it |

The fix lives in `scripts/x4pro_flash_backup.py`: stays in download mode for the whole run (so the stock
app can never boot between blocks and change flash mid-dump — the earlier chunked attempts used
`--after hard-reset` and could have produced an inconsistent image), reads 64KB blocks, and retries any
failed block with smaller packet sizes (1024, 1000, 256, …) to move the frame boundary. Also checked
first that this wasn't intentional read protection: `esptool get-security-info` reports Secure Boot
**disabled**, Flash Encryption **disabled**, no eFuse keys. Chip: ESP32-S3 (QFN56) rev v0.2, 8MB
embedded PSRAM (AP_3v3), MAC `7c:0c:5f:41:8e:50`.

Not reported upstream yet (esptool / esp-flasher-stub). Writing firmware *to* the device is the other
direction and shouldn't hit it (INFERRED — confirm on the first `pio run -e x4pro -t upload`).

**Stock image anatomy (parsed from the dump, 2026-09-18):**

| Partition | Type | Offset | Size | Notes |
| --- | --- | --- | --- | --- |
| nvs | data/nvs | 0x9000 | 20KB | settings, WiFi creds, account token |
| otadata | data/ota | 0xe000 | 8KB | seq 1 → app0, seq 2 → app1: **boots app1** |
| app0 | app/ota_0 | 0x10000 | 7.88MB | `xteink_app` **7.2.4**, built 2026-08-14, checksum + SHA256 valid |
| app1 | app/ota_1 | 0x7f0000 | 7.88MB | `xteink_app` **7.5.10**, built 2026-09-10, checksum + SHA256 valid |
| spiffs | data/spiffs | 0xfd0000 | 80KB | |
| coredump | data/coredump | 0xfe4000 | 112KB | |

Bootloader: ESP-IDF v6.0.1, valid. Stock is **pure ESP-IDF v6.0.1**, not Arduino. Its app slots
(7.88MB) are bigger than CrossPoint's (6.25MB, `partitions.csv`); flashing CrossPoint writes its own
table, and the full-image revert above restores stock's. The About screen said `XT V7.2.4` at 14:24,
but the active slot holds 7.5.10 — with Auto Check Updates on, it most likely auto-updated over WiFi
after the photos were taken. Unconfirmed: check About Device.

**Why run the dump at all, and what to expect (Logan's goals: confirm the dump is good, then mine
stock's UI for CrossLight ideas):**
- *Is it good?* — already answered by the hash checks above, which are stronger evidence than booting
  it would be.
- *Emulating it for UI:* Espressif's QEMU fork (`qemu-system-xtensa -machine esp32s3`) can boot this
  exact image and will show bootloader/app serial logs. It will **not** draw the UI — QEMU has no
  model of the e-ink panel, GT911 touch, buttons, SD, or frontlight, so the app likely stalls in
  display/touch init. Not worth it for UI.
- *Better UI sources:* (1) the device itself — it's still running stock until step 13, so photograph
  every screen now; (2) static analysis of `app1` — strings (menu names, the cloud-sync host), embedded
  fonts (MiSans) and icon bitmaps, and possibly the panel waveform tables (the one thing that would
  directly improve CrossLight if stock's refresh looks better — speculative until looked at).

**Stock UI patterns worth borrowing** (from `docs/images/stock-firmware-baseline/`, Claude's read,
2026-09-18 — ideas, not decisions):
- **Continue-reading card on Home** — a big cover + title/author/%/time-read + "Continue" strip at the
  bottom of Bookshelf. Same idea as the Bible hub's planned "Continue Reading" row.
- **Paged lists with explicit ▲/▼ and a counter** ("2 folders 0 files", "1/1") instead of scrolling —
  e-ink-friendly, and what physical buttons map onto naturally.
- **Dithered scrim behind overlays** — the side drawer dims the page with a checkerboard, a
  cheap way to show depth with only black and white.
- **Consistent chrome** — back arrow top-left, centered title, one action top-right ("Get Fonts",
  "Retry All", "Rescan"); settings rows are icon + label + gray subtitle + chevron or pill toggle.
- **Line-art illustration for USB Mode** plus a centered "Preparing…" interstitial — the connection
  state is unmistakable.
- Weaker spots not to copy: primary nav hidden behind a hamburger drawer (extra tap for everything),
  a mostly-empty Bookshelf grid with truncated titles ("The Phanto..."), low-contrast gray secondary
  text.
- Not yet checked: how many of these CrossPoint's existing themes (Lyra, Lyra Extended, RoundedRaff)
  already do — check before building any.

**First flash and first on-device session (2026-09-18):**
- **Step 13 skipped by Logan's call** — flashed CrossLight directly, not plain CrossPoint first. If
  something looks wrong on-device, flashing plain CrossPoint is still the fastest upstream-vs-ours test.
- **How it was flashed:** `esptool erase-flash` first (wipes stock NVS — confirmed all-`0xFF` at
  0x9000 — and stock's leftover regions), then `pio run -e x4pro -t upload`. The upload writes bootloader
  (0x0), CrossPoint's partition table (0x8000), `boot_app0.bin` (0xe000, resets otadata to app0) and the
  app (0x10000). **Don't use upstream README's app-only `write_flash 0x10000`** on a unit fresh from
  stock: stock's otadata pointed at app1, so an app-only flash would "succeed" and keep booting stock.
  Writing to flash is unaffected by the esptool read bug (hash verified on every upload).
- **Panel controller answered (step 12's open question): UC8279**, not SSD1677/UC8179 — boot log:
  `bus probe ... -> UltraChip`, `promoted SSD1677 -> UC8279 800x480 (LUT_VER=02)`, driver `8279x4`.
  Refresh: full ~1331 ms, fast ~485 ms. Consequence: grayscale sleep images need SSD1677
  (`SleepActivity` gates on it), so the custom sleep screen renders 1-bit on this unit.
- **SD card as set up:** `/Bible/KJV/kjv.json` (+ the generated `kjv.json.cache`), `/sleep.bmp` (the
  stock Sabaton wallpaper, converted from stock's `Pushed Images/*.xth` — XTH is a 22-byte header + two
  column-major bit planes, decoded with the same mapping as `lib/Xtc/Xtc.cpp`, written as a 2-bit 4-gray
  BMP), `/Books/` (4 EPUBs). Stock's own folders (`XTApps`, `XTCache`, `XTData`, `Pushed *`) left alone.
- **Bible on real hardware — two bugs the simulator structurally couldn't show (item 15 partly done):**
  | Symptom | Cause | Fix | Measured |
  | --- | --- | --- | --- |
  | Opening the Bible froze ~10s | `onEnter()` ran `loadBookIndex` = full SAX parse of the 8.9MB JSON, just for 66 names + chapter counts. `loadChapter` also re-scans from the file start, so late books cost another near-full pass. The book picker did its own full pass too. Sim hid it (host SSD/CPU ~100× faster). | 1) 512B stack read buffer → 16KB heap (7.0s, parse-bound, not I/O-bound). 2) **Chapter cache** `<json>.cache`: one full parse writes book table + every chapter's verses at known offsets (4.25MB); invalidated by source size/mtime; reader and book picker read it first, JSON fallback if anything fails | Open: 10,060 ms → **7 ms** (book list) + **8 ms** (chapter). One-time cache build: 14.7 s behind a Loading popup |
  | No way to reach Books/Verse/Bookmarks | Bible menu only opened on `Confirm`; the X4 Pro has no Confirm button | Also accept `ReaderUtils::isTouchMenuGesture` (center-third tap / menu swipe), same as the EPUB reader | Confirmed on device |
  Host tests: 353 pass, including a full-KJV check (skipped where `fs_/` is absent) that the cache's
  first and last chapter of every book match the JSON loader byte for byte.
- **Serial logging from the device:** CrossLight logs over the same USB-Serial/JTAG. Opening the port
  **resets the chip** even with DTR/RTS pre-cleared (Linux raises them on open), so attaching a logger
  reboots the device — harmless, just expect it. `Errno 71` on port open = device asleep; wake it.

*Stock bring-up — before flashing anything else:*
12. Boot on stock firmware. Note the panel-controller batch from the boot/about screen or visible
    behavior (SSD1677 vs UC8179 — see "freeink-sdk findings" above; both auto-detect, but which one
    this specific unit has is still unconfirmed). Exercise display, touch, WiFi connect, frontlight
    (both warm/cold channels), sleep/wake, Home key, and reading an EPUB, and write down what stock
    behavior actually looks like (refresh timing/ghosting, sleep image, boot time) — this is the
    baseline the next step gets compared against, not just a box-check.
13. Flash an unmodified, self-built `x4pro` CrossPoint image (no CrossLight changes). Re-run the same
    checklist as #12. **Must match stock behavior before any code changes go on the device** — if it
    doesn't, that's an upstream/build issue to resolve first, not a CrossLight bug to chase.

*Then CrossLight itself:*
14. Flash the real `x4pro` CrossLight build. Confirm the same baseline list once more (display, touch,
    SD, WiFi, frontlight, sleep/wake, Home key, EPUB reading) still holds with the fork's changes in.
15. Bible app smoke test on real hardware — this is the first time any of Phase 1's simulator-verified
    behavior touches real e-ink timing/PSRAM/touch: open the Bible, book/chapter picker, page through a
    chapter/book boundary (the simulator can't exercise real e-ink refresh-batching or true PSRAM
    behavior — see "Desktop dev loop" above), confirm reading position survives a real sleep/wake and a
    real power-off/on, not just a simulator relaunch.
16. Bookmarks on real hardware (item 11). Everything below is simulator-verified already, so the point
    is only what the simulator structurally cannot test — check in this order, since each one isolates
    a different layer:
    - **Real SD write latency on toggle.** Every toggle does a synchronous `saveToFile()` on the
      button-press path. On host disk that's free; on SD behind a full-refresh e-ink update it may not
      be. If toggling feels laggy, that's where to look first — the store, not the UI.
    - **Persistence across a real power cycle**, not just an app relaunch — same distinction as #15.
      Bookmarks and reading position are separate files; confirm both survive independently.
    - **The `*` marker under the device's real font.** It is ASCII precisely so this should be boring,
      but it is the only visual confirmation a toggle worked, so verify it before trusting the feature.
    - **Touch activation on the menu and picker rows.** Both screens set `inputMask = InputTouch` with
      physical buttons handled in `loop()` — that split is untested against a real GT911 digitizer.
    - **A bookmark whose saved page no longer exists** (bookmark a late page, change font size, jump
      back). `goTo()`'s clamp handles it; this is the path most likely to be wrong on hardware, since
      pagination depends on real font metrics.
    - **Verse-ref jump speed on the real keyboard** (item 12). The whole question of whether verse-ref
      earns its keep: is typing `John 3:16` letter-by-letter on e-ink actually faster than scroll-picking
      book+chapter? It feels fast on the sim only because that's a host keyboard. If it's slow here, it's
      a drop candidate in the cut pass.
    - **The full-text-search micro-benchmark** (no feature yet — just the measurement that gates it).
      Time a bare `StreamingJsonParser` pass over `kjv.json` from the real SD card, no search logic. That
      one number decides whether whole-Bible search (approach A), current-book-only (B), or an index (C)
      is the viable design — see "Bible full-text search — scoping" below. Cheap to run, needs only the
      hardware, and blocks nothing else.
17. If anything in 14/15/16 regresses vs. #13's CrossPoint-unmodified baseline, that narrows the cause
    to CrossLight's changes specifically (Bible app or fork strip) rather than upstream/build/hardware —
    the point of doing 12/13 first instead of jumping straight to the fork build.
18. Only after 12-17 hold: full-text search (design chosen from #16's micro-benchmark number, not
    guessed — see scoping below); the translation downloader (scoped below) after that.

## Bible data/feature shape (for when that work starts)

- SD layout: `/Bible/<ABBREV>/<abbrev>.json` — the raw getBible file, verified working (see below), not
  the byte-offset `bible.dat`/`index.bin` scheme once sketched here; that remains a possible later
  optimization, not the current design.
- Search: sequential scan for the prototype; word→verse-ID index once proven. Tens of thousands of
  verses total — no SQLite/search engine needed.
- Offline-first: reading never needs WiFi. **A translation pre-dropped on the SD card just works with
  zero download** — the SD-read path is primary and now built (see below); the getBible downloader is
  only a convenience for fetching new ones into the same `/Bible/<ABBREV>/` folder. KJV is the default
  (imperfect but least-encumbered available option — see the corrected license note further down, it is
  NOT simply public domain). Not bundled inside the firmware image (keeps flash lean); document "drop a
  translation file on the card" instead.
- Translation source = **getBible v2 API** (what OpenBible2 uses; a front end for Crosswire SWORD
  modules). **Verified live 2026-09-09 by fetching the real files** (not assumed from OpenBible2's
  source): the host `api.getbible.life` (what OpenBible2 uses) now 301-redirects to
  **`api.getbible.net`** — point the loader at `api.getbible.net` directly.
  - Catalog: `GET /v2/translations.json` → dict keyed by abbreviation, 117 translations. Each entry
    carries a `distribution_license` field (verified KJV's is `"GPL"`, not public domain — see below).
  - One translation: `GET /v2/<abbrev>.json` → `{ translation, abbreviation, distribution_license, ...,
    "books": [ { nr, name, "chapters": [ { chapter, name, "verses": [ { chapter, verse, name, "text" }
    ] } ] } ] }`. Confirmed via real KJV fetch: 66 books, 31,102 verses, `text` is clean plain text (no
    embedded Strong's/morphology markup despite the translation including that data elsewhere). File
    size ~8.9MB; actual verse text is only ~4.0MB of that — rest is metadata/JSON structural overhead.
  - **License reality, corrected:** KJV is NOT simply "public domain, freely bundleable." Its own
    `distribution_about` states "The rights to the base text are held by the Crown of England" — public
    domain in the US, but the UK Crown holds a perpetual printing-rights patent there. Separately, this
    specific getBible distribution (Strong's/morphology edition) is tagged `distribution_license: GPL`
    by the source. Not a blocker (GPL-3 already decided acceptable to ship under — see "Decisions made"
    above), but **check `distribution_license` per translation programmatically before treating any as
    freely distributable** — the catalog conveniently exposes this per entry; don't assume any
    translation's status without reading that field.
  - OpenBible2 stores `<abbrev>.json` and re-downloads on SHA-checksum change (from the catalog). Same
    pattern works for CrossLight.
- File format on SD: **raw getBible JSON, verified working, not just planned.** `BibleChapterLoader`
  streams it through `StreamingJsonParser` (SAX, no DOM) rather than loading the whole ~9MB file at
  once — confirmed necessary and sufficient against a real KJV file. A byte-offset index / compact
  `bible.dat` remains a possible later optimization if streaming-and-rescanning per navigation proves
  too slow on-device, but isn't needed yet and hasn't been shown to be.
- Standard Ebooks (https://standardebooks.org/) and Project Gutenberg for public-domain EPUB reading
  generally — the normal reading workflow this device is for.

### Bible translation downloader — API/infra scoping (2026-09-09, not yet built)

Checked whether this needs building from scratch before scoping it: it doesn't. CrossPoint already
ships everything the download side needs, in production use today by OTA updates and the OPDS book
browser. This is reuse, not new infrastructure — the only genuinely new piece is the picker UI.

- **HTTP client — reuse directly:** `HttpDownloader` (`src/network/HttpDownloader.h/.cpp`), built on
  `esp_http_client`. `fetchUrl(url, ...)` for the `translations.json` catalog (small JSON, fits in
  memory); `downloadToFile(url, destPath, ProgressCallback, cancelFlag*, ...)` for a translation file
  (multi-MB — KJV is 8.9MB, some translations will be larger). Handles HTTPS CA verification and has a
  `ProgressCallback` + cancel flag already, which a multi-MB transfer needs. Also carries
  `MIN_TLS_FREE_HEAP`/`MIN_TLS_MAX_ALLOC` heap pre-flight constants — check these before opening the
  connection, same as OPDS does (below), since a translation file is meaningfully bigger than a typical
  OPDS ebook.
- **WiFi connect — reuse directly:** the `checkAndConnectWifi()` / `launchWifiSelection()` /
  `onWifiSelectionComplete(bool)` three-method pattern, reference implementation in
  `OpdsBookBrowserActivity` (`src/activities/browser/`), backed by the shared `WifiSelectionActivity`
  (`src/activities/network/`). Every network feature in this codebase gates on WiFi this same way;
  no reason for the Bible downloader to do it differently.
- **Browse-catalog-and-download UI — reuse the *shape*, not the class:** `OpdsBookBrowserActivity` is a
  full `Activity`/`UiAppHost` (not a `UiListActivity`) with `buildBrowsingScreen()` /
  `buildDownloadScreen()` (progress UI, cancel-flag-driven) / `buildStatusScreen()` and a
  `downloadBook(entry)` that does the heap pre-flight check before calling `downloadToFile()`. It's
  OPDS-XML-entry-shaped internally, so not directly subclassable — but it's the concrete template: a
  new `BibleTranslationDownloadActivity` (name TBD) of comparable weight (not a small add-on to the
  existing `UiListActivity`-based `BibleBookSelectionActivity`/`BibleChapterSelectionActivity` pickers)
  with the same catalog-list → confirm → progress-download → save shape.
- **Flow, given the above:** list `translations.json` (117 entries) → user picks one → show its
  `distribution_license` and confirm (per the license-reality note above, this must be surfaced, not
  silently downloaded) → `downloadToFile()` into `/Bible/<ABBREV>/<abbrev>.json` with a progress screen
  → `BibleReaderActivity`/`BibleBookSelectionActivity` already read from that exact path shape with zero
  changes needed (the SD-first design was already built this way — see "Offline-first" above).
- **Not yet decided:** re-download-on-checksum-change (OpenBible2's pattern, mentioned above) vs.
  one-shot download with manual re-fetch; where the translation list/picker hangs off the home menu or
  UI (a Bible settings/translations sub-screen, most likely, once one exists) vs. inside
  `BibleBookSelectionActivity` itself.
- Sequencing: after the device-arrival checklist above and after search/bookmarks (item 16) — this
  isn't blocked technically (everything it needs already exists and works on the simulator, no hardware
  required to build or test the download flow), just deliberately ordered behind get-the-MVP-solid on
  real hardware first.

### Bible full-text search — scoping (2026-09-10, not built, perf-gated on hardware)

Distinct from the verse-*reference* jump already shipped (item 12, "go to John 3:16"). This is content
search — "find the verse that says *X*" — the one search pickers can't replace. Scoped now, at Logan's
request, as a candidate feature for a **Bible-leaning build** (see "Build strategy"): if a given image
spends less on wireless tooling, this is one of the things the freed attention goes into.

**The one blocking unknown is perf, and the simulator cannot answer it.** A naive search is a full
streaming pass over the ~8.9MB `kjv.json` from SD per query. On the sim that file is on host SSD, so a
sim timing measures parse cost while hiding the SD-read cost — and on a 1-bit SDMMC card the read is
likely the dominant term. So **the first actual step is a micro-benchmark on the device, not code**:
time a bare `StreamingJsonParser` pass over `kjv.json` on real hardware with no search logic at all.
That number is the floor for every approach below and decides which is viable. Don't design an index
before knowing it. [[verify-dont-assume]].

**Approaches, cheapest-first:**
- **A — naive per-query streaming scan.** Reuse `StreamingJsonParser`; test each verse's `text` against
  the query as it streams; collect matches into a bounded results vector (cap it, e.g. first 100 hits,
  so RAM stays flat regardless of query). Zero new storage, ~no new flash beyond the match/UI glue.
  Viable *iff* the micro-benchmark says a full pass is a tolerable wait on e-ink (e-ink redraws are
  already ~1s, so a few seconds with a progress indicator may be fine; 10s+ is not). This is the one to
  try first.
- **B — scope to the current book.** Same scan, one book instead of 66 → roughly 50x less data for an
  average book. The obvious fallback if A's whole-Bible pass is too slow, and often what you actually
  want ("find this phrase in John"). `loadBookIndex()` already gives the per-book structure to bound it.
- **C — prebuilt inverted index** (word → verse refs), built once and stored on SD, queried at search
  time. Fast queries, but real complexity: index build cost (precompute on desktop and ship it, or
  build on-device once — minutes, and a translation-download would have to trigger a rebuild), and index
  size (an inverted index over the whole KJV is likely multi-MB — fine on SD, but a lot of moving
  parts). Almost certainly overkill for personal use; only revisit if both A and B are too slow *and*
  search turns out to be used constantly.

**Match semantics:** start with case-insensitive substring — simplest, and enough for "I remember a
phrase." Word-boundary matching and stemming ("love" also matching "loved/loving") are future polish,
not v1. Reuse the same UTF-8-safe comparison discipline the rest of the Bible code already follows.

**UI:** reuse `KeyboardEntryActivity` for the query (same as verse-ref), render results as a
`UiListActivity` of `Book C:V` rows plus a short text snippet, and jump on select via the existing
`goTo()` / `goToVerse()` — so the results screen is the only genuinely new surface, everything it
launches into already exists.

**Flash / build-flavor honesty:** search *code* is KB-scale in any of these forms — it fits even a
wireless-heavy build, so "we need a Bible build to afford search" isn't really a flash statement. What a
Bible-leaning profile actually buys here is **CPU/UX headroom and attention**, not flash: a multi-second
blocking whole-Bible scan is acceptable on a device whose job is reading, less so on one juggling a live
radio. The only version of "more Bible" that genuinely spends *flash* is baking large data tables into
the image (a concordance, Strong's numbers) instead of SD-loading them — keep that kind of data on SD by
default, same rule as the translation files and the Flock signatures.

**Sequencing:** run the micro-benchmark during the device-arrival checklist (a natural addition to
item 15/16 — it needs the hardware and nothing else), then decide A vs. B from the number. Not before.
