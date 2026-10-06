# Claude Panel — planning doc

**Status: WORKING ON REAL HARDWARE, confirmed 2026-10-05 via USB serial.** Claude Panel's usage fetch
is fully verified end-to-end on the X4 Pro: TLS handshake succeeds, HTTP 200, real headers parsed
(5h 53% used, 7d 22% used at time of test). Release **26.10.9** carries the fix (26.10.8 was TLS-broken
on real hardware — see "Hardware debugging" below). Token is on the SD card
(`/claude/token.txt`, format-checked: single line, correct `sk-ant-oat01...` OAuth prefix, no stray
whitespace, 109 bytes). Files: `src/claude/ClaudeClient.*` (was `ClaudeUsageClient`, generalized — see
`docs/crosslight/claude-features.md`), `src/activities/utilities/ClaudePanelActivity.*` (full UI: 5h/7d
bars, %, reset countdown, status/warning/limit text, mode chip, stale marker), tile in
`UtilityRegistry.cpp`, strings in `english.yaml`.

## Hardware debugging (2026-10-05) — the real root cause of "Request failed"

26.10.8 shipped with a TLS bug that only showed up on real hardware (the simulator doesn't exercise
real wolfSSL TLS against a live server, so this was invisible until Logan actually OTA'd and tested).
Debugged live over USB serial (`pio device monitor` substitute: a small pyserial capture script, since
`pio device monitor` needs a real tty and failed under the sandboxed shell). Root cause had two parts,
both in `freeink-sdk/libs/network/SecureNet/src/SecureClient.cpp`'s wolfSSL config (set in
`platformio.ini`'s `[env:x4pro] build_flags`, mirrored in `platformio.local.ini` for the simulator):

1. **`wolfSSL_connect` failed with `ASN_NO_SIGNER_E` (-188).** `api.anthropic.com`'s real chain (confirmed
   via `openssl s_client -showcerts`) is `leaf -> WE1 -> GTS Root R4 (GlobalSign cross-signed variant)` —
   the third cert is signed BY GlobalSign Root CA, not self-signed, even though its subject is "GTS Root
   R4" same as the self-signed root we pin. wolfSSL's default (non-"alt chain") verifier walks the certs
   in the order the server sent them and only consults the trust store at the final link, so it tried to
   find a signer for "GlobalSign Root CA" (not loaded) instead of recognizing that the earlier link
   (WE1 -> our pinned root) already terminates in a trusted CA. Fix: `-DWOLFSSL_ALT_CERT_CHAINS` — wolfSSL
   has this exact scenario named in its own source comments (`internal.c`).
2. **After that fix, failure changed to `HASH_TYPE_E` (-232).** Once ALT_CERT_CHAINS let wolfSSL actually
   reach the WE1->root signature check, it failed because `WOLFSSL_SHA384` was never enabled in this
   build (GTS Root R4 signs WE1 with `ecdsa-with-SHA384`). Fix: `-DWOLFSSL_SHA384`.

Both flags added to `[env:x4pro]` in `platformio.ini` and mirrored in `platformio.local.ini`'s simulator
build flags for parity (the simulator doesn't currently exercise this path against a live server, but
keeps the two builds' wolfSSL config from silently diverging). Verified fixed live on Logan's X4 Pro
2026-10-05 via USB flash + serial capture — see the "Findings" section below for the superseded, less
specific theory ("the handshake may fail, that's the first suspect") this replaces.

## Model finding (2026-10-05): Opus not usable on this subscription tier

Once TLS worked, `fetchUsage()` (Haiku probe) succeeded immediately (HTTP 200, 5h 53%/7d 22% used).
`ask()` (originally `claude-opus-5-5`) consistently returned **HTTP 429 with a genuine
`{"type":"rate_limit_error"}` body** — not transient: it still failed after Logan was away from the
device for 20 minutes, ruling out a short burst/per-minute limit. Swapping the model string live
(rebuild + reflash, no code structure change) isolated it fast: **Haiku (`claude-haiku-4-5`) returned
HTTP 200 immediately; Opus 429'd every time.** Read as the subscription OAuth token's tier not carrying
Opus access via the Messages API, even though the unified 5h/7d usage windows weren't near limit.
Sonnet (`claude-sonnet-5-5`) was queued to test the same way but Logan called it — **`kAskModel` is now
permanently `claude-haiku-4-5`** (`src/claude/ClaudeClient.cpp`), not a temporary test value. If a higher
tier is ever added to the account, Sonnet is the first thing worth re-testing before Opus.

**Ask Claude's first real test returned HTTP 429**, not a bug: the response's own
`anthropic-ratelimit-unified-overage-disabled-reason: out_of_credits` / `-overage-status: rejected`
headers say Logan's account has no overage credits enabled and is tapped out on whatever window covers
completions. The TLS fix is confirmed working; that 429 is Anthropic reporting real account state.

**Remaining step: Logan OTA-updates any other devices to 26.10.9** (his test unit already has the fix
flashed directly over USB this session) and confirms Ask Claude / Bible Passage Q&A once quota allows.
v1 = usage-only Claude Panel; see "Decided in the back-and-forth". **Next action: Phase 0 spike** (read SD
token, one authed HTTPS call, log rate-limit headers to serial, no UI), then Phase 1. Only the
wishlist/MCP items remain undecided — one at a time, after v1 works on device.

Name: **Claude Panel** (final, decided 2026-10-05).

---

## What it is

A CrossLight utility that shows your **Claude usage** (how much of your subscription window you've burned)
at a glance on the X4 Pro, and — later — lets the device trigger **remote Claude / MCP actions** directly.

Origin idea: **[sidewinderzz/claude_desk_panel](https://github.com/sidewinderzz/claude_desk_panel)** — a
desk display for the Waveshare ESP32-S3-Touch-LCD-4.3B (800×480 **RGB** LVGL panel) showing Claude usage, a
clock/weather page, and Home Assistant controls. Licensed **MIT (© 2026 sidewinderzz)** — attribution-only,
so we may lift snippets (e.g. the rate-limit-header read) with credit. **We are not vendoring their code:**
their UI is LVGL/C99 and their design assumes a mains-powered bridge PC; CrossLight is e-ink and portable, so
we write native. License entanglement is therefore minimal — credit the project, don't copy the LVGL/bridge.

## Decided architecture (locked with Logan 2026-10-04 — do not re-litigate)

1. **Direct device → Anthropic. No bridge.** The desk panel runs a Python bridge on an always-on PC; a
   *carried* e-reader shouldn't depend on your machine being up, so the device talks to `api.anthropic.com`
   itself. (Claude floated a "dumb device, smart bridge" split; Logan rejected it. Settled.)
2. **Token lives on the SD card:** `/claude/token.txt`, presence-read once at launch — same pattern as
   `/offensive/enabled`. Generated with `claude setup-token` (OAuth, not an API key).
3. **Security posture is Logan's, stated once:** `claude setup-token` is a long-lived OAuth credential tied
   to his Claude subscription; plaintext on SD means whoever holds the card/device holds the token. Logan
   owns that risk ("security is on me not losing the thing"). Same for MCP. **No further nagging in this
   doc or in code** — no modal, no babysitter (cf. the offensive-tools `ActiveAuditGate` stance).
4. **Native e-ink UI, not LVGL.** Render with CrossLight's existing draw primitives. It's a static frame
   that updates on a poll, which is e-ink's ideal workload (holds the image at zero power between refreshes).
5. **Power model = foreground-only (revised 2026-10-05, supersedes the original poll-and-sleep idea):** Wi-Fi
   and polling run only while the Claude Panel is the active activity (30 s auto-refresh or manual). No
   background polling, no sleep-screen widget. The e-ink frame holds at zero power between refreshes.
6. **MCP is direct too, via the Messages API MCP connector** (not a local MCP runtime) — see Phase 3.

## What the firmware already gives us (verified 2026-10-04, read from source)

The hard parts exist — this is a small lift, not a new subsystem:

- **CA-verified HTTPS** over `esp_http_client`: `src/network/HttpDownloader.cpp` (+ `HttpDownloader.h:12`
  documents "https is verified against the CA bundle"; `setInsecure()` fallback at `HttpDownloader.cpp:36`).
  TLS to a public host with real cert verification already works (OTA pulls GitHub over HTTPS).
- **HTTPS with custom auth headers**, which is almost exactly our request shape:
  `lib/KOReaderSync/KOReaderSyncClient.cpp` does `http.setInsecure(); http.begin(url);` then attaches
  `x-auth-*` headers and reads the response (lines ~63/102/134/260). Swap host → `api.anthropic.com`, swap
  `x-auth-*` → the two headers below, read the response + its `anthropic-ratelimit-*` headers.
- **SD presence/read** via `Storage` (same `Storage.exists()` / read used for `/offensive/enabled`).
- **Wi-Fi STA association + Utilities registry gating** already exist (OTA associates to the home network;
  `UtilityRegistry` is where a new "Claude" tile is registered under `kGeneral`).

## API specifics (from the claude-api skill, cached 2026-10-04 — verify at build time)

- **Auth header (OAuth setup-token):** `Authorization: Bearer <token>` **plus** `anthropic-beta: oauth-2025-04-20`.
  OAuth tokens go on `Authorization: Bearer`, **not** `x-api-key`. (If Logan ever switches to a real API key
  `sk-ant-...`, that one uses `x-api-key:` and no beta header — different path.)
- **Usage read:** make a minimal `POST /v1/messages` and read the `anthropic-ratelimit-*` **response
  headers** for window burn — this is what the desk panel does and it's the most accurate signal. Keep
  `max_tokens` tiny. (Confirm whether a cheaper/dedicated usage endpoint exists at build time before
  spending tokens on every poll.)
- **Default model** if a completion is ever actually needed: `claude-opus-5-5` (skill default). For a
  throwaway usage-probe request the model barely matters; pick the cheapest that returns the headers.
- **MCP connector (Phase 3):** `mcp_servers: [{type:"url", url, name}]` **and** a matching
  `tools: [{type:"mcp_toolset", mcp_server_name:<same name>}]`, beta header `mcp-client-2025-11-20`.
  Anthropic brokers the MCP server, so the device stays a plain HTTPS-POST client. Availability varies by
  platform — first-party API is fine; re-verify the beta string at build time (API drifts).

---

## Phases (the full arc, including "later finishing")

Each phase is independently shippable. Phases 0–2 are the usable product; 3–4 are the "later finishing".

**Phase 0 — spike (prove the pipe).** Read `/claude/token.txt`, do one authed HTTPS request to Anthropic,
log the rate-limit headers to serial. No UI. Goal: confirm the KOReaderSyncClient-style path reaches
`api.anthropic.com` and the OAuth headers parse. De-risks everything downstream.

**Phase 1 — Usage glance (the headline).** New `kGeneral` utility. Static e-ink frame: session-window and
weekly burn (bar/gauge), last-updated time, "no token" fallback state. One manual refresh action. This alone
is the "normal-person utility" that justifies the feature.

**Phase 2 — foreground auto-refresh toggle (DECIDED: 30 s or manual, active-only, no background/sleep polling).** Originally poll-and-sleep + settings: Configurable poll interval; auto-refresh loop with Wi-Fi
bring-up/teardown per poll; graceful offline/stale-data rendering; settings for interval + token presence
check. This is where the power model lands.

**Phase 3 — MCP / remote actions.** Device triggers remote Claude/MCP actions via the Messages API MCP
connector. Scope TBD in the back-and-forth (see open list). Keep the device a thin HTTPS client.

**Phase 4 — later finishing / "done" looks like:** polish pass — on-device token entry or rotation UX if
wanted, error/retry hardening, battery measurement of the poll loop on real hardware, optional extra
glance data (whatever the feature back-and-forth lands on), docs + a release. Define the real exit
criteria with Logan when Phases 1–2 are proven; don't gold-plate before the core is validated on device.

---

## Decided in the back-and-forth (2026-10-05, Logan)

- **Pages:** usage only for v1. Logan's "other things" were really *more Claude features*, not clock/weather/HA
  (undecided what those are — see open list).
- **Granularity:** 5h + 7d bars with %, reset times, and status.
- **Refresh:** a header toggle between **30 s auto-refresh** and **manual refresh button**. The app only polls
  while it is the selected/active activity — **no background polling, no sleep-screen widget.** (So the
  Phase 2 "poll-and-sleep" collapses to "foreground auto-refresh while open"; Wi-Fi stays up only while open.)
- **Placement:** Utilities tile (`kGeneral`). No sleep-screen overlay.

- **Token:** SD file only (`/claude/token.txt`); no on-device entry or rotation UI.
- **Failure UX:** one short status line ("No Wi-Fi", "Token rejected (401)", rate-limited) over the last good
  data, marked stale. Retry on the next tick or on tap.
- **Name:** "Claude Panel" (final).
- **Later feature wishlist (NOT v1, not committed — "sound cool," no urgency):** quick ask + reply (on-screen
  keyboard → Messages API → read on e-ink); ask about a selected passage/verse in the open book; remote
  Claude Code session status/control. MCP tool calls stay the vaguest. Build v1 = usage only first, then
  pick wishlist items one at a time.

## Still open (only these; everything else is in "Decided")

- **Wishlist order** (quick ask, ask-about-passage, remote session status) — pick one at a time after v1.
- **MCP scope (Phase 3):** vaguest; needs its own talk. Not v1.
- **30 s auto-refresh vs quota:** each refresh is a real `/v1/messages` call (counts against quota; Logan was
  at 97% weekly on 2026-10-04). Proposed: toggle defaults to **manual**. Confirm with Logan.

## Phase 1 UI (2026-10-05) — built, matches the decided spec

- **5h + 7d bars** with %, a drawn progress bar, and a reset countdown computed from the response
  `date` header plus the `anthropic-ratelimit-unified-{5h,7d}-reset` epoch (no on-device clock needed).
- **Status surfaced per window:** `allowed_warning` shows "near limit", `rejected` shows "limit reached",
  read straight off the `-status` headers (real values confirmed live, see below).
- **Header toggle (manual default, confirmed on):** a "Mode: Manual/Auto 30s" chip at the top of the
  screen. Tap the chip (or press Left) to flip it; tap elsewhere (or press Confirm) to refresh now.
  Auto mode polls every 30 s only while this screen is foregrounded (`loop()` checks elapsed time;
  `preventAutoSleep()` only holds the screen awake in auto mode).
- **Stale-data status line:** on any failure (`No Wi-Fi`, `Token rejected (401)`, `Request failed`,
  low memory) the last good bars stay on screen with a "stale" marker next to the mode chip, exactly
  per the decided failure UX.
- **Real header names confirmed live 2026-10-05** (via curl, token never printed): the panel reads
  `anthropic-ratelimit-unified-5h-utilization`, `-5h-reset`, `-5h-status`, `-7d-utilization`, `-7d-reset`,
  `-7d-status` — these exist and match what the client parses. Also present but unused by v1:
  `-representative-claim`, `-overage-status`, `-overage-disabled-reason`, `-fallback-percentage`,
  top-level `-reset`/`-status`.

## Findings during Phase 0 (2026-10-05)

- **CORRECTION: the firmware's HTTPS is NOT cert-verified.** `HttpDownloader.h`'s "verified against the CA bundle"
  comment is stale: `HttpDownloader.cpp` and `KOReaderSyncClient.cpp` both call `setInsecure()` over wolfSSL
  (`freeink::SecureHttpClient`). The earlier "CA-verified HTTPS" claim in this doc was wrong. The spike therefore
  uses `SecureHttpClient` directly with `setCACert()` pinned to **GTS Root R4** (the root `api.anthropic.com`
  chains to, verified via openssl 2026-10-05). UNVERIFIED on device: whether wolfSSL accepts the chain (the server
  may send the GlobalSign cross-sign). If the handshake fails, that's the first suspect.
- Reuse answer: `SecureHttpClient` (freeink-sdk) supports POST, custom headers and `getHeaders()`; no new shared
  helper needed.
- The activity mirrors `BibleDownloadActivity`'s Wi-Fi flow, including `silentRestart()` on exit.
- Toggle default: spike has no toggle yet; Phase 1 will default to manual (assumed, not yet confirmed by Logan).

## Verify-before-building checklist (don't assume — [[verify-dont-assume]])

- [x] (2026-10-05: use `SecureHttpClient` directly, see Findings) Confirm the `KOReaderSyncClient`/`HttpDownloader` HTTPS path is cleanly reusable for a new host +
      custom headers (or whether a small shared HTTPS helper should be factored out first).
- [ ] Confirm the exact usage signal: do a real authed request and read which `anthropic-ratelimit-*`
      headers come back on a subscription OAuth token; confirm there isn't a dedicated usage endpoint.
- [ ] Re-verify the `anthropic-beta: oauth-2025-04-20` and `mcp-client-2025-11-20` beta strings against the
      claude-api skill at build time (these drift).
- [ ] Measure Wi-Fi bring-up/teardown cost per poll on the device before committing to an interval default.
- [x] (sim build passes 2026-10-05) Simulator stub parity: any new HAL/Arduino call needs a matching stub in the sim fork (recurring gotcha).

## Credit

Feature inspired by **sidewinderzz/claude_desk_panel** (MIT). If any snippet is lifted (e.g. the rate-limit
header read), keep the MIT attribution in the source file.
