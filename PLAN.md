# PLAN.md

Status: Bible reader with a real book/chapter picker (real KJV data, verified in simulator); device not in hand yet (2026-09-09)

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

**Known limitations, not yet fixed (flagged, not hidden):**
- `StreamingJsonParser`'s fixed 512-byte token buffer still *drops* (not truncates) any string over that
  length. One verse in all of KJV exceeds it (Esther 8:9, 528 chars) — its text is currently lost
  silently. Needs either a larger buffer (check other consumers first) or a dedicated overflow path.
- Still only the hardcoded KJV path (`/Bible/KJV/kjv.json`) — no translation selection — and no
  pagination beyond "truncate at the screen edge" (verses are single-line-truncated, not wrapped).

**No hardware needed, next up:**
8. Real line-wrapping/pagination for verse text (currently one truncated line per verse).
9. Fix the Esther 8:9 token-overflow gap above.

**Blocked on device arrival:**
10. Run stock firmware briefly, document hardware/display-controller batch (SSD1677 vs UC8179).
11. Flash unmodified CrossPoint (`x4pro`); verify display, touch, SD, WiFi, frontlight, sleep/wake, Home
    key, EPUB reading. Confirm a self-built unmodified image matches stock before any code changes.
12. Then: search, bookmarks/history. Translation downloader after the MVP is stable.

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
