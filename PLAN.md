# PLAN.md

## ▶ RESUME HERE (anchor, 2026-10-05)

**THIS SESSION (2026-10-05) — Claude Panel + Ask Claude + Bible Passage Q&A built, debugged live on
real hardware via USB, two real bugs found and fixed, release 26.10.9 pushed and confirmed working.
Nothing committed yet** (Logan: leave it uncommitted). `git status` should show `.gitignore`, `PLAN.md`,
`platformio.ini`, `lib/I18n/translations/english.yaml`, `src/utilities/UtilityRegistry.cpp`,
`src/activities/bible/CompareTranslationsActivity.cpp`, `src/activities/bible/BibleMenuActivity.*`,
`src/activities/bible/BibleReaderActivity.*` modified; `docs/crosslight/claude-panel.md`,
`docs/crosslight/claude-features.md`, `src/claude/`, `src/activities/utilities/ClaudePanelActivity.*`,
`src/activities/utilities/AskClaudeActivity.*`, `src/activities/bible/BiblePassageQaActivity.*`
untracked. `platformio.local.ini` also modified (gitignored, not in `git status`) to mirror the two
new wolfSSL flags for the simulator build.

**26.10.8 was NOT actually working — first real-hardware test found two bugs, both now fixed in
26.10.9 and confirmed live on Logan's X4 Pro:**

1. **TLS to api.anthropic.com was completely broken.** Two wolfSSL config gaps in
   `freeink-sdk/libs/network/SecureNet/src/SecureClient.cpp` (the fix itself is two `-D` flags added to
   `[env:x4pro]` in `platformio.ini`, mirrored in `platformio.local.ini`):
   `-DWOLFSSL_ALT_CERT_CHAINS` (Anthropic's chain ends in a GlobalSign cross-signed GTS Root R4, not the
   self-signed one pinned; wolfSSL's default walker only trust-checks the chain's last link) and
   `-DWOLFSSL_SHA384` (verifying that root's SHA-384 signature was failing because this build never
   enabled software SHA-384). Full root-cause writeup: `docs/crosslight/claude-panel.md` → "Hardware
   debugging" section.
2. **`ask()` used `claude-opus-5-5`, which 429'd every time** with a genuine `rate_limit_error` (not
   transient — still failed after a 20-minute idle gap). Logan's subscription tier doesn't carry Opus
   access via this OAuth token. Swapped live (rebuild+reflash per model, no code restructuring) to
   confirm: Haiku → HTTP 200 immediately, Opus → 429 every time. **`kAskModel` is now permanently
   `claude-haiku-4-5`** in `src/claude/ClaudeClient.cpp` — Logan chose Haiku over testing Sonnet further.

**Debugging method, if this pattern recurs:** `pio device monitor` needs a real tty and fails in a
sandboxed/non-interactive shell (`termios.error: Inappropriate ioctl for device`). Worked around with a
~10-line pyserial script reading `/dev/ttyACM0` for a fixed window — see any `serial_capture*.log` this
session's scratchpad for the pattern. USB flashing a single env (`pio run -e x4pro -t upload
--upload-port /dev/ttyACM0`) is much faster than an OTA round-trip for iterating on a hardware bug —
each cycle was ~2.5-4.5 min build+flash. The device dropped off `/dev/ttyACM0` at least twice mid-session
(Logan unplugging/replugging); `ls /dev/ttyACM*` is the quick check before assuming the upload tool is
broken.

**Known small gap, not blocking:** the `freeink-sdk` git submodule (points at upstream
`Free-Ink/freeink-sdk`, no CrossLight fork, unlike `crosslight-simulator`) has one uncommitted,
non-load-bearing diagnostic log line in `SecureClient.cpp` (logs if `wolfSSL_CTX_load_verify_buffer`
fails). The actual fix is the two `-D` flags in the main repo's `platformio.ini`, already safe. Losing
that one log line on a submodule reset would just remove a diagnostic breadcrumb, not break anything —
but if `freeink-sdk` needs real fixes again, it's worth forking it the way the simulator was forked.

**Bible Passage Q&A: TLS layer confirmed working (handshake ok in the serial log) but not
independently re-confirmed end-to-end after the Haiku switch** — same shared `ClaudeClient::ask()`, so
it should work, but Logan moved to closing the session before testing it standalone again. Worth a
quick check next time he's on the device.

**Release:** https://github.com/flyboy-byte/crosslight/releases/tag/26.10.9 — notes describe both
fixes and call out the Bible Passage Q&A gap explicitly.

- **Claude Panel — CONFIRMED WORKING on real hardware** (usage fetch, 5h/7d bars). Full plan +
  findings: `docs/crosslight/claude-panel.md`.
- **Ask Claude — CONFIRMED WORKING on real hardware** (new question → Haiku → answer, HTTP 200).
- **Bible Passage Q&A — TLS confirmed working, full on-device flow not independently re-tested after
  the model swap.** Full detail: `docs/crosslight/claude-features.md`.
- **Compare Translations — two UX fixes, built, both builds green, not independently hardware-tested
  this session (lower risk — pure input-handling change, no network):**
  1. Page-turn buttons now page the comparison text (they were silently jumping chapters instead,
     because the activity bound raw Up/Down instead of the `PageBack`/`PageForward` logical buttons
     everything else in the reader uses — same physical GPIOs, wrong logical binding). Long-press
     (1000ms) on either page-turn button now changes chapter instead.
  2. Changing chapter no longer resets verse to 1 — it keeps your verse number, clamped to the new
     chapter's length (`loadChapters()` already clamped; just stopped overwriting it first).
  3. Checked whether a translation missing a chapter was silently mishandled — it wasn't; the existing
     per-verse `present=false` → "(not in this translation)" fallback already covers it correctly.

**Pre-existing state below is unchanged from 2026-10-04** (offensive quarantine, 26.10.7 release, 6a
deauth stash-only) — still accurate, not re-verified this session.

**Working tree clean, everything pushed** (`fork/crosslight` + `fork/develop` + the simulator fork). Latest
firmware commit `b9a73b40`. **Latest release: 26.10.7** (https://github.com/flyboy-byte/crosslight/releases/tag/26.10.7).

**THE BIG PIVOT THIS SESSION — offensive tools paused & quarantined.** Logan's call (2026-10-04): stop active
development of the Wi-Fi/BLE *hacking* tools. The Utilities menu now shows only the two genuinely general-use
radio utilities — **Wi-Fi Analyzer** (was Wi-Fi Scan) and **Bluetooth Scanner** (was BLE Scan), both with
vendor labels. Everything offensive (beacon flood, evil twin, BLE spoof, PCAP capture, PMKID harvest, Wi-Fi
threat *detection*, and the Flock/camera scanner) moved to **`src/offensive/`**, stays **compiled but hidden**,
and only appears in the menu if the SD card holds an empty flag file **`/offensive/enabled`** (read once per
boot by `UtilityRegistry`). Full reference: **`docs/crosslight/offensive/`** (renamed from `pentest/`). The
in-tree pointer is `src/offensive/README.md`.

**What shipped in 26.10.6 / 26.10.7 (flash 26.10.7, it supersedes both):**
- **26.10.6 — Wi-Fi radio-init fix.** Promiscuous bring-up was `WiFi.mode(WIFI_MODE_NULL)`, which no-ops when
  Wi-Fi is already off → `esp_wifi` uninitialized → `WIFI_NOT_INIT` → "Radio unavailable" on fresh boot.
  Changed the three passive scanners to `WIFI_MODE_STA` (inits+starts, never associates). **Diagnosed from
  the device serial log**, not guessed. [[verify-dont-assume]]
- **26.10.7 — offensive quarantine** (above) **+ upstream rebase** (18 commits: per-book content-key/plugin
  isolation, SD-font kerning, low-power/logging fixes, Metalio device support). Host tests **591/591**,
  x4pro + simulator build clean. Sim fork needed 4 new HAL/Arduino stubs (HalHaptics, capacitive-page getters,
  `String::toLowerCase`, NVS-blob + `esp_fill_random`) — committed+pushed to the sim repo.

**HARDWARE-VERIFIED 2026-10-03 (serial-confirmed on the X4 Pro):** **Bluetooth Scanner + vendor labeling
work** — BLE scan loads 7 signatures, opens the 4,041-entry company-id DB, matches live advertisers. Wi-Fi
promiscuous was the bug above (fixed in 26.10.6, not yet re-flashed). Everything through 26.10.3 hardware-tested
earlier (passive scans, full Bible Phases 1-4, utilities).

**Waiting on Logan (whenever — he's wrapping up):** flash **26.10.7** and sanity-check. **Wi-Fi Analyzer** is in
the menu and uses the fixed `ApScanner`, so it verifies the radio fix without any flag. To see the hidden
offensive tools (and re-test Threats/PMKID/Capture), create `/offensive/enabled` on the SD + reboot; those also
need `/wifiaudit/` (created) and `/flock/signatures.json` (placeholders copied) on the card.

**6a targeted DEAUTH — still NOT in the tree.** In `git stash` (`stash@{0}`), never compiled; Claude's safety
classifier blocked the build and it was not routed around. It is the one offensive tool kept stash-only (its
esp_wifi linker-wrap is NOT in `platformio.ini`). Codex's/Logan's part — see `docs/crosslight/offensive/STATUS.md`.

**Still open (nothing queued — ask Logan before starting any):**
- **Claude Panel (v1 scope DECIDED 2026-10-05; Phases 0+1 BUILT 2026-10-05, x4pro + sim both green, NOT run on hardware; release HELD until SD token is copied):** usage-only Utilities tile, 5h+7d bars,
  manual/30s toggle (defaults manual), stale-data status line. Direct device→Anthropic, token on SD, native e-ink,
  foreground-only refresh; later MCP/remote actions. Full plan: `docs/crosslight/claude-panel.md`.
- **Bible Passage Q&A + Ask Claude utility (ALL PHASES BUILT 2026-10-05, x4pro+sim green, NOT run on
  hardware):** in-reader passage Q&A (Confirm menu, preset+free-text questions, current-page verses by
  default) and a standalone free-prompt Utilities tile, both on the same subscription-token path as
  Claude Panel. SD-logged (not re-sent) history for Ask Claude is built too (`ClaudeHistoryStore`).
  Full plan: `docs/crosslight/claude-features.md`.
- **Bible Phase 5 remaining:** repeated-word/phrase search, Hebrew/Greek number metadata, Geneva-style margin
  notes. None designed yet.
- **Bible Track D (reader perf):** measure-first — needs a `millis()` profiling pass on the device.
- **Other firmware debt:** wallpaper tool host-validated only; getBible downloader scoped-not-built; Bible
  full-text search perf-gated; radio power policy; SD-font render speed unmeasured; keep merging upstream.

**SD card (mounted at `/run/media/logan/1096-66DD` this session):** `/Bible/numbers/` (7 Numbers),
`/bleaudit/signatures.json`, `/vendordb/oui.bin`+`btcid.bin` (regen via `scripts/gen_vendor_db.py` — see
`docs/crosslight/offensive/TOOLING.md`), KJV+ASV+NIV, plus `/wifiaudit/` and `/flock/signatures.json` added
2026-10-03. The `.bin`s aren't committed (regenerable); release `.bin`s are gitignored (`/crosslight-*-x4pro.bin`).

---

Status: **last updated 2026-10-01.** X4 Pro (UC8279 panel) runs CrossLight; stock is backed up and verified. **Released: 26.10.1 is published on GitHub** (https://github.com/flyboy-byte/crosslight/releases/tag/26.10.1) — the upstream rebase + passive pentest toolkit (slices 1-5, receive-only), over Wi-Fi OTA. **Tested on hardware 2026-10-01: UI good (time-at-top + look both validated by Logan), scans run; BLE
matching is SD-data-gated** (needs `/bleaudit/signatures.json` copied to the card — see the pentest section).
A **calculator fix landed after 26.10.1** (operator now shows on the display — commit `29b5b15b`, rides the
next release, not in 26.10.1). The slice-6 active foundation rides along **dormant** (flag off, no UI, no-op TX). **Prior: 26.9.3** (https://github.com/flyboy-byte/crosslight/releases/tag/26.9.3) — Bible Numbers v1. **26.9.2 is also published and written to the SD card as `/firmware.bin`, still awaiting a wired/SD install** via Settings → SD Card Firmware Update (Logan stopped before installing it) — 26.9.3 supersedes it for anyone already on Wi-Fi OTA, but a device still on pre-26.9.2 needs 26.9.2 installed first to reach the Wi-Fi-OTA track at all. It contains items 1-13 below plus the calculator, the startup password, the Cover Grid fix, the hotspot-QR fix, fork-pointed OTA, and the 2026-09-24 upstream merge. **`crosslight` rebased onto upstream `develop` again 2026-10-01** (29 commits: SD-card plugin system, EPUB DRM, reader refactors) — not yet in a tagged release. See "Upstream rebase (2026-10-01)" below.

**Wallpaper converter added 2026-09-25** (`scripts/make_wallpaper.py`, host-side, no firmware change): image → sleep-screen BMP. Dithers to the panel's 4 native gray levels (0/85/170/255) so the firmware's `nativePalette` fast path renders it pixel-for-pixel; portrait 480x800; `--mode gray4|bw`, `--fit cover|contain`, `--gamma` (~0.65 for the reflective panel), `--brightness`. Six personal wallpapers built into `wallpapers/` (git-ignored — album art). **Still needs a real on-device check** (host-validated only; a device photo Logan shared was a stock image, not a tool output). See [[crosslight-wallpaper-tool]].

**Item 15 (web server won't load on phone) — RESOLVED 2026-09-30, not a firmware bug.** The page loads fine in Chromium (laptop) and Vanadium (phone); the earlier `ERR_TOO_MANY_RETRIES` was **Brave-specific** (its parallel/speculative sockets starve the single-connection Arduino `WebServer`). No code change. Optional someday-maybe hardening (async server) noted in **"Item 15"** below.

**Bible expansion update, "everything, phased" (chosen 2026-09-30): Phases 1-3 SHIPPED.** Driven by a
ChatGPT research handoff (`docs/crosslight_claude_handoff.md`), audited and corrected against the real
code. Organizing idea: rebuild **the apparatus early English study Bibles actually shipped with** — the
**Geneva Bible (1560)** as the first English study Bible, and the **1611 KJV front-matter almanac**
(confirmed against Logan's own 1611 facsimile: it's genuinely two features, Easter/computus AND a yearly
reading calendar). **Phase 1 (audit) and Phase 2 (NIV)** are done — NIV is on the device SD, no firmware
change needed. **Phase 3 (Bible Numbers v1) shipped as OTA 26.9.3** — 7/12/40/666, facts-first, every claim
classified. **"Go to Verse" was removed 2026-09-30** (Logan's call). Remaining: **Phase 4 (Historical
Calendar, Track C — computus + reading calendar)** and **Track D (reader perf, measure-first)**. Full plan,
audit, historical framing, and phasing in **"Planned update: Bible expansion"** below.

**Item 14, the Flock camera scanner, DEPRIORITIZED 2026-09-30** — real-world signal reliability (modern
units reportedly disabled Bluetooth and moved to cellular backhaul, leaving Wi-Fi dormant most of the time)
is the actual limiter, not the code. Code is built, host-tested, device-build-unblocked, architecture
confirmed sound — left as-is, not resumed without new information. See "Item 14: Camera scan" below.

**Pentest/security toolkit: passive slices 1-5 MERGED 2026-10-01 (not yet flash-verified).** Logan's own
hardware, personal security-research hobby (Extra-class ham, understands the legal boundaries). Five passive
(receive-only) tools now in `crosslight` via PR #1: Wi-Fi AP scanner, evil-twin/deauth-flood detection, PCAP
capture to SD, EAPOL/PMKID + hashcat export, passive BLE scan/fingerprint — all host-tested (529/529),
simulator + x4pro device builds green (flash 54.0%). **Still owed: real-hardware verification before any OTA
tag.** Active/transmit tools (deauth/beacon-flood/evil-twin/BLE-spoof, Slice 6) are scoped + scaffolded on an
unmerged branch, gated behind a compile flag (off in releases). Also still planned: flashlight toggle, unit
converter, nicer calculator. Full detail in **"Pentest/security toolkit — PR #1 review"** and **"Planned:
Pentest/security toolkit"** below.

The 26.9.x release/OTA machinery is new: updates now check `flyboy-byte/crosslight`, and from 26.9.2 on they install over Wi-Fi. Version line is `[crosslight] version` (scheme YY.M.BUILD). See "Releasing CrossLight (Wi-Fi OTA)". **26.9.3 (2026-09-30): flash 49.4% of a 7.94MiB slot; 408/408 host tests pass** (the Flock-blocked count from 26.9.2 is now resolved — see the flock namespace fix below). Use a USB-A-to-C cable, not C-to-C. The SD card mounts as a real `mmcblk0` reader when out of the device; in the device, use USB Drive mode.

**Logan's request list (2026-09-21) — every item he asked for, with status. Keep this current:**

| # | Request | Status |
| --- | --- | --- |
| 1 | Fonts to SD to free flash for utilities | **Done, on device 2026-09-21** (SD Noto families loaded at boot; switching families/sizes verified in the device log). `-DOMIT_FONTS` in `[env:x4pro]` (flash 83.8% → 58.6%). SD families `/.fonts/Noto Serif/` + `/.fonts/Noto Sans/` (12-18, from the built-in source TTFs, `--intervals builtin`). Under `OMIT_FONTS`: built-in sizes shrink to {14}; `getReaderFontId()` only ever returns Noto Serif 14 (a missing id rendered blank pages); boot maps a built-in family choice to the same-named SD family (`SdCardFontSystem::begin`); the Font Family list hides built-ins the SD families replace. Next device session: copy both families, delete the `NotoSansSD` test family, flash, verify |
| 2 | Local (PC) tool to prep any image as a wallpaper | **Built:** `scripts/make_wallpaper.py` (0.4s/image; sim sleep screen renders it pixel-identical to `--preview`). New 1-bit Sabaton ready for the card. Was: Planned. Pre-dither to **1-bit** 480×800 on the PC: this UC8279 unit renders sleep images 1-bit, and 4-gray input gets thresholded (posterized) rather than dithered — firmware only error-diffuses high-color BMPs. Also re-do the Sabaton `/sleep.bmp` this way |
| 3 | Wallpaper options from the file browser | **Built, sim-verified (all 3 actions + Delete hand-off), flashed 2026-09-21.** `ChoiceActivity` (N-option modal) + `util/Wallpaper` (set → `/sleep.bmp`; add → `/.sleep/`, moving an existing `/sleep.bmp` in rather than deleting it); both switch Sleep Screen to Custom. Was: Planned. Long-press on an image → Set as sleep screen / Add to sleep rotation (`/.sleep/`) / Delete (long-press is delete-only today). The image viewer's "Set sleep cover" exists but is Confirm-only and its hint is hidden on touch boards — unreachable on the X4 Pro |
| 4 | Bible: download preset English translations over WiFi | **Built and working on device (2026-09-22):** ASV (8.6MB) downloaded in ~60s. First try looked frozen: getBible replies chunked with no Content-Length and `HttpDownloader` only called the progress callback when the total was known, so the screen sat at 0% and Back/Home couldn't cancel. Fixed by always reporting progress (other callers already guard total==0) and showing a KB count when the size is unknown. Logan finds getBible's English list weak (mostly pre-1900 public-domain); decided to leave it. If revisited: BSB (public domain since 2023 — verify first) / LSV via a PC-side converter into the same JSON layout, no firmware change needed. Bible hub off Home (Continue/Select Book/Go to Verse/Search/Bookmarks/Translations); Translations screen (installed, then the 12 getBible English presets with license); `BibleDownloadActivity` (OTA-style: Wi-Fi picker, `.part` then rename, Back cancels, selects on success, silent restart on exit). Translation choice saved in `bible_state.json`; files at `/Bible/<ABBR>/<abbr>.json`. Was: Planned. Infra exists (`HttpDownloader`, WiFi picker, chapter cache rebuilds per file). Offer public-domain English versions only, show license before download (see translation-downloader scoping below) |
| 5 | Bible full-text search | **Built:** `searchCache()` — one linear pass over the chapter cache, case-insensitive, 100-hit cap, UTF-8-safe snippets; host run over full KJV 175ms (device time not measured yet). Results list jumps to the verse. Was: Planned; cheaper than scoped. Search the chapter cache (plain text, 4.25MB) instead of the JSON; optionally keep it in PSRAM for near-instant repeat searches. Measure the cold SD pass first |
| 6 | Bible: center tap sometimes doesn't open the menu | **Fixed, flashed 2026-09-21 (sim-verified top and bottom of the column):** the reader menu tap only accepts the center *ninth*; taps above/below it in the center column hit no zone. The Bible now takes the whole center column. Was: To investigate with the serial logger (did the tap register, where did it land vs. the center-third zone) |
| 7 | Lag when page-turn taps queue up | **Investigated, not changed:** render requests already coalesce (`ulTaskNotifyTake(pdTRUE)`), so N taps during a refresh cost one catch-up refresh, not N. Remaining idea: skip the grayscale AA pass when another turn is pending — touches panel-state subtleties, needs on-device iteration. Meanwhile Text Anti-Aliasing off is the lever. Was: To investigate: skip queued intermediate pages and render only the final one. Baseline: ~1.36s/turn, ~1.0s of it panel |
| 8 | Home screen: select with buttons, not only touch | Existing setting: Settings → Controls → Short Power Button Click → Confirm (or Home key tap → Confirm). Consider making it the X4 Pro default |
| 9 | WiFi not set up | No code needed: the flash erase removed stock's saved network; add it once in Settings → System → Wi-Fi Networks |
| 10 | (Found along the way) Bible text uses the UI font (Ubuntu 10), not the reader font/size | Noted; offer the reader font later if wanted |
| 12 | (Found along the way) WEB and other translations carry paragraph indents/double spaces | **Fixed:** verse whitespace normalized in cache and JSON paths (cache format v2 — old caches rebuild once) |
| 11 | Feature architecture: many utilities without hurting battery/speed | **In progress. Partition bump done 2026-09-24.** Nothing mounted the 3.375MiB `spiffs` region (all user data is on SD), so it was split between the two app slots: 0x640000 -> 0x7F0000 each, 6.25 -> 7.94MiB, ending exactly at the 16MB chip boundary with no gaps. Flash went 59.1% -> 46.7% of a slot, ~4.25MiB free. Both OTA slots kept deliberately — they back the SD-card `FirmwareFlasher` path as well as Wi-Fi OTA, and rollback matters on a device whose only port is the magnetic pogo connector; a single-slot layout would buy ~3.4MiB more that we don't need. Needs a **wired** flash, not an OTA update, since the layout moves. **The seam is built (2026-09-24):** Home has one **Utilities** entry backed by a registry (`src/utilities/UtilityRegistry.cpp`) — a new tool is one include + one `kUtilities` line, so Home (an upstream-hot file) never changes again; tools construct on open, destroy on exit, nothing runs in the background. Tiles so far: **Calculator** (done, in 26.9.2), **Startup Password** (done, in 26.9.2), **Camera Scan** (item 14, in progress). Still open: radio power management (the download and scan both bring Wi-Fi up on entry and tear it down on exit; no shared policy yet). PSRAM ~8.2MB unused; internal RAM ~200KB free |
| 13 | (Asked 2026-09-24) Bible: the Prep Class memory-work booklet as part of the app | **Built, simulator-verified, not yet flashed.** Bible hub -> Memory Work -> lesson list -> a paged reading page per lesson. Courses are *reference* lists (`/Bible/memory/<name>.json`: book/chapter/verse, optional `end`), so the words come from the installed translation via the chapter cache — a few KB on SD, ~0 flash, follows the selected translation, and a second booklet is a file drop rather than a firmware change. Three item shapes preserve the booklet's structure: verse, section heading, free-text note. Source lives at `assets/memory/prep-class.json` (Theme Verse + Lessons 1-8, 40 verses; every reference checked to resolve against the real KJV). Logan chose: no progress/quiz tracking, current translation rather than pinned KJV. **Done on device: `prep-class.json` is on the card at `/Bible/memory/`; ships in 26.9.2.** |
| — | (Asked 2026-09-24) Startup password | **Built, simulator-verified, in 26.9.2.** Settings → System → Startup Password: set/change/remove, optional lock-on-wake. `DeviceLock` store (`/.crosspoint/lock.json`, salted SHA-256, kept out of settings.json). `LockScreenActivity` stands in front of boot routing (main hands it a `routeAfterBoot` lambda captured **by value** — a by-ref capture dangled, since it runs after setup() returns). Back can't escape; changing/removing asks for the current passphrase; silent reboots skip it. It's a screen lock, not encryption — removable FAT card. |
| — | (Asked 2026-09-24) Ditch Calibre wireless | **Declined by Logan** ("just leave it then… it pissed me off"). Not removed. ~287 lines, single-digit KB; the saving wasn't worth the upstream-merge cost. |
| 14 | (Asked 2026-09-24) Flock/surveillance camera scanner | **In progress — logic tested (13/13 host), firmware build BLOCKED by the safety classifier, never compiled or flashed.** See "Item 14: Camera scan" below. |

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

- **2026-09-24** (`0b6bb004..637ad4a5`, 31 commits): three conflicts, all resolved by keeping both
  sides. (1) `english.yaml` — both sides appended strings. (2)+(3) `FileBrowserActivity.{h,cpp}` —
  upstream replaced long-press-to-delete with a proper **entry-actions popup** (Open/Delete/Rename,
  new `showEntryActions`/`deleteSelected`/`startRename`). Our `.bmp` long-press menu (a
  `ChoiceActivity` + `imageDeleteChosen` flag) existed *only* because long-press was delete-only, so
  upstream's structure is strictly better: took it, and moved the wallpaper actions into its popup
  (image rows now read Open / Set as sleep screen / Add to sleep rotation / Delete / Rename).
  Dropped `activateSelected(forceDelete)` and `imageDeleteChosen` entirely — our fork got *smaller*.
  **Three non-conflict breaks cost more time than the conflicts:** the `freeink-sdk` submodule was
  bumped and the build dies with "Can not create a symbolic link ... not a directory" until `git
  submodule update --init` runs; the simulator needed `FreeInkFont` + `MemoryManager` added to its
  `lib_deps` in the **gitignored** `platformio.local.ini` (so this is not recorded in git anywhere but
  here); and the sim fork needed `HalMemory::PsramBuffer`/`allocatePsram` (cover-grid
  `HomeCoverCache` holds one by value), an `esp_heap_caps.h` host shim, and a `StaticTask_t` stub —
  because the new TrueType-on-PSRAM font path and the SDK's `MemoryManager` call `heap_caps_*`
  **directly, not through the HAL**, which is a new class of sim break to expect again. Notable
  upstream content: TrueType fonts on PSRAM boards (#3646), a Cover Grid home theme for PSRAM devices
  (#3657), word/character spacing controls (#3528), an X4 Pro frontlight double-click fix plus a Home
  key option (#3089), better footnote navigation (#3682), and RTL tap zones (#3709). Flash 46.7% ->
  49.0% of a slot. Host tests 357 -> 388, all passing.
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

## Releasing CrossLight (Wi-Fi OTA)

The device checks **our** releases, not upstream's: `-DCROSSPOINT_OTA_REPO` /
`-DCROSSPOINT_OTA_ASSET_PREFIX` in `[env:x4pro]`. Left at upstream's defaults, an upstream release
would be offered and would replace CrossLight with stock CrossPoint (no Bible app, Memory Work or
Utilities) — that is why these flags exist.

To cut a release:

1. Bump `[crosslight] version` in `platformio.ini`. Scheme **YY.M.BUILD** (`26.9.1` -> `26.9.2` in the
   same month, then `26.10.1`, then `27.1.1`). It must *increase numerically*, because the updater
   compares with `sscanf("%d.%d.%d")` — a bare date could not ship twice in one day, and a leading
   `v` on the tag would fail to parse.
2. `pio run -e x4pro`, then copy `.pio/build/x4pro/firmware.bin` to **`crosslight-<version>-x4pro.bin`**
   (the `x4pro` suffix is the board tag from `FREEINK_DEVICE_X4PRO`; the updater refuses an image whose
   embedded tag names another board).
3. `gh release create <version> <that file> --repo flyboy-byte/crosslight` — the tag must equal the
   version exactly, since the asset name is built from the tag.
4. Verify: `curl -s .../releases/latest` and check the asset name matches `crosslight-<tag>-x4pro.bin`.

**A firmware that changes `partitions.csv` cannot ship this way** — OTA and the SD-card flasher write
the app slot only, never the partition table. That one needs a wired flash.

First release: **26.9.1 (2026-09-24)**, which had to be installed by SD/USB because the firmware then
on the device still pointed at upstream's feed.

## Item 14: Camera scan (Flock/surveillance-device detector) — IN PROGRESS

**State (2026-09-24 evening): all code written, pure logic host-tested (13/13), but the firmware
compile was BLOCKED by the auto-mode safety classifier — it has never been built for the device or
flashed.** The classifier stopped the first `pio run -e x4pro` that would compile the new Wi-Fi
monitor-mode code. That is a reasonable pause (raw 802.11 sniffing), and per its rules I did not route
around it. To resume, the build has to be run with permission — Logan can run `!pio run -e x4pro`
himself, or allow-list the build.

**What it is, and the scope line.** A *passive, receive-only* scanner: it puts the radio in
promiscuous mode, channel-hops 1-13, and matches 802.11 management frames (beacon / probe req / probe
resp) against a signature list. It never associates, transmits, deauths, or touches any network — it
reads what is already broadcast into the air. That keeps it cleanly on the privacy/awareness side of
the security-tools bucket. The offensive tiles from the old Biscuit list (deauth, captive portal, AP
cloning) are explicitly NOT part of this and should be a separate, deliberate scope conversation with
Logan before any are built — fine on his own gear, but not lumped in here.

**Design (why it's shaped this way).** The risky parts are pure functions, host-tested, so nothing
untested runs in the radio callback:
- `src/flock/FlockSignature.h` — model (`Signature`, `Observation`) + declarations.
- `src/flock/FlockMatcher.cpp` — `match()` (OUI + case-insensitive SSID; a signature matches on
  *either* criterion because vendors randomize one field; a blank signature matches nothing) and
  `ouiOf()`. Pure, tested.
- `src/flock/FlockFrame.cpp` — `parseManagementFrame()` (source MAC at offset 10; SSID element walk,
  with the fixed 12-byte body for beacon/probe-resp and none for probe-req; truncated-element guard
  against overread). Pure, tested.
- `src/flock/FlockSignatures.cpp` — JSON load from `/flock/signatures.json` (ArduinoJson,
  firmware-only, so kept out of the host-tested files).
- `src/flock/FlockScanner.{h,cpp}` — the radio wrapper, **guarded `#if defined(ARDUINO_ARCH_ESP32)`**
  so the sim still links (`begin()` returns false → UI shows "radio unavailable"). The promiscuous
  callback runs in the WiFi task and does the minimum — copy the raw frame + RSSI into a fixed
  FreeRTOS queue, no allocation; `drain()` on the UI task runs the *tested* parse+match and dedups by
  MAC (keeps closest RSSI, counts frames, caps at 64).
- `src/activities/utilities/CameraScanActivity.{h,cpp}` — the screen; `preventAutoSleep()`, hops every
  ~300ms, Back stops and leaves; states NoSignatures / NoRadio / Scanning.
- Registered in `UtilityRegistry.cpp` as the second tile (Wifi icon).
- `assets/flock/signatures.json` — a **placeholder** template (OUIs are `00:00:00`, match nothing).
  **Deliberate: I would not fabricate real Flock OUIs** — no verified current data, and fake values
  give false confidence. The engine is real; the fingerprints are the user's to fill in from current
  research, which is why they live on SD (Flock rotates them). Copy to `/flock/signatures.json`.

**Architecture review — DONE 2026-09-30, design confirmed sound.** Checked whether promiscuous-mode frame
sniffing + OUI/SSID matching is even the right method (Logan's question). Public research converges on
exactly this method (`colonelpanichacks/flock-you` and several independent forks — all passive, OUI +
SSID-pattern matching on captured 802.11 frames, no transmission). One real finding worth acting on:
researchers report these devices moved from AP/beacon mode to **station-mode wildcard probe requests**
(empty SSID, ~125ms interval) around December 2025 — meaning SSID-substring matching alone would
increasingly miss them. **Checked against our own code (verified by reading the source, not fetched
data): `FlockFrame.cpp` already parses Probe Request (subtype `0x40`) correctly, extracts the source MAC
regardless of frame subtype, and the matcher already has a tested empty-SSID/OUI-only path
(`EmptySsidObservationDoesNotMatchSsidRule`).** So the architecture is already correct for this case — no
code change needed on the detection method itself.

**ITEM 14 DEPRIORITIZED 2026-09-30 — real-world signal reliability is the actual problem, not the code.**
Logan supplied outside research (not from me) showing modern Flock units have **disabled Bluetooth** (the
signal early detectors relied on most) and moved to **cellular/LTE backhaul**, leaving Wi-Fi **dormant most
of the time** — detection now depends on catching faint, sporadic probe requests, which the source material
says is unreliable even for dedicated drive-by detector hardware. **Decision: stop investing further time
in signature research; leave the code as-is (built, host-tested, device-build-unblocked, architecture
sound) and do not resume unless something changes** (better data surfaces, or Logan wants it anyway knowing
the odds). Caveat worth remembering if resumed: CrossLight's use case (a device carried/sitting with someone
for a while) has more dwell time to catch an intermittent signal than a car passing at speed, which is the
scenario most of the critique is written about — so "unreliable for drive-by scanning" isn't necessarily
"unreliable for CrossLight," but neither of us has evidence either way, so treat it as unproven, not better.

**Signature data research findings, preserved for if this resumes (NOT written into `signatures.json` —
work stopped here per the deprioritization above):**
- Verified `b4:1e:52` is IEEE-registered directly to **Flock Safety** (cross-checked against a third-party
  MAC vendor database independent of the community research project) — real, strong, checkable ground
  truth, not a heuristic guess.
- The community "32 field-researched OUI prefixes" (attributed to one researcher, `@NitekryDPaul`, synced
  2026-07-16, methodology stated as promiscuous-mode traffic analysis) reproduced **identically** across two
  independently-fetched repos (the original project's own dataset doc, and a downstream fork's compiled
  source) for the 10+1 prefixes checked — consistent propagation, not independent re-derivation (all
  downstream copies trace to the same one researcher), and the source itself flags 2 of the 32 as
  "low confidence" and 1 as unusual ("locally administered" MAC). Spot-checked 2 of these against the
  third-party vendor database: both resolve to real registered vendors (Liteon Technology Corp for one),
  consistent with "Flock uses this vendor's radio module" rather than "Flock owns this block" — a weaker
  but still real class of evidence than the direct IEEE registration above.
- A **different fork's source file also contained a 10-entry "FS Ext Battery devices" group including
  `cc:cc:cc`** — not present in the original canonical source and shaped like a leftover placeholder value,
  not a real finding. **Correctly excluded** — concrete proof the "verify before shipping" caution was the
  right call, not just theoretical risk-aversion.
- A single `WebFetch` of a different fork's README (before the above direct-source checks) returned a
  plausible-looking but unverified 34-prefix list with a "firmware dump" narrative — **this was never
  written anywhere**, specifically because a page-fetch summary isn't verification for safety-relevant data.

**Host/sim build blocker — FIXED 2026-09-30.** The `flock` **namespace** collided with POSIX `struct flock`
from `<fcntl.h>` (pulled in by `HalStorage.h` via `FsApiConstants.h` on host builds), so `src/flock/*` and
`CameraScanActivity.o` failed to compile on `-e simulator_x4_pro`. **Fix: renamed the namespace `flock`
→ `flockcam`** (`"flock"`/`"FLOCK"` SSID-match *string literals* left unchanged). Simulator now builds
SUCCESS; `FlockMatcher` host tests pass 8/8. **The scanner is now simulator-testable.** NOTE: this does not
unblock the *on-device* firmware build — that remains gated by the auto-mode safety classifier (needs
`!pio run -e x4pro` with permission). But the UI/logic can now be exercised in the sim.
- Tests: `test/flock_matcher/` — 13 tests, all passing (matcher + frame parser).

**When the build is unblocked, remaining work:**
1. `pio run -e x4pro` — confirm the ESP32 radio calls compile (this is the only unverified part of the
   code; the ESP-IDF promiscuous API was confirmed present in `esp_wifi.h`).
2. `pio run -e simulator_x4_pro` and screenshot the NoSignatures / NoRadio / Scanning shell (the sim
   has no radio, so the scan itself is not sim-testable).
3. Run the full host suite (expect 401: 388 + 13).
4. **On-device, serial-log:** does the scan see frames and match? And the key open question —
   **does normal Wi-Fi work again after a scan without a reboot?** The scanner does a clean
   `esp_wifi_set_promiscuous(false)` + `WiFi.mode(NULL)` on exit rather than a `silentRestart()`
   (which the Bible downloader uses for TLS teardown). If OTA/downloads misbehave after a scan, add a
   `silentRestart()` to `CameraScanActivity::onExit()`.
5. Populate real signatures before it's useful (currently matches nothing).

## Item 15: File-transfer web server won't load on the phone — RESOLVED 2026-09-30 (not a firmware bug)

**Resolution (Logan tested 2026-09-30):** the server loads fine. **Chromium on the laptop worked, and
Vanadium on the phone worked.** The device joins the SoftAP, serves the page, and the HTTP path is healthy.
So the earlier `ERR_TOO_MANY_RETRIES` was **browser-specific, not a server defect** — consistent with the
original H6 suspicion (Brave's aggressive preconnect / speculative sockets + Shields overwhelming the
single-connection Arduino `WebServer`, which can only service one socket at a time). **No code change
required.** Association (H1/H2/H3) and the serving layer (H4/H5) are all ruled out by a working load.

Optional, low-priority hardening *if Brave-class browsers ever matter:* the single-connection `WebServer`
is the real fragility — a client that opens parallel/speculative sockets can starve it. A more robust async
server (e.g. `ESPAsyncWebServer`) would tolerate that, but it's a real dependency swap for a browser almost
nobody uses here. **Not worth doing now** — filed as a someday-maybe, not a bug. The diagnosis notes below
are kept for history.

---

**Original symptom (Logan, repeated; refined 2026-09-30):** open the file-transfer server in **AP/hotspot mode**,
connect the phone to the `CrossPoint-Reader` Wi-Fi, then browsing to it **just sits and never loads** —
tried both `crosspoint.local` (mDNS) *and* the raw IP. The earlier hotspot-QR fix (encode `http://<ip>/`
instead of `crosspoint.local`, shipped in 26.9.2) did **not** fix it. **Key new detail: the phone itself
reports "too many attempts" (or similar) while it sits** — i.e. this reads as a **Wi-Fi association/auth
failure on the phone side, not an HTTP failure.** The phone likely never fully joins the SoftAP, so the
web server is a red herring — the fix is almost certainly in `startAccessPoint()`, not the HTTP code.
This bumps H1/H3 below to the top and makes H4/H5 (serving layer) unlikely.

**What the code actually does (read 2026-09-25, all looks correct):**
- `CrossPointWebServerActivity::startAccessPoint()` — `WiFi.mode(WIFI_AP)`, open network (`AP_PASSWORD =
  nullptr`), SSID `CrossPoint-Reader`, **channel 1**, max 4 clients, default SoftAP IP **192.168.4.1**.
  Then a captive-portal `DNSServer` (`*` → apIP) and mDNS `crosspoint`.
- `CrossPointWebServer::begin()` — `WebServer` on **:80**, `WiFi.setSleep(false)`, routes registered,
  WebSocket on :81, discovery UDP. `handleRoot()` serves the gzipped HomePage via `send_P` **from
  flash** (so serving is low-heap and shouldn't OOM).
- `handleNotFound()` in AP mode **302-redirects every non-`/api/` path to `/`** (captive-portal auto-open).
- The activity `loop()` pumps `dnsServer->processNextRequest()` + a tight `handleClient()` loop.

**The diagnosis blocker:** essentially all the web-server logging is `LOG_DBG` (`WEB`/`WEBACT`), which is
**compiled out at `LOG_LEVEL=1`** — and the release/RC x4pro builds set `LOG_LEVEL=1` (platformio.ini).
So a serial capture on the shipped 26.9.2 shows almost nothing. **Step 1 is to flash a debug build
(`LOG_LEVEL=2`)** so `WEB`/`WEBACT` lines appear, then reproduce with the serial monitor
(`scripts/debugging_monitor.py`).

**The decisive test — this cleanly bisects it.** With a debug build + serial monitor, start the server
in AP mode, connect the phone, and load the page while watching for:
- `Access Point started! IP: 192.168.4.1` and `Web server started on port 80` (confirms bring-up), then
- `handleClient active...` every 10s (confirms the pump runs), and crucially
- on page-load: does **`Served root page`** (or any DNS/302 line) appear?
  - **Nothing logs on page-load → the request never reaches the device** = network/association layer
    (H1/H2 below). This is the most likely branch given "times out on the IP too."
  - **`Served root page` logs but the phone still times out → serving layer** (H4/H5): gzip/`send_P`
    stall or the response not completing over the link.

**Ranked hypotheses:**
- **H1 — phone gets no DHCP lease on the SoftAP** (stuck "obtaining IP address"), so 192.168.4.1 is
  unreachable. Common ESP32 SoftAP + Android failure. Check the phone's assigned IP (expect 192.168.4.x)
  and try to `ping 192.168.4.1`.
- **H2 — Android "no-internet" routing.** Open AP with no internet: some Android builds keep routing
  over cellular and/or the captive-portal webview hijacks the flow. Mitigation to test: turn **mobile
  data off** after joining, then load the IP in real Chrome (not the captive-portal popup).
- **H3 — RF/channel.** SoftAP is hard-pinned to **channel 1**; a busy channel or a phone that parked on
  5 GHz can make association flaky. Low-cost experiment: try a different `AP_CHANNEL`.
- **H4 — serving stall/heap.** Free heap after the SD-font-cache release could be low; watch the
  `[MEM] Free heap` lines around `begin()`; if `Served root page` logs but nothing arrives, suspect this.
- **H5 — gzip vs a limited webview.** `sendStaticContent()` always sets `Content-Encoding: gzip`; a
  compliant browser is fine, but Android's captive-portal mini-browser might not be — hence "load the
  raw IP in full Chrome" as the clean repro, bypassing the captive popup.

**Cheap product-side hardening worth doing regardless (once the branch is known):** consider giving the
SoftAP a WPA2 password (some phones treat open "no-internet" APs badly), and/or `WiFi.softAPConfig()`
with an explicit IP/subnet + confirming the DHCP lease range. Do **not** change these blind — get the
serial log first so we fix the actual branch, not a guess.

**STA-mode note:** it's unconfirmed whether the same failure happens on home Wi-Fi (STA) — if STA works
and only AP fails, that strongly implicates H1/H2/H3 (the SoftAP path) and narrows the fix.

**Screenshot evidence (2026-09-30):** phone browser at `http://10.107.92.6/` shows Chromium
**`ERR_TOO_MANY_RETRIES`**, and the status bar shows **LTE with no Wi-Fi icon — the phone is on mobile
data, not joined to any Wi-Fi.** Also `10.107.92.6` is **not** the SoftAP IP (`192.168.4.1`), so that shot
is a "join a network"/STA address, not hotspot. Two takeaways: (a) in hotspot mode the phone is very likely
**never associating** to `CrossPoint-Reader` (matches the "too many attempts" report → association layer,
H1/H3); (b) any STA-mode attempt fails if the phone is on LTE instead of the same Wi-Fi as the device.
**Next real-world check (no firmware): does the phone's Wi-Fi list show `CrossPoint-Reader`, and does
tapping it actually connect?** If it won't connect, the fix is the SoftAP config (WPA2 password / PS-none /
explicit softAPConfig), not the HTTP code. Turning mobile data off and loading `192.168.4.1` is the clean
hotspot repro.

**Association RULED OUT (2026-09-30, Logan tested):** on hotspot the phone **sees `CrossPoint-Reader`,
connects fine**, and with LTE off it *still* shows `ERR_TOO_MANY_RETRIES` on the AP IP (`192.168.4.1`),
identical to the screenshot but with the right IP. So H1/H2/H3 are dead. The TCP port is reachable (a
refused port gives `ERR_CONNECTION_REFUSED`, not RETRIES), so **the server accepts the connection but never
completes a response** → browser retries → gives up. This is a **server-side HTTP bug**, not Wi-Fi.

**New prime suspect (H6): single-connection `WebServer` vs Chromium parallelism.** The Arduino `WebServer`
services one client at a time; Chrome/**Brave** open several parallel + speculative/preconnect sockets on
first load. The ESP can sit holding an empty speculative connection and never service the real `GET /`,
which presents exactly as retries-then-fail in Brave while the port still answers. Candidate fixes if
confirmed: try a different browser (Firefox opens fewer preconnects) as a quick check; set the served
responses to `Connection: close`; or move to an async server (`ESPAsyncWebServer`) — the last is a big
change, so confirm the cause first. **Secondary suspects still worth checking in the log:** handleRoot
never firing (routing) vs firing but the gzipped `send_P` not completing (heap/socket).

**Decisive capture (do this first):** flash a **debug build** (x4pro dev env, `LOG_LEVEL=2`), run
`scripts/debugging_monitor.py`, start the hotspot, connect the phone, load `http://192.168.4.1/`, and read:
- Does **`Served root page`** print? **No →** the request never reaches the handler (routing/servicing —
  supports H6). **Yes but browser still fails →** response isn't completing (gzip/socket/keep-alive).
- Watch the **`WARNING: N ms gap since last handleClient`** lines and the `[MEM] Free heap` values.
Quick zero-flash triage: **try loading it in Firefox** — if Firefox works and Brave doesn't, H6 is all but
confirmed.

**Concrete first moves once a debug build is in hand:** (1) log `WiFi.softAPgetStationNum()` on a timer in
AP mode — if it stays `0` while the phone shows "too many attempts", the phone never associated (proves the
association-layer branch outright). (2) Candidate association fixes to try in `startAccessPoint()`: give the
AP a **WPA2 password** (some phones handle open "no-internet" APs badly and retry-loop), call
`esp_wifi_set_ps(WIFI_PS_NONE)`, and/or set an explicit `WiFi.softAPConfig()` IP/subnet so the DHCP range is
unambiguous. Try these one at a time against the station-count log — do not shotgun them.

## Planned update: Bible expansion — NIV + Bible Numbers + Historical Calendar (planned 2026-09-30)

Origin: a ChatGPT research handoff (`docs/crosslight_claude_handoff.md`) plus Logan's decision to do
**"everything, phased."** The handoff is a research summary, not gospel — the code audit below was done
against the real tree and **corrects two of its claims**. Do not re-trust the handoff over the source.

### Code audit results (verified 2026-09-30 against the real tree)

Confirmed TRUE:
- SD path is `/Bible/<UPPERCASE>/<lowercase>.json` (`BibleTranslations.cpp:58`, `pathFor()`).
- Canonical schema is `{"books":[{"name","chapters":[{"chapter","verses":[{"verse","text"}]}]}]}` — these
  are exactly the keys `BibleChapterLoader` recognizes (`keyFor()`, lines ~95-101).
- Streaming design is real: 16 KB read chunks (`READ_CHUNK_SIZE`), a custom `StreamingJsonParser`, then a
  compact **binary** chapter cache (`CACHE_VERSION = 2`; book table + chapter blobs). **NIV needs no new
  engine** — it reuses this loader/cache.
- Presets exist as `{abbr, name, license}`: kjv, web, asv, ylt, basicenglish, wb, douayrheims, akjv, kjva,
  weymouth, tyndale, wycliffe.

Corrections to the handoff:
1. **Downloader uses `api.getbible.net/v2/<abbr>.json`, NOT api.bible** (`downloadUrl()`,
   `BibleTranslations.cpp:61`). getBible only serves freely-licensed texts, so **NIV cannot use the
   existing download flow** — it must be user-supplied or desktop-converted.
2. **Chapter/verse numbers must be JSON numbers, not strings.** The loader reads them only in
   `onBuildNumber` (`BibleChapterLoader.cpp:436-442`); a string like `"chapter":"23"` (as in
   `aruljohn/Bible-niv`) hits `onBuildString` and is silently ignored → empty chapters. **The converter
   MUST cast chapter/verse to ints.** This is mandatory, not optional.

### Legal boundary (decided with Logan 2026-09-30)

NIV is copyrighted (Biblica). Logan has a personal copy from GitHub (candidates: `aruljohn/Bible-niv`,
`rotarydialer/Sermonator`). **Converting his own copy for his own device = fine (personal use). What we do
NOT do: commit NIV text into the public fork, or attach it to a GitHub release** — that's redistribution,
and no GitHub repo's MIT license establishes the right to sublicense the NIV. Public artifacts = converter +
preset + docs only; the NIV JSON lives only on the SD card. See [[crosslight-releases]].

### Track A — NIV support (architecture: user-supplied + desktop converter)

- **`tools/convert_niv.py`** (host-side, like `scripts/make_wallpaper.py` — no firmware change). Input:
  the user's authorized NIV source. Auto-detect and support: aruljohn per-book JSON (66 files + `Books.json`
  for order), the canonical single-file schema, and (lower priority) the Sermonator TXT. Output:
  `/Bible/NIV/niv.json` in CrossLight canonical schema. **Must cast chapter/verse to numeric.** Validation
  (fail loudly): exactly 66 books, expected names/order, valid UTF-8, expected chapter counts, contiguous
  unique chapter+verse numbers, no missing/dup/empty verses. (Validation runs on whatever file Logan
  supplies — no need to pre-fetch all 66 files into this repo.)
- **Preset is OPTIONAL — proven 2026-09-30.** `BibleTranslations::installed()` *scans `/Bible/` for
  folders* and shows any `/Bible/<ABBR>/<abbr>.json` it finds; it does **not** gate on the preset table.
  So **the converted file alone makes NIV appear** (as the bare label "NIV") and be selectable, with no
  firmware change — and it's already on the device SD at `/Bible/NIV/niv.json`. Adding
  `{"niv", "New International Version", "Copyrighted — user-supplied"}` to `PRESETS` is pure polish: it
  upgrades the display name (`displayName()` falls back to `upper(abbr)` without it) and carries a license
  string. If added, it must **not** auto-download (no getBible entry); check the menu doesn't render a
  download button for it. Ship it in a future OTA, not a blocker.
- **Docs:** short "Add your own NIV" guide (run converter → copy to `/Bible/NIV/niv.json`).
- **Test verses after install:** Genesis 1, Psalms 23, John 3, Romans 8, Revelation 13, plus random chapters.
- Blocked on: Logan dropping his NIV file here so the converter targets its exact format.

### The organizing idea (decided with Logan 2026-09-30): a historical study Bible, not a numerology toy

Tracks B and C aren't two loose gimmicks — together they rebuild the **study apparatus that early English
study Bibles actually shipped with**, which is what makes it interesting rather than a novelty. Two concrete
anchors:

- **The Geneva Bible (1560) was the first English "study Bible."** It was the first English Bible with
  *numbered verses* (the numbering scheme our reader still uses), the first with cross-references, the first
  to italicize supplied words, and it carried book "arguments" (prologues), a concordance, and ~300,000 words
  of marginal notes drawn from Reformation writers. It is the model for "a Bible you *study in*, with a layer
  around the text." (Sources below.)
- **The 1611 KJV was bound with a front matter that is pure calendar/computus apparatus.** Its opening leaves
  held "The Kalendar" (holy days), **"An Almanacke for xxxix yeeres"** (tabulated 1603–1641), **"To finde
  Easter for euer,"** and the table of Psalms/Lessons for Morning and Evening Prayer. Those tables *are*
  Golden Number + Dominical (Sunday) Letter + Epact + computus. **Track C is reconstructing what the 1611
  Bible literally printed in its front pages — not inventing abstract math.**

So the study layer has a spine — **"what did the book itself carry?"** Numbers = the countable/interpretive
layer readers worked out in the margins; Calendar = the almanac tables printed in the opening leaves. Keep
the two separate (the handoff is right), but present both under one **Study** area that reads as a
historically-grounded companion — every claim cited, every interpretation labelled — not a numerology toy.

Sources (research 2026-09-30): 1611 front-matter contents & the Golden-Number/Dominical-Letter columns —
[CPHC "Easter with the KJV 1611"](https://www.cphc.org.uk/updates/2016/3/19/9cpn5q8ufbga59nzgsgd98iulenb8j),
[St Aelfric facsimile review](https://saint-aelfric-customary.org/2019/12/07/book-review-the-holy-bible-1611-fascimile-edition/);
Geneva as first English study Bible —
[HBU Dunham Bible Museum](https://hc.edu/museums/dunham-bible-museum/tour-of-the-museum/past-exhibits/from-geneva-the-first-english-study-bible/).

### Track B — Bible Numbers (a concordance-with-commentary, data-driven)

- UI category name **"Bible Numbers"** (not "Numerology"). Reuses the existing verse cache for occurrence
  counts — no second Bible parser.
- **What actually makes it a study tool, not a gimmick:** each number screen *leads with countable facts from
  the loaded translation* — live occurrence count of the English word(s), and its book/chapter distribution,
  computed off the verse cache — and only then layers interpretation on top, clearly ranked. So the reader
  sees the data first and the tradition second, and can jump from any cited reference straight into the reader
  (reuse `goTo`/`goToVerse`, which already exist). It's a small, honest concordance-with-commentary for a
  handful of resonant numbers.
- **Data-driven, not hard-coded in C++.** Per-number JSON (e.g. `/Bible/numbers/7.json`) with an explicit
  `classification` per claim, layered: FACT (countable from text) / LITERARY PATTERN / TRADITIONAL
  INTERPRETATION / SCHOLARLY DEBATE / SPECULATION. The firmware is a renderer/query engine. Never present
  `7 = perfection` or `6 = evil` as absolute.
- Initial set: **7, 6/666, 12, 40** (then 3, 10, 70/77, 1000). 666: show the textual fact (Rev 13:18), the
  gematria/isopsephy explanation, major interpretations (incl. Nero Caesar), the 616 variant, and citations
  — no single interpretation as absolute.
- **Caveat to bake in:** English lexical occurrence counts differ across translations and differ from the
  underlying Hebrew/Greek number. A count is "occurrences of the English word 'seven' in <translation>",
  labeled as such — not "the biblical number seven."

### Track C — Historical Calendar (the 1611 almanac, reconstructed — separate from numerology)

- **Concept: pick a year, get the row the 1611 almanac would have shown** — Golden Number, Dominical (Sunday)
  Letter, Epact, and the computus date of Easter — the same columns "An Almanacke for xxxix yeeres" tabulated
  for 1603–1641, now for *any* year. Present each with a one-line plain-language note on what the column meant
  and why a Bible printed it. **Must NOT be filed under Bible Numbers.**
- **Golden Number:** `(year mod 19) + 1` (handoff wrote `(year+1) mod 19`, 0→19; both give 13 for 1611 —
  **verify the exact formula and edge cases when building, don't trust either blindly**). Pin with
  known-answer host tests: 1611 → 13, plus a modern year cross-checked against a published table.
- **Easter/computus is the headline output.** Note the wrinkle that fits the historical framing: in 1611
  England was still on the **Julian** calendar (Old Style), so offer both Julian and Gregorian computus and
  label them — that difference is itself part of the history the tool teaches.
- Supporting columns: Epact and Dominical Letter (needed for the Easter calc anyway), Metonic-cycle position
  (Golden Number is just the cycle index). Pure math + a little data → **host-testable** like the reference
  parser and the Flock matcher.
- **Parked adjacency (not a phase):** a Geneva-style "book argument" / marginal-note layer could later sit
  beside the reader, since Geneva is the study-edition model. Data-only, SD-resident, no engine change.

### Track D — Bible reader responsiveness ("flow faster / smoother"), investigation (added 2026-09-30)

Logan's ask: make the Bible app **flow a bit faster and smoother.** This is a **measure-first item, not a
committed change** — do not optimize by guess. Profile on the real X4 Pro (the simulator structurally cannot
measure e-ink timing or SD latency — see CLAUDE.md), find the actual bottleneck, then decide. Candidates to
instrument with `millis()` timing:
- **Chapter open:** the 16 KB streaming JSON read → binary chapter-cache path. Is first-open reparse the cost,
  or is it the e-ink refresh that follows? (Cache is `CACHE_VERSION 2`; a cold cache reparses the whole file.)
- **Page turn:** full vs partial e-ink refresh, and `buildPages()` cost per chapter (pagination + `versePages`).
- **Hub/menu transitions** after the hub-first change (extra Activity construct/destroy on each open).
Likely low-risk wins *if the data supports them:* keep the chapter cache warm across hub→reader hops, prefer
partial refresh on page turns where the panel allows, avoid rebuilding pages when only the page index moved.
[[verify-dont-assume]] — nothing here ships until a real-hardware measurement names the bottleneck.

### Phasing (what ships when)

- **Phase 1 — DONE (this audit).** Schema/path/preset/loader/cache confirmed; handoff corrected.
- **Phase 2 — DONE 2026-09-30 (Track A).** NIV converter built/tested; `niv.json` copied to the device SD
  at `/Bible/NIV/niv.json` and validated there. No firmware preset needed — `installed()` scans `/Bible/`
  for folders, so NIV already shows up and is selectable. The preset remains optional polish.
- **Phase 3 — SHIPPED 2026-09-30, OTA 26.9.3 (Track B): Bible Numbers v1.** `src/bible/BibleNumbers.{h,cpp}`
  + `BibleNumbersActivity`/`BibleNumberDetailActivity`, a new Bible-hub row (gated on data present), 4
  number studies (7, 12, 40, 666) in `assets/bible_numbers/` (copy to `/Bible/numbers/` on SD). Every claim
  classified FACT/PATTERN/TRADITION/DEBATE/SPECULATION; cited verses load live off the chapter cache, same
  dedup technique as Memory Work. Host tests: `test/bible_numbers/` (7 cases, pins `classificationFromString`
  and `Ref::reference()` — both made header-inline specifically so they're testable without ArduinoJson
  stubs). 408/408 host tests pass. Real-device build: flash 49.4% of the x4pro slot (up from 49.1%
  baseline) — did **not** hit the auto-mode safety classifier this time. Release:
  https://github.com/flyboy-byte/crosslight/releases/tag/26.9.3. **Planned initial set now COMPLETE
  (2026-09-30): added 3, 77, 1000** as pure data drops — no code or firmware change, exactly as the
  architecture intended. All 7 (3, 7, 12, 40, 77, 666, 1000) validated and committed. **STILL NEEDS: a
  second SD copy** — these three were written while the card was back in the device; only the original 4
  (7/12/40/666) have actually been copied to `/Bible/numbers/` on the card so far. Copy
  `assets/bible_numbers/3.json`, `77.json`, `1000.json` next time the card is out. Further numbers (e.g.
  10, 6/666-adjacent variants) can be added the same way, anytime, no release needed if OTA'd code already
  supports the schema (it does).

  **Lesson learned 2026-09-30, same day: the row shipped silently disabled on first install.** The OTA
  firmware was correct, but the `/Bible/numbers/*.json` data files only exist in the repo's `assets/` —
  they are a manual SD copy, same convention as Memory Work and Flock signatures, and that copy step was
  not done before telling Logan to go try it. Symptom matched a disabled row exactly (same look as
  Bookmarks/Memory Work when empty): not selectable, side buttons can't highlight it. Fixed same day by
  copying `assets/bible_numbers/*.json` to the SD. **Process fix: for any feature gated on manual SD data
  (not just firmware), copy the data to the actual device SD card as part of shipping it, the same way NIV's
  `niv.json` was copied — don't just tell the user to copy a release's code and leave the data step implicit.**
- **Phase 4 — Historical Calendar (Track C): computus half SHIPPED to the tree 2026-10-01 (commit
  `89da088e`, not yet OTA-tagged).** `src/bible/HistoricalCalendar.h` (pure, inline, dependency-free so it's
  host-testable like the Numbers parser) computes Golden Number, Julian Epact, Gregorian Dominical/Sunday
  Letter, and Easter in both the Julian (Old Style) and Gregorian (New Style) calendars.
  `HistoricalCalendarActivity` is a year-keypad screen (defaults to the current year via `halClock.localTime`,
  else 1611) reached from a new always-enabled "Historical Calendar" hub row. `test/historical_calendar/`
  pins it (7 cases) against known modern Easters, Orthodox-via-+13-offset, and the 1611 row. **Verify-don't-
  assume paid off: the ChatGPT handoff's "Golden Number 1611 = 13" is WRONG — it's 16** (13 is 1608);
  recorded in the header and tests. Host 549/549, x4pro + sim both build green; simulator-screenshot-verified
  (default-year render and the 1611 almanac row). Needed one sim HAL stub (`HalClock::localTime`, mirrored in
  the simulator repo, commit `f03286c`). **Remaining in Track C: the reading-calendar half** (below). **New
  research 2026-09-30, from Logan's own 1611 facsimile:** the
  almanac front matter is genuinely two features, not one — (a) Easter/computus math (Golden Number,
  Dominical Letter, Epact, "To finde Easter for euer"), and (b) **a yearly Scripture-reading calendar**
  ("The Table and Kalender... of Psalmes and Lessons... at Morning and Euening prayer"), which Logan
  confirmed his copy also has. Track C should ship both: the computus calculator, and a reading-plan
  feature shaped like Memory Work (a JSON day→reading list) modeled on the 1611's own lectionary table
  rather than invented. Logan may supply specifics from his physical copy to ground the exact format;
  not blocking — build from solid modern computus sources either way if he doesn't.
- **Phase 5 — advanced:** repeated-word/pattern search, cross-translation comparison, Hebrew/Greek number
  metadata, and the parked Geneva-style note layer. Only after the above prove out on hardware.
- **Track D (reader responsiveness)** runs alongside, not as a gated phase — it's a profile-then-fix loop that
  can land in any release once a hardware measurement justifies a specific change.

Every phase ships as a Wi-Fi OTA (26.9.x → bump the version), and each new UI must be simulator-checked
before it's called done (the number/calendar engines are pure logic → host-testable like the Flock matcher).

## Pentest/security toolkit — PR #1 review (2026-10-01)

**Status: MERGED into `crosslight` 2026-10-01** (was `flyboy-byte/crosslight#1`, branch
`claude/amazing-mendel-ni6blk`, opened by a cloud-agent session against this plan). Reviewed in a worktree,
then merged after the fixes below. This section is the review record; "Planned: Pentest/security toolkit"
below it is the original scope doc (still accurate for context). The historical doc references in this
section (`crosslight_pentest_handoff.md`, `_research.md`, `_slice6_scope.md`) point at scratch/handoff docs
that were **removed on merge** — their load-bearing content is folded into the "Planned" section below.

### What's in the PR

Slices 1-5 of the passive toolkit, each its own module: `src/wifiaudit/` (AP scanner, evil-twin/deauth-flood
threat detection, PCAP capture to SD, EAPOL/PMKID + hashcat-22000 harvest) and `src/bleaudit/` (passive BLE
scanner + device fingerprinting). Plus two scratch docs (since removed) and a PLAN.md update. 4057
insertions, 49 files. All five tiles wired into `UtilityRegistry.cpp` correctly (one include + one entry
each, `HomeActivity.cpp` untouched, matching the established pattern).

### Independently verified (built and checked myself, in a worktree, not just reading the PR's own claims)

- **Simulator build: SUCCESS** (`pio run -e simulator_x4_pro`, after symlinking `simulator/` and running
  `git submodule update --init --recursive` — neither done by the PR's own CI since it had no PlatformIO).
- **Device build: FAILS.** `src/bleaudit/BleScanner.cpp` includes `<NimBLEDevice.h>`, but **`NimBLE-Arduino`
  was never added to `platformio.ini`'s `lib_deps`.** This is the exact gap the PR's own checklist predicted
  ("no PlatformIO in the build environment... device/sim build not run"). **Concrete fix needed before
  merge:** add the NimBLE-Arduino lib dep to the `x4pro` env (and `simulator_x4_pro` if BLE needs a host
  stub there — check whether `BleScanner` is guarded for non-ESP32 builds the way `FlockScanner` is).
- **Host tests: 478/478 pass** (408 prior baseline + 70 new — matches the PR's claim exactly). Ran the full
  suite myself via `cmake --build . && ctest`, not just trusted the PR description.
- **BLE signature data (`assets/bleaudit/signatures.json`) is genuinely well-sourced, not fabricated.**
  Fetched the actual raw Bluetooth SIG assigned-numbers registry (`company_identifiers.yaml`,
  `member_uuids.yaml` from the official `bluetooth-SIG/public` Bitbucket repo, via `curl`, not an
  AI-summarized fetch — learned that lesson from the Flock OUI episode) and independently confirmed all 6
  entries: `0x004C`=Apple Inc. ✓, `0x0006`=Microsoft ✓, `0xFEED`=Tile Inc. ✓, `0xFD5A`/`0xFD59`=Samsung
  Electronics ✓, `0xFEAA`=Google LLC (Eddystone) ✓, `0xFE2C`=Google LLC (Fast Pair) ✓. The 7th entry (Flipper
  Zero, matched by BLE local-name substring) is correctly labeled as a heuristic, not a registry fact. This
  is a materially different, better-grounded situation than the Flock OUI research — a stable public
  standard, not field-sniffed/rotating data.
- **No transmit code present** — grepped the new files for `esp_wifi_80211_tx`/`send`/`transmit` patterns;
  none found. Matches the PR's claim that slices 1-5 are receive-only.
- **No copyleft contamination found** (shallow check: grepped for Marauder/Bruce/GPL/AGPL references in the
  new source — none). The research doc explicitly and correctly flags Marauder (GPL-3.0) and Bruce
  (AGPL-3.0) as copyleft-contaminating for this MIT tree and recommends Ghost ESP/Radio-Ink (MIT) as code
  references instead — good practice, worth preserving as new code gets added.
- **8 files need `clang-format`** (minor, mechanical): `Eapol.{h,cpp}`, `HarvestScanner.{h,cpp}`,
  `WifiFrame.{h,cpp}`, `ThreatDetect.h`, `BleMatcher.cpp`.
- **`partitions.csv` untouched**, no other upstream-hot files touched beyond the expected `UtilityRegistry.cpp`.

### Gating decision — RESOLVED with Logan directly 2026-10-01 (not just the agent's call)

The PR's slice-6 scope doc unilaterally dropped the per-boot confirmation prompt that
`docs/crosslight_pentest_handoff.md` had specified as the second gate layer, using Logan's own
"don't need training wheels" reasoning from the *manual-MAC-entry* discussion — a different question the
agent wasn't present for. Flagged this distinction to Logan directly (forced manual entry = pointless
friction, correctly dropped; a one-tap per-boot confirmation = a real guard against misclicks, a different
thing) and asked for his own ratification rather than letting it stand by inheritance.

**Logan's explicit answer: drop it. "the agent was correct. i dont need a babysitter. especially if u build
a good ui."** So: confirmed final gating is **compile-time flag only**
(`CROSSLIGHT_ENABLE_ACTIVE_AUDIT`, off in `gh_release*`, on in local/dev builds) — no per-boot UI
confirmation screen. `ActiveAuditGate` may stay in the tree as inert defense-in-depth (per the scope doc) or
be simplified away entirely; Logan's "especially if u build a good ui" is a design note for whoever builds
slice 6 — deliberate navigation to reach an active tool (not a shortcut from Home) does real work here
instead of a confirmation dialog.

### Not yet reviewed by me — on the table for next session

- **The slice-6 transmit primitives** (`FrameBuilder`, `ActiveAuditGate`, `AttackTx` + tests):
  on branch `worktree-agent-a5724a1624b098548` (`e2f56c1`), **recovered and pushed to the fork 2026-10-01**
  (after a scare where it was briefly unreachable — see the Slice 6 note in "Planned"). **Not independently
  built or reviewed by me**, and based on pre-rebase `crosslight`. Review + rebuild-test with the same rigor
  as the PR review before merging; `test/wifi_audit/CMakeLists.txt` is the one shared edit to re-apply.
- **The deauth linker-bypass tradeoff.** `docs/crosslight_pentest_research.md` is clear-eyed about this:
  deauth needs `-Wl,-wrap=ieee80211_raw_frame_sanity_check` (the stock `esp_wifi` blob blocks it on purpose)
  and that **"complicates OTA (pinned/patched lib)."** This is a real engineering/architecture decision, not
  a detail — may be worth building 6b (beacon flood) and 6c (evil-twin) first since neither needs the patch,
  and deciding on 6a (deauth) separately once the OTA-complication tradeoff is weighed.
- **On-device verification** of slices 1-5 (scan correctness, SD writes, radio release on exit) — same
  caveat every radio feature in this project has carried (simulator can't test real RF/SD timing).

### Next steps (the "rebase, fix issues, get it up to date" pass)

1. ~~Add `NimBLE-Arduino` to `lib_deps`~~ **DONE 2026-10-01** — `h2zero/NimBLE-Arduino @ 2.5.1` in base
   `lib_deps`; BLE code is `#if defined(ARDUINO_ARCH_ESP32)`-guarded so host/sim don't need it (confirmed).
2. ~~Run `clang-format` on the flagged files~~ **DONE 2026-10-01** — all new `src/wifiaudit/`+`src/bleaudit/`
   sources + the new utility activities formatted (clang-format 22 flagged more than the PR's original 8).
3. ~~Rebuild simulator + device + host clean~~ **DONE 2026-10-01** — host 529/529, simulator SUCCESS, x4pro
   SUCCESS (flash 54.0%, RAM 33.6%). **PR #1 (passive slices 1-5) merged into `crosslight`.**
4. Independently review + rebuild-test the slice-6 foundation branch (`worktree-agent-a5724a1624b098548`,
   `e2f56c1` — recovered + pushed 2026-10-01) before merging it; re-apply its `test/wifi_audit/CMakeLists.txt`
   edit onto current `crosslight`.
5. Decide the deauth/OTA tradeoff; build 6b/6c first if deauth is deferred (see Slice 6 notes above).
   **← the real engineering decision when active tools are picked up.**
6. Flash and verify passive slices 1-5 on real hardware (still owed — no OTA tag until done).
7. ~~Merge PR #1~~ **DONE** (merged to branch 2026-10-01; release gated on item 6).

## Upstream rebase (2026-10-01) — DONE

`crosslight` merged onto `develop` after fast-forwarding `develop` to `origin/develop` (29 new upstream
commits: SD-card plugin system, EPUB content-protection/DRM, the `ReaderSession`/`CatalogActivity` refactor,
`TxtToHtml` rendering, assorted fixes). This was the "rebase our code, fix issues, get it up to date" step
of Logan's plan, done *before* starting the PR #1 pentest-toolkit fix list above (self-aware ordering: check
upstream's implementation against ours on every conflict, take theirs when it's genuinely better, not just
different). Commit `84237e0e` on `crosslight`, pushed to `fork/crosslight`.

**Where upstream's implementation won over ours (adopted, didn't keep our version):**
- **StreamingJsonParser** — upstream moved its copy into the `freeink-sdk` submodule (`libs/network/JsonSax`)
  with full RFC 8259 `\uXXXX` decoding (surrogate pairs, multi-byte UTF-8, malformed/truncated-escape
  detection, chunked-feed state). Our in-tree copy (`lib/JsonParser/StreamingJsonParser.{cpp,h}`) only
  handled ASCII `A`-style escapes. Deleted ours, `BibleChapterLoader.cpp` now pulls from freeink-sdk
  like everything else does.
- **HttpDownloader.cpp** — upstream rewrote it entirely around `freeink::fetchResumable` (resumable Range
  downloads, 401/403 UNAUTHORIZED distinction, cleaner redirect handling). Our side was just the
  pre-rewrite code with no CrossLight-specific logic in it — took upstream's wholesale.

**Where both sides were additive (kept both, no real conflict in intent):**
- `main.cpp`'s boot routing: kept our `LockScreenActivity`/`routeAfterBoot` wrapper (startup passphrase
  gate), folded upstream's new `SILENT_REBOOT_TARGET_JOIN_NETWORK` branch into the wrapped `routeAfterBoot`
  lambda so it isn't lost.
- `HomeActivity`/`SettingsActivity`: our Bible/Utilities rows + upstream's Plugins row; our StartupPassword
  setting + upstream's Plugins setting. Both kept.

**A real mistake caught before it shipped:** `git checkout --theirs freeink-sdk` on a submodule gitlink is a
silent no-op (`git checkout` doesn't resolve submodule conflicts that way) — it left the working tree on
whatever commit was already checked out, which happened to be a stale, unrelated, *older* freeink-sdk commit
(`deb62ab7`, 2026-09-22) rather than the one upstream's `develop` actually pins (`23392260`, 2026-09-30).
Caught because the build then failed on APIs (`FONT_LABEL`, `ListProps::toggleCheckbox`) that upstream's
just-merged UI code uses and genuinely exist at `23392260` — the stale pointer, not a real incompatibility.
Fixed by checking out the commit directly inside the submodule. Lesson: never trust `checkout --theirs/--ours`
on a gitlink path; verify with `git ls-tree <ref> -- <submodule-path>` on both sides and resolve by checking
out the target commit inside the submodule itself.

**Also fixed, surfaced by the rebase itself:**
- Duplicate `STR_DOWNLOAD_COMPLETE` key in `english.yaml` (both sides added it independently, same value).
- `test/bible_chapter_loader/CMakeLists.txt` and `test/streaming_json_parser/CMakeLists.txt` still pointed
  at the deleted `lib/JsonParser` copy; repointed at `freeink-sdk`. The `streaming_json_parser` one had been
  silently auto-merged to upstream's content by git (we'd never touched that file ourselves, so there was no
  conflict to force a manual look) — a reminder that a clean 3-way auto-merge on a file you do depend on is
  not automatically a safe merge, just a mechanically unambiguous one.

**Simulator-side work (separate repo, `flyboy-byte/crosslight-simulator`, commit `e80550e`, pushed):**
upstream's new plugin system and `TrustedTime` touch real network (TLS via wolfSSL, HTTP arg handling) and
ESP32 HAL surface the simulator fork had never needed before. Rather than exclude it from the sim build,
built it out properly since `CrossPointWebServer.cpp`'s plugin endpoints and `HomeActivity`'s
`anyPluginInstalled()` are now load-bearing, not optional:
- `Client.h`, `IPAddress.h`, `WiFiClient.h` — split out to match the real Arduino network-client header
  layout that freeink's `SecureClient` derives from (avoiding a `WiFi.h` ↔ `NetworkClient.h` ↔ `Client.h`
  include cycle).
- `NetworkClient` now derives from `Client`; `available()`/`read()`/`peek()` became real `recv()`/
  `ioctl(FIONREAD)` calls instead of permanent `0`/`-1` stubs — freeink's `SecureHttpClient` actually calls
  these at runtime for plugin-catalog HTTP(S) fetches, not just at compile time.
- `WebServer`: added the real ESP32 WebServer's protected `RequestArgument`/`_currentArgs`/`_postArgs`
  fields (unused placeholders backing an override that frees request memory between requests — a real
  embedded concern, a no-op on host).
- `HalStorage::readFileToString`/`replaceFile`, `HalFile::truncate`, `HalGPIO::getFactoryMac`,
  `esp_wifi_get_ps`, `esp_random.h`, `Preferences.h` (in-process key/value map, not persisted across sim
  restarts — fine for exercising logic within one run), `esp_sntp.h`'s `configTzTime` — new real-HAL/ESP32
  surface from the plugin system and `TrustedTime`.
- `Arduino.h` now pulls in `freertos/FreeRTOS.h` (matching real ESP32 Arduino's transitive include) so
  `portMUX_TYPE`/`portENTER_CRITICAL` resolve the same way `test/test_trusted_time.py`'s own host-stub
  `Arduino.h` already assumes they do — **first attempt added the include directly to `TrustedTime.cpp`
  instead, which built the simulator fine but broke that host test** (its stub set has no
  `freertos/FreeRTOS.h`). Moved the include into the simulator's `Arduino.h` instead, leaving the shared
  firmware file untouched. Caught by running the full host suite again after the simulator went green,
  not just trusting "the thing I was working on now works."
- `WString.h`: `reserve`/`remove`/`begin`/`end` — ArduinoJson's Arduino-`String` converter and new
  plugin-system code need these.
- `platformio.local.ini`: added the freeink-sdk libs the new code needs (`SecureNet`, `JsonSax`,
  `CatalogList`, `ContentProtection`) and `wolfssl/Arduino-wolfSSL @ 5.7.2` + its build flags (mirrors the
  relevant subset of `[env:x4pro]`'s), plus `-include sys/time.h` for a `gettimeofday` portability gap in
  that vendored library on plain native Linux (third-party code, fixed via a force-include flag rather than
  editing the vendored source).

**Verified clean on all three targets:** 459/459 host tests, simulator build+link (SUCCESS), `x4pro` device
build (SUCCESS — flash 51.3%, RAM 31.5%, no regression in headroom).

## Planned: Pentest/security toolkit (scoped 2026-09-30) — PAUSED & SPLIT 2026-10-04

> [!IMPORTANT]
> **Pivot 2026-10-04.** Active development of the offensive radio tools is **paused**. The two general-use
> tools (Wi-Fi Analyzer, Bluetooth Scanner) stayed in the normal menu; everything offensive moved to
> `src/offensive/`, compiled but **hidden** behind the SD flag `/offensive/enabled`. The full reference is now
> **[`docs/crosslight/offensive/`](docs/crosslight/offensive/README.md)** (renamed from `pentest/`) — README,
> STATUS, ARCHITECTURE, TOOLING, ROADMAP, reframed as "paused & quarantined." That folder is the source of
> truth for the offensive tools' state and (if ever resumed) open work; the sections below are original
> scoping/history.

**Origin and authorization context:** Logan is an Extra-class ham radio operator (the top US amateur license
class — requires real RF-law knowledge), owns the hardware under test (laptops in his dorm), understands the
legal boundaries, and wants Hak5/DEFCON-style pentest tooling as a personal security-research hobby. This
is a legitimate "security research / defensive use case" authorization context, not a request to attack
others' infrastructure. **The one hard boundary that doesn't move regardless of skill:** point it at gear
you own, never dorm-shared/university network infrastructure or other students' devices — that boundary is
about whose network it is, not about competence.

**Reference project found 2026-09-30: `dagnazty/Radio-Ink`** — a fork of the *same upstream*
(`crosspoint-reader`) CrossLight is built on, adding a full Wi-Fi/BLE security-audit toolkit (947 commits).
Its feature set (passive scanning/recon/detection + gated active/transmit modes) is the shape to build
toward; not something to merge wholesale (different fork, own conventions), but a strong reference for scope
and for its safety model specifically.

**Safety model to adopt, copied because it's already proven and resolves the friction-vs-capability
question cleanly (from Radio-Ink's `SCOPE.md`):** active/transmitting features are gated **two ways** — a
compile-time flag (their `RADIO_AUDIT_ENABLE_ACTIVE`, off in all public release builds) **and** a per-session
on-device confirmation prompt. Passive tools ship in every build, including public GitHub releases. **This
maps directly onto how CrossLight already works:** public OTA releases vs. a local `pio run -e x4pro` build
Logan compiles himself. Plan: a new build flag (e.g. `CROSSLIGHT_ENABLE_ACTIVE_AUDIT`), left **off** in
`x4pro-gh_release`/`x4pro-gh_release_rc` (so public releases never carry transmit capability), **on** only
in a local/dev build; plus a one-time-per-session on-device confirmation screen before any active tool runs.
No UI friction beyond that — normal scan → select → act flow, not forced manual entry (an earlier, overly
restrictive proposal here was walked back after Logan correctly pushed back on treating a licensed,
knowledgeable operator like he needed training wheels).

**Reusable foundation already in this tree:** `src/flock/*`'s promiscuous-mode 802.11 frame capture
(`FlockScanner`/`FlockFrame`) is the same radio primitive most passive Wi-Fi tools need — frame parsing,
OUI extraction, SSID extraction are already built, tested, and now building clean on both host and device
(see Item 14 above). New passive tools are mostly new *matching/reporting* logic on top of frames the radio
layer already captures, not a new radio engine.

**Scope for CrossLight (adapted from Radio-Ink's list, not copied wholesale — pick what's useful for a
personal lab, skip what isn't):**
- **Passive (ships in every build, zero legal ambiguity):** Wi-Fi scan (APs, channels, signal, encryption
  type), client/probe-request recon, BLE scan + known-device-type identification (trackers, Flipper Zero,
  etc. — useful personal-safety awareness, same spirit as the Flock work), rogue-AP/KARMA/evil-twin
  *detection* (as opposed to performing it), deauth-flood *detection*.
- **Active (gated behind the two-layer flag, dev builds only):** targeted deauth (scan → select → fire, on
  gear Logan owns), evil-twin/captive-portal (own-network testing), BLE advertisement spoof.
- **Utilities discussed alongside this (easy, no scope question, build anytime):** flashlight/frontlight
  toggle, unit converter, a better-looking calculator (current one is functional but plain) — these can
  land before or in parallel with the security toolkit, no dependency between them.

**Passive toolkit MERGED into `crosslight` 2026-10-01 — all three targets green, not yet flash-verified.**
Five passive slices (from PR #1, `claude/amazing-mendel-ni6blk`), all in the new `src/wifiaudit/` +
`src/bleaudit/` modules:
1. Wi-Fi AP scanner (SSID/BSSID/channel/encryption) — `WifiScanActivity`.
2. Threat detection (evil-twin + deauth-flood) — `WifiThreatActivity`.
3. PCAP capture to SD — `WifiCaptureActivity` (`/wifiaudit/capNNN.pcap`).
4. EAPOL/PMKID + hashcat 22000 export — `PmkidHarvestActivity` (clientless PMKID live; full handshake via the
   captured pcap offline).
5. Passive BLE scanner + fingerprinting — `BleScanActivity` (verified-identifier starter list only, from
   `assets/bleaudit/signatures.json` — all 6 SIG entries independently verified via the real Bluetooth SIG
   registry during the PR review).

All receive-only/passive, so they ship in every build (no gate). **Fixes applied on merge** (the PR review's
checklist items 1-3): added `h2zero/NimBLE-Arduino @ 2.5.1` to base `lib_deps` (the device-build blocker —
BLE code is `#if defined(ARDUINO_ARCH_ESP32)`-guarded so host/sim don't pull it in); clang-formatted all the
new sources; rebuilt clean on **host (529/529 tests), simulator (SUCCESS), and x4pro device (SUCCESS, flash
54.0%, RAM 33.6%)**. The two cloud-agent scratch docs (`crosslight_pentest_research.md`,
`crosslight_pentest_slice6_scope.md`) and the handoff doc were removed — this section is the source of truth.

**On-hardware test 2026-10-01 (26.10.1): UI + scan work; BLE matching is SD-data-gated.** Logan flashed
26.10.1 and ran the tools. The UI is good (he called out the new time-at-top and overall look), the scans
run. **But BLE fingerprinting labels nothing** because the matcher loads `/bleaudit/signatures.json` from the
SD card (`BleSignatures.h` `SIGNATURE_PATH`) and that file isn't on the card yet — the scan works, matching is
data-gated. **Exact same lesson as Bible Numbers: shipping the firmware ≠ shipping the SD data.** Fix: copy
`assets/bleaudit/signatures.json` → `/bleaudit/signatures.json` on the device SD next time the card is out
(Wi-Fi threat detection + PMKID need no data file — they're pattern-based — so only BLE is affected). Logan's
fine leaving it for now ("thats fine"). Still also owed: on-hardware check of the SD writes (PCAP/hccapx
files land correctly) and radio release on exit.

**Active/transmit FOUNDATION (Slice 6 primitives) — recovered + MERGED into `crosslight` 2026-10-01; dormant.**
`FrameBuilder` (pure deauth/disassoc/beacon byte builders), `AttackTx` (the single transmit path), and
`ActiveAuditGate` (per-boot bool) + their two tests, from branch `worktree-agent-a5724a1624b098548`
(`e2f56c1`). **Timeline worth remembering:** the agent first called this "parked, ready to merge" while it was
a *local-only* commit in an ephemeral cloud container; a later session couldn't reach it (`git cat-file` → not
a valid object), so this doc briefly (correctly then) said it was gone. The original container was still
alive, the agent pushed it, and it's now genuinely on the remote — recovered, not rebuilt. Merged cleanly onto
the rebased `crosslight` (zero conflicts), clang-formatted, and **builds green on all three targets: host
542/542 (13 new slice-6 tests), simulator + x4pro device SUCCESS, flash/RAM UNCHANGED (54.0% / 33.6%).**
Why zero size impact: it's **fully dormant** — `AttackTx::transmitFrame` is a hardcoded `return false` unless
`CROSSLIGHT_ENABLE_ACTIVE_AUDIT` is defined (no env defines it), and nothing in the UI invokes `FrameBuilder`
yet, so it's dead-stripped. Merging it foreclosed no decision: no linker-wrap, no activities, flag off.
**Byte-level correctness review DONE 2026-10-01:** line-reviewed `FrameBuilder` against IEEE 802.11 — FC
subtypes (0xC0 deauth / 0xA0 disassoc / 0x80 beacon), MAC addressing (addr1=dest, addr2/addr3=BSSID for the
spoofed-source deauth), little-endian reason code, and the beacon fixed body + SSID/Rates/DS info elements are
all well-formed and correctly bounded (`DEAUTH_FRAME_LEN`=26, SSID capped at 32), matching the `WifiFrame`
parser and pinned by thorough host tests. `AttackTx` confirmed a no-op without the flag. One comment nit fixed
(Supported Rates were labeled 6/12/24/54 Mbps; they decode to 18/24/36/54). **Frames are correct — safe to
un-gate from a correctness standpoint when the time comes.** **Still owed before any of it can transmit:**
(1) the UI activities (6a-6d); (2) the compile flag + deauth linker-wrap decision below. (Lesson, now in
memory: a sub/cloud-agent saying "built X on branch Y" means nothing until Y is confirmed *pushed and
reachable*.)
Planned active tools: 6a targeted deauth, 6b beacon flood, 6c evil-twin captive portal, 6d BLE adv spoof.

**Gating policy REVISED 2026-10-01 (Logan's explicit call — "ignore that, i didnt agree with it, my firmware
will be the same as on gh"):** the earlier "off in gh_release*, on in local builds" split never actually
matched reality — every CrossLight release has always been built from plain `[env:x4pro]`, not a separate
`gh_release` env. Rather than introduce a build split now, `CROSSLIGHT_ENABLE_ACTIVE_AUDIT` is defined
directly in `[env:x4pro]` — **the same binary ships on the public GitHub release and on Logan's own device.**
Flagged to Logan once, plainly, before building: this means anyone who downloads the release gets working
active-transmit capability on their own hardware, not just Logan on his own gear — the "own gear only"
boundary now rests entirely on whoever flashes it, not on the build. He owns that tradeoff for his own fork.
The two-layer gate (compile flag + `ActiveAuditGate`'s per-boot bool) stays as defense-in-depth regardless.
Per-boot confirm friction dropped (Logan's call — "i dont need a babysitter"): the gate auto-confirms on
first transmit per screen, no modal, but the on-screen "your own gear only" warning line stays visible the
whole time an active-tool screen is open.

**6b (Beacon Flood) SHIPPED to the tree 2026-10-01 (commit `aef086b0`, not yet OTA-tagged).** First active
tool built. New primitives: `wifiaudit::TxRadio` (STA-mode radio bring-up for raw-frame TX, mirrors
`ApScanner`'s begin/end/channel shape but for transmit), `wifiaudit::makeLocallyAdministered` (RandomMac.h —
legalizes random bytes into a valid synthesized MAC so each flooded beacon reads as a distinct AP, host-tested
4 cases), `AttackTx::buildSupportsActiveAudit()` (lets the UI show "no radio" vs "disabled in this build" vs
actually running, rather than attempting to transmit and silently getting 0%). Cycles through 12 built-in
placeholder SSIDs with synthesized BSSIDs on a user-picked channel at ~5 frames/sec. Host 559/559 (+4 tests),
x4pro + sim both green (flash 54.2%), simulator-screenshot-verified for the no-radio shell (Running is
device-only to verify, same bar as every other radio tool here). **Still owed: on-hardware test** (actual
transmit, and a nearby device's scan list genuinely filling with the flood).

**Key engineering decision still open: the deauth path (6a).** Deauth/disassoc need
`-Wl,-wrap=ieee80211_raw_frame_sanity_check` (the stock `esp_wifi` blob blocks raw deauth on purpose), and
that linker-wrap pins/patches the Wi-Fi lib — a bigger build-system change than 6b/6c/6d need. Still deferred;
decide separately once 6c is built and 6b is hardware-verified.

**6c (Evil Twin) SHIPPED to the tree 2026-10-01 (commit `8c7e86f3`, not yet OTA-tagged).** Clones a
user-typed SSID (via the existing `KeyboardEntryActivity`) as an open AP, serves a captive-portal landing
page to anything that joins via the same `DNSServer` wildcard-redirect + `WebServer` mechanism
`CrossPointWebServerActivity` already uses for the file-transfer hotspot. **Deliberate scope decision, stated
to Logan rather than silently built:** the landing page is a plain test notice, not a credential-harvesting
login form — useful for its actual purpose (confirming your own devices/the already-shipped
`WifiThreatActivity` evil-twin DETECTOR correctly react to an evil twin) without adding a phishing payload to
a binary that, per the gating-policy revision above, ships on the public release. Same gate as 6b
(`buildSupportsActiveAudit()` + auto-confirmed `ActiveAuditGate`). Unlike 6b's raw-frame TX, AP mode + the web
server genuinely work in the simulator (same stack the hotspot already exercises there), so this could in
principle get deeper sim verification than 6b did — not done this pass, kept to the same no-radio/disabled
shell-only bar as every other radio tool for now. Host 559/559 (unchanged — no new pure logic), x4pro + sim
both green (flash 54.3%). **Still owed: on-hardware test** (AP actually broadcasts, a client actually
associates and gets the landing page, and ideally a second CrossLight device's `WifiThreatActivity` actually
flags it).

**6d (BLE Advertisement Spoof) SHIPPED to the tree 2026-10-01 (commit `9568d65f`, not yet OTA-tagged).**
Active BLE broadcaster — the transmit counterpart to the passive BLE scanner. `bleaudit::BleSpoofer` wraps
NimBLE advertising with two deliberately GENERIC profiles (a named "Test Device" and an example all-zero-UUID
iBeacon) — test transmitters for exercising your own BLE scanner/detector, NOT impersonations of any real
product/person/tracker. Pure host-tested iBeacon manufacturer-data builder `bleaudit::BleBeacon.h`
(`buildIBeaconManufacturerData`, 6 cases, the FrameBuilder pattern). Same double gate as the other active
tools. Host 565/565 (+6), x4pro + sim both green, simulator-screenshot-verified (no-radio shell). **Still
owed: on-hardware test.** **Slice 6 active UI tools 6b/6c/6d all shipped.**

**6a (targeted Deauth) — CODE COMPLETE but BLOCKED at the build step by the auto-mode safety classifier
(2026-10-01). The code is NOT committed; it is preserved in a git stash** (`stash@{0}`, "6a deauth ...").
What the stash contains: `DeauthActivity.{h,cpp}` (scan APs via `ApScanner` → tap to select → broadcast
deauth via `FrameBuilder::buildDeauth` + `AttackTx::transmitFrame`), the `-Wl,-wrap=ieee80211_raw_frame_sanity_check`
flag added to `[env:x4pro]`, the `__wrap_ieee80211_raw_frame_sanity_check()` definition in `AttackTx.cpp`,
the registry entry, and the `STR_DEAUTH*` strings. **What happened:** after writing the code, the classifier
denied `python3 scripts/gen_i18n.py` with "[Security Weaken]" — it had run fine all session for 6b/6c/6d, so
it's reacting to the deauth content specifically. Deauth is the most purely-weaponizable tool and (per the
gating revision) would ship in the public binary; the earlier design concern and the classifier are pointing
the same way. **Decision: did NOT work around the denial** (no retry, no routing the build through Logan via
`!pio run` — that's the same outcome through another actor, which the denial forbids). The code was stashed to
keep the tree clean + building at 6d, and the call on whether/how to take 6a forward is left to Logan. **The
deauth code was never compiled by this session — treat it as unverified if resumed.** The `__wrap` signature
and linker-wrap are the FRAGILE parts (closed-blob symbol, can break on an SDK bump).

## Backlog (queued 2026-10-01)

**26.10.1 test passed → kept building (easy utilities, Logan's pick 2026-10-01).** Shipped since 26.10.1,
**unreleased, all on `crosslight`, host 542/542 + sim + device green:**
- **Calculator** now shows the pending operation on the display (commit `29b5b15b`) — the `2/2` "can't see the
  `/`" bug.
- **Flashlight** utility (`FlashlightActivity`) — frontlight to full brightness, tap to toggle, restores
  prior state on exit, holds off auto-sleep while lit.
- **Unit converter** (`UnitConverterActivity`) — Length/Mass/Temp/Volume/Speed, tap-cycle category + units,
  digit keypad, live result; affine `scale+offset` model so C/F/K work from the same data table.

These three want an on-hardware test like the pentest tools got — cut a **26.10.2** release when ready (bundle
with any other near-term work). Remaining backlog below.

- ~~**Custom tile icons for the utilities**~~ **DONE 2026-10-01** — added Lucide flashlight/calculator/
  arrow-left-right via the icon manifest + `gen_icons.py`, new `Flashlight`/`Calculator`/`Convert` `UIIcon`
  values + mappings in `UiAppHelpers.h`, tiles repointed. Device flash 54.1%.
- **Next real feature work** (Logan's "keep building out" direction): the Slice 6 active tools — review the
  recovered `FrameBuilder`/`AttackTx` byte layouts, build 6b/6c first, decide the deauth/OTA tradeoff. Plus,
  whenever the SD card is out: copy `assets/bleaudit/signatures.json` → `/bleaudit/` so BLE matching works.
- ~~**README overhaul, using the `readme` skill.**~~ **DONE 2026-10-01** — rewrote `README.md` from upstream
  CrossPoint's into a CrossLight landing page (house style: centered hero, honest status table, ASCII stack
  diagram, install/build/simulator, Wi-Fi/BLE scope note, upstream credit). Browser-verified via the skill's
  script (3 alerts, 3 collapsibles, 3 tables, clean heading hierarchy).
- ~~**Better "About".**~~ **DONE 2026-10-01** — "About" turned out to mean the **GitHub repo About sidebar**
  (it was upstream's "Open-source e-reader firmware" with the homepage pointing at upstream's commercial
  site), not the device screen. Fixed via `gh repo edit`: accurate description, 7 topics
  (bible/crosspoint/e-ink/e-reader/esp32-s3/firmware/xteink), stale homepage cleared. (The *device's*
  `AboutActivity` is already a rich hardware/firmware spec list — it was never the generic text Logan saw.)

## Decisions made

- **Partition layout: keep both OTA slots, take the space from `spiffs` instead (2026-09-24).** The
  16MB chip was carved as app0/app1 at 0x640000 each plus a 3.375MiB `spiffs` region that **nothing in
  the firmware mounts** — all user data is on SD under `/.crosspoint/`, and the only mention of SPIFFS
  in the tree is a comment. Split it between the app slots (0x7F0000 each, 7.94MiB), which lands app1
  on 0x800000 and ends exactly at 0x1000000 with no gaps. Considered and rejected: a single-slot
  layout, worth ~3.4MiB more. The second slot is not just Wi-Fi OTA — `FirmwareFlasher` +
  `OtaBootSwitch` write it for **SD-card firmware updates** too, and it's what makes a half-written
  image survivable. On a device whose only port is the finicky magnetic pogo connector, keeping the
  card-update and rollback paths beats headroom we don't need (46.7% of a slot used after the change).
  Revisit only if a tile genuinely needs >7.9MiB. **Changing this requires a wired flash, not OTA.**
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
12. ~~Verse-reference jump (2026-09-10)~~ — **REMOVED 2026-09-30 (Logan's call).** The "Go to Verse"
    rows were taken off both the Bible hub (`BibleHubActivity`) and the in-reader menu
    (`BibleMenuActivity`), along with `openVerseJump()`, `InitialAction::VerseJump`, and the
    keyboard-entry glue (`lastVerseQuery`, `MAX_REFERENCE_LENGTH`). Kept: `goToVerse()` (still used by
    Search results) and the `src/bible/BibleReference.{h,cpp}` parser + its host test — now unused by the
    UI but left in place as a self-contained, tested util (cheap to re-wire if wanted). Removal verified:
    all 11 Bible translation units compile on `-e simulator_x4_pro`. Original design notes retained below
    for history:

    Verse-reference jump (2026-09-10). "Go to Verse" row on `BibleMenuActivity` opened
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
