# PLAN.md

Status: architecture + freeink-sdk mapped from source; device not in hand yet (2026-09-09)

Personal fork of [crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader) for an
Xteink X4 Pro. Fork name **CrossLight**, repo `flyboy-byte/crosslight`. Remotes: `origin` = upstream,
`fork` = CrossLight. Branches: `develop` = untouched upstream mirror (never commit); `crosslight` =
personal main line + fork default branch; topic branches off `crosslight`, deleted after merge.

**Keep this fork current:** upstream is very active. Periodically `git fetch origin && git merge
origin/develop` into `develop`, then merge `develop` into `crosslight`. Small frequent catch-ups, not
one big drift-merge.

## Path forward (two phases)

1. **Phase 1 (current focus): solid e-reader + Bible app.** CrossPoint as-is, plus the native Bible
   app. This is the whole scope until it works and is stable on the device.
2. **Phase 2 (later): broader custom firmware**, one capability at a time, measuring flash/heap after
   each (`scripts/firmware_size_history.py`, `ESP.getFreeHeap()`). Sequenced after Phase 1.
   - **Plain networking:** WiFi connect/scan, HTTP client, ping/DNS, file transfer. Same weight class as
     what CrossPoint already ships; the enabler for the Bible translation downloader.
   - **Biscuit-derived tools** (`rayrayrayyyym/biscuit`, MIT, but targets the C3 X4 — porting to S3 is
     real work, not a recompile). Its eight tiles split into: plain networking (above), genuinely
     dual-use offensive security (deauth, credential-capturing captive portal, AP cloning, BLE/USB HID
     injection), defensive/awareness (tracker detection, rogue-AP/camera sweep, MAC rotation, RF kill),
     comms (ESP-NOW mesh chat, anonymous file drop), and utilities/games (TOTP, password manager,
     cipher tools, calculator, chess, etc.). Offensive tiles are fine for authorized testing on own
     gear; the narrow carve-out is anything aimed at deceiving/attacking people or networks not yours.
   - **Flock camera detector** (idea from `colonelpanichacks/flock-you`, MIT — *inspiration, not a
     direct port*). Passive 2.4GHz promiscuous sniff for Flock camera OUIs + IE fingerprint. Runs on
     the same ESP32-S3, needs no BLE, purely passive → cleanest fit (privacy/awareness, detects cameras
     watching you). Port the detection logic only (not the Flask/GPS wardriving dashboard). Keep OUI +
     IE signatures in an updatable SD file — Flock changes behavior often and detection methods keep
     breaking. US/Canada only (that's where Flock is deployed). A dedicated scanner screen, never
     background (promiscuous mode monopolizes the radio, so it can't run while reading).
   - **Coupling stance:** OK to strip the fork back somewhat rather than keep upstream's tree pristine;
     deal with upstream-PR conflicts later if that ever happens. Note the tradeoff: stripping raises the
     conflict cost of the periodic upstream merges too, so strip conservatively (things upstream rarely
     touches) and eat conflicts when they land.
   - App flash budget: ~6.25MB per OTA slot (`partitions.csv`: `app0`/`app1` at `0x640000` each), not
     the full 16MB. Dropped: Bionic Reading / CrossInk typography. Not pursuing.

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

## Next steps

**No hardware needed:**
1. ~~Inspect `freeink-sdk/`~~ — done (findings above).
2. Read 1-2 full `UiListActivity` subclasses + `crosspoint-reader-apps`'s `App`/`AppRegistry` end-to-end
   to settle the Bible module's directory shape and menu-registration approach.
3. Prove the host-render loop: minimal desktop build that renders a `FreeInkUIDisplayTarget` screen to a
   PNG, so Bible UI can be iterated without the device. (Offered; not yet built.)
4. Sketch the Bible data format against `HalStorage`'s read/seek API (shape below).

**Blocked on device arrival:**
5. Run stock firmware briefly, document hardware/display-controller batch (SSD1677 vs UC8179).
6. Flash unmodified CrossPoint (`x4pro`); verify display, touch, SD, WiFi, frontlight, sleep/wake, Home
   key, EPUB reading. Confirm a self-built unmodified image matches stock before any code changes.
7. Smallest possible Bible menu entry rendering a static test chapter, then book/chapter picker →
   pagination → search → bookmarks/history. Translation downloader after the MVP is stable.

## Bible data/feature shape (for when that work starts)

- `/Bible/<TRANSLATION>/bible.dat` + `index.bin` (byte offsets per book/chapter/verse) + `meta.json`.
  Seek directly to a chapter; don't load a whole translation. JSON-on-SD is an OK first prototype.
- Search: sequential scan for the prototype; word→verse-ID index once proven. Tens of thousands of
  verses total — no SQLite/search engine needed.
- Offline-first: reading never needs WiFi. Network only for installing/updating translations.
- Translation source = **getBible v2 API** (what OpenBible2 uses; a front end for Crosswire SWORD
  modules). Catalog: `https://api.getbible.life/v2/translations.json`; one translation:
  `https://api.getbible.life/v2/<abbrev>.json` (single JSON, `books[]→chapters[]→verses[]`, 66 books
  for a full Bible). OpenBible2 stores `<abbrev>.json` and re-downloads on SHA-checksum change. CrossLight
  would fetch the same, then either keep JSON + build a byte-offset index, or transcode to `bible.dat`.
  Check per-translation licensing; ship no copyrighted text in the firmware image.
- Standard Ebooks (https://standardebooks.org/) and Project Gutenberg for public-domain EPUB reading
  generally — the normal reading workflow this device is for.
