# PLAN.md

Status: architecture mapped from source; device not in hand yet (2026-09-09)

Personal fork of [crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader) for an
Xteink X4 Pro. `origin` = upstream, `fork` = `flyboy-byte/crosspoint-reader`. Fork name: **CrossLight**.

## Path forward (two phases)

1. **Phase 1 (current focus): solid e-reader + Bible app.** CrossPoint as-is, plus the native Bible
   app. This is the whole scope until it's working and stable on the device.
2. **Phase 2 (later): broader custom firmware**, Biscuit-inspired — plain networking (WiFi
   connect/scan, HTTP client, ping/DNS, file transfer), wireless security/red-team tooling (recon,
   offense, defense — see Biscuit's tile breakdown), and a Flock/ALPR-camera-detection tool (privacy-
   awareness category, same shape as tracker detection). Sequenced after Phase 1, not alongside it.
   - **BLE is a real open question, not a known blocker.** The ESP32-S3 chip supports BLE 5 (LE) at the
     silicon level, same as WiFi. `freeink-sdk` has no BLE HAL module yet (checked `libs/hardware/` —
     WiFi has one, `SecureNet`; BLE doesn't). Unconfirmed whether the X4 Pro board even routes an
     antenna path for it. Check the board spec / FreeInk community once the device is in hand before
     assuming BLE-dependent apps (BLE Scanner, Mesh Chat, BLE Spam, Tracker Detector, etc.) are portable
     at all — building a BLE HAL layer from scratch is bigger scope than porting a WiFi-only app.
   - Biscuit itself targets the X4 (ESP32-C3), not the X4 Pro (S3/PSRAM) — porting its WiFi/tools/games
     tiles is real work, not a recompile, but is fine to do; it currently runs its full suite on
     *worse* hardware than the X4 Pro, which is a good sign for headroom once ported.
   - App flash budget: ~6.25MB per OTA slot (`partitions.csv`: `app0`/`app1` at `0x640000` each), not
     the full 16MB — budget against that number, not the marketing spec.
   - Dropped: Bionic Reading / CrossInk typography features. Not pursuing.

## Where things stand

- Device ordered, in transit from China — not arrived yet. No firmware flashed, no code written.
- Read through the repo (activities, storage, settings, build config) to know the terrain before the
  device shows up. Findings below.

## Decisions made

- **Base firmware: CrossPoint**, not CrossInk (typography-focused fork) or CrossPlay (apps/games fork).
  Main upstream project, already supports the X4 Pro, activity-based structure reads as extensible.
- **Bible app is an isolated module** — own app/activity/data tree, not woven into `lib/Epub/` or
  reader internals. Keeps upstream rebases cheap; leaves the door open to PR a generically useful piece
  later (e.g. a verse-index format) without dragging the rest along.
- **OpenBible2 is a reference, not a porting target.** Confirmed by inspecting the repo: Kotlin/Compose/
  Android, Apache-2.0. No JVM or Compose runtime exists on the ESP32-S3, so there's nothing to port to —
  this is a native C++ reimplementation of the idea. A PR adding e-ink support to OpenBible2 itself
  doesn't make sense either (grafting a whole second toolchain onto an Android repo). If code or a
  specific algorithm gets lifted rather than just the concept, carry Apache-2.0 attribution.
- **DRM: deferred until further notice.** Adobe and Kindle aren't goals at all. Readium LCP is the only
  technically interesting option, and only ever as its own separate research track later.
- **Manga: untested, not decided.** Try one legal sample on real hardware before any comic-specific
  work. CrossPlay is prior art if this proceeds.
- **GPL-3 is fine to ship under.** Top-level `LICENSE` says MIT, but `wolfssl/Arduino-wolfSSL@5.7.2`
  (statically linked, in `platformio.ini`) is GPL-3.0 — confirmed via GitHub API, not assumed. wolfSSL's
  free tier is GPL-3 on purpose (paid tier avoids it), which makes the combined firmware binary GPL-3
  when distributed. Not a blocker here. If anything from this fork goes back to upstream CrossPoint,
  flag this — upstream's `LICENSE` doesn't currently acknowledge it.
- No dedicated open-source e-ink Bible app exists to build on (checked KOReader plugins and the
  CrossPoint fork ecosystem — CrossInk/CrossMux/crosspoint-reader-apps). The native rewrite is the path.

## Architecture notes (read directly from source, not assumed)

- `ActivityManager` (`src/activities/ActivityManager.h`) owns a stack of `Activity`-derived screens, one
  shared render task/mutex — see `docs/activity-manager.md`. This replaced an older per-activity-task
  model; anything describing that older model elsewhere is stale.
- New screens use FreeInkUI (`docs/contributing/touch-and-ui.md`): `UiListActivity` for a single list,
  `UiTabListActivity` for tabbed lists, `UiAppHost` directly for custom layouts. One hosting stack only.
- **Menu registration isn't declarative.** The home menu is a hand-maintained `enum class HomeMenuItem`
  in `ActivityManager.h` plus manual index math in `HomeActivity::menuItemToIndex`/`indexToMenuItem`. A
  Bible entry point means editing both directly — no registry to hook into.
- Settings persist as JSON on SD via `HalStorage`/`PersistableStore` (`src/CrossPointSettings.h`,
  `/.crosspoint/` on SD, SPIFFS not mounted). **Decided:** Bible settings (selected translation, last
  reference) get their own `PersistableStore` subclass under `/.crosspoint/`, not folded into
  `CrossPointSettings`. Translation data files themselves do NOT go under `/.crosspoint/` — that
  directory is reserved for cache/settings, not user content. There's no single fixed "books" folder in
  CrossPoint either: the file browser browses the whole SD card freeform, and OPDS downloads default to
  SD root or a configurable folder (`SETTINGS.opdsDownloadFolder`). So translations live as ordinary
  user-visible SD content, same tier as books — a dedicated `/Bible/` folder at SD root (see data shape
  below), not tucked inside the cache directory.
- Storage access only through `HalStorage`/`HalFile`, never raw SdFat (unsynchronized SPI state crashes
  under concurrent access — see `AGENTS.md`).
- X4 Pro build: `[env:x4pro]` in `platformio.ini` — ESP32-S3, `dio_opi` PSRAM variant, native 1-bit
  SDMMC. `pio run -e x4pro`, `pio run -e x4pro -t upload`.
- Host-side GoogleTest harness (`test/`, 19 suites, no hardware needed) — worth using once a Bible
  verse-index/lookup format exists.
- **Unconfirmed:** no on-repo UI simulator found (searched README/AGENTS.md/docs — no hits), despite
  early notes assuming one exists. Don't plan around it until it turns up somewhere. `freeink-sdk/`
  (book APIs, display-controller variants) hasn't been inspected yet either.

## Next steps

**No hardware needed:**
1. Inspect `freeink-sdk/` for `FreeInkBook`/display-controller handling (SSD1677 vs UC8179).
2. Read 1-2 full `UiListActivity` subclasses end-to-end to settle the Bible module's directory shape.
3. Sketch the Bible data format against `HalStorage`'s actual read/seek API (see shape below).

**Blocked on device arrival:**
4. Run stock firmware briefly, document hardware/display-controller revision.
5. Flash unmodified CrossPoint (`x4pro` env); verify display, touch, SD, Wi-Fi, frontlight, sleep/wake,
   Home key, EPUB reading.
6. Confirm a self-built unmodified image matches stock behavior before any code changes.
7. Add the smallest possible test menu entry (extend `HomeMenuItem`) rendering a static test chapter.
8. Then: book/chapter picker → pagination → search → bookmarks/history, in that order. Translation
   downloader comes after the MVP is stable, not before.

## Bible data/feature shape (for when that work starts)

- `/Bible/<TRANSLATION>/bible.dat` + `index.bin` (byte offsets per book/chapter/verse) + `meta.json`.
  Seek directly to a chapter rather than loading a whole translation. JSON-on-SD is an acceptable
  easier first prototype before the binary/index format.
- Search: sequential scan for the prototype; word→verse-ID index once that's proven out. Tens of
  thousands of verses total — no need for SQLite or an external search engine.
- Offline-first: reading never needs Wi-Fi. Network use is limited to installing/updating translations.
  `getBible` (https://getbible.net/) is a plausible source — check per-translation licensing before
  bundling anything; ship no copyrighted text in the firmware image itself.
- Standard Ebooks (https://standardebooks.org/) and Project Gutenberg for public-domain EPUB reading
  material generally — unrelated to the Bible app, just the normal reading workflow this device is for.
