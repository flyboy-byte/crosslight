# Unified API: what it means for Claude Panel and the e-reader

**Status: server side is LIVE as of 2026-10-18.** `https://bridge.flyboybyte.com` now serves scoped
Bearer keys (`llk_...`), Claude prompts, Claude usage, host status, and the existing glucose
routes — all four routes CrossLight could want. Reported by the `extraction` session over two
cross-session messages, 2026-10-18 (the second correcting the first's claim that usage wasn't
built — it is) — tagged `DOCUMENTED` below where it quotes those messages directly, since
`extraction` is the authoritative server-side source, not `INFERRED`.
Firmware-side work (this repo) has not started yet. The shipped Claude Panel design in
`docs/crosslight/claude-panel.md` is unchanged until Phase C below actually ships: Ask Claude and
Bible Passage Q&A still go device-direct to `api.anthropic.com` today.

**Blocked on one thing:** the e-reader's own key. Keys are minted only by Logan, from his laptop,
via `extraction`'s `tools/libre-keys` (`add ereader --scopes read:glucose,read:status,claude:prompt
--expires 90d`) — not something this session can do. `docs/crosslight/firmware-profiles.md` (where
this bridge client lands as a `PersonalIntegration` module) is unaffected and not blocked by this.

**This doc covers the Wi-Fi/HTTP routes only.** The BLE glucose relay (`LibreBleServer`,
`LibreBlePacket`) is a separate, already-frozen contract — confirmed by extraction (2026-10-18,
`DECISIONS.md` D-029): the Android companion app is the reference sender, the e-reader (like the
PineTime watch) is a follower that implements the spec as given, never the other way around. Our
implementation already matches it exactly (8-byte LE packet, same service/characteristic UUIDs) —
nothing to change there. BLE carries glucose only; everything else in this doc goes over Wi-Fi.

Server-side design, security and risk analysis live in the owner's private `extraction` repo (`docs/unified-api.md`, `docs/claude-account-risk.md`, PR #2 there). This doc covers only the e-reader's view. Evidence tags: `DOCUMENTED` = read in a repo file, `INFERRED` = reasoning, `UNKNOWN` = must be checked.

## The idea in one paragraph

One authenticated HTTPS API on the owner's own server that the phone app, the laptop, agents and this e-reader all use, for: Libre glucose (exists today), Claude usage, server status, and Claude prompting. The e-reader would hold a small, scoped, revocable **API key for that server** instead of the Claude OAuth setup-token, and the server would be the only place the Claude credential lives and the only program that talks to Anthropic. The watch never talks to it directly; it goes through the phone app.

## Why consider it

1. **One place holds the Claude token.** Today the OAuth token sits in plaintext at `/claude/token.txt` on the SD card (accepted, decision 3 in `claude-panel.md`). With the unified API the card would hold only a scoped key; losing the device means revoking one key, not rotating the Claude credential.
2. **Account-policy exposure goes through one gate.** Anthropic's Claude Code legal page says OAuth subscription tokens are for ordinary use of Claude Code and native apps, and that developers' own apps should use API keys (https://code.claude.com/docs/en/legal-and-compliance.md, read 2026-10-16). A custom client sending the setup-token (as `ClaudeClient.cpp` does with the `oauth-2025-04-20` beta header) is outside the documented carve-out. **The owner asked Anthropic support on 2026-10-16** (chat, answered by an AI agent, so not a binding ruling): personal use of `claude setup-token` on the owner's own server is **allowed** when billed to the owner and not resold or intermediated, but there is **no supported programmatic way** to read Pro-plan usage limits. So the on-device direct path is the part the reply did not cover, and the server-side usage check is the one gray item. Funnelling Claude calls through one server-side program with one queue and caps keeps the footprint small. Stated as fact, not as a reason to stop; the owner owns the call, as with the SD-card stance. The gating record (conditions, unknowns, what to do if rejected) is in the private `extraction` repo, `docs/claude-account-risk.md`.
3. **More than Claude on the one device.** A glucose glance and a server-status screen, using the same key system.

## Scale: what actually shipped vs. what was scoped

The 2026-10-16 scoping below (usage-first, prompt deferred) predates the server work. Extraction
first reported (2026-10-18) that `claude:prompt` shipped but the usage glance did not; a same-day
**correction** reversed that second half — `read:usage` is also live (`GET
/api/v1/latest/claude-usage`). So as of 2026-10-18, **all four routes CrossLight could want exist**:
`claude:prompt`, `read:usage`, `read:glucose`, `read:status`. Nothing is left to wait on
server-side; Phase A-D in the table below are all buildable once the key exists.

## What would NOT change

- Claude Panel v1 (usage glance, Ask Claude, Bible Passage Q&A) works as built, directly device to `api.anthropic.com`. Nothing here removes or breaks that path.
- Foreground-only power model (decision 5): Wi-Fi and polling only while the activity is open; the e-ink frame holds between refreshes.
- Native e-ink UI, no LVGL (decision 4).
- The owner's security stance (decision 3): no modals, no babysitting.

## The decision it touches

Decision 1 says "direct device to Anthropic, **no bridge**", rejected because a carried reader should not depend on the owner's PC being up. That reason concerned an always-on PC. An always-on server is a different dependency, but the device would still need Wi-Fi **and** the server, where direct needs only Wi-Fi. So this proposal keeps **direct as the default and the fallback**: the bridge is used only when a key file is present, and the device falls back to direct if the bridge is unreachable (owner's choice whether to enable that fallback). Reopening decision 1 is only needed if the owner later wants the bridge to become the only path.

## What the e-reader can call now (`DOCUMENTED`, from extraction's 2026-10-18 report)

| Screen / feature | Route | Scope | Notes |
| --- | --- | --- | --- |
| Ask Claude / Bible Q&A | `POST /api/v1/claude/prompt` | `claude:prompt` | Body `{"prompt":"<=4000 chars"}`, 8 KB cap. Reply `{"reply":"<=4000 chars"}`. Stateless — no memory between calls, server picks the model (Haiku), no tools. |
| Glucose glance | `GET /api/v1/latest/glucose`, `/api/v1/glucose`, `/api/v1/health` | `read:glucose` | Unchanged from today's Libre bridge. Never poll faster than 60s; stop polling on a 401. |
| Server status | `GET /api/v1/status` | `read:status` | Fixed-schema JSON; `age_s` + `stale` flag (stale = >120s old, show as unknown not a number). Also returns `cpu_pct`, `cores_pct`, `mem_used_gb`/`mem_total_gb`, `disk_used_gb`/`disk_total_gb`, `net_recv_mb`/`net_sent_mb`, and per-unit `cpu_pct` for libre-bridge/nginx/fail2ban/ssh — every one of these is nullable; null means "--", not zero. |
| Process list | `GET /api/v1/processes` | `read:processes` | **Separate scope from `read:status`** — `read:status` alone gets 403 here. Counts only, never names (per the original contract). Only worth requesting if the e-reader actually wants an app-count tile; not in the e-reader key by default. |
| Usage glance | `GET /api/v1/latest/claude-usage` | `read:usage` | **Live** (corrected 2026-10-18 — an earlier same-day report said this didn't exist; it does). See shape below. |

The e-reader key needs `read:glucose,read:status,read:usage,claude:prompt` — no admin/process/
action scope. A valid key missing a route's scope gets 403; expired/revoked gets 401. Keys expire
(90d is the planned e-reader cap), so this needs a re-issue path, not a one-time setup.

### Usage glance shape (`GET /api/v1/latest/claude-usage`, `DOCUMENTED`)

200 response:
```json
{"five_hour":{"used_pct":40,"resets_at":"2026-10-18T03:50:00.000Z"},
 "seven_day":{"used_pct":63,"resets_at":"2026-10-19T07:00:00.000Z"},
 "state":"allowed","age_s":0}
```
`used_pct` is 0-100 or `null`; `resets_at` is ISO UTC or `null`; `state` is `"allowed"` or a short
word for limited. The number comes straight from the unmodified `claude` binary's own output — no
Anthropic header probing, so this route needs no Claude token or Anthropic call of any kind.
Cached 10 min server-side, probed at most once per 5 min — polling faster than every few minutes
gains nothing; poll on demand or every 5-10 min. If `age_s` is large, or a `503 {"error":"unknown"}`
comes back (the server returns that once its own data passes 30 min old), show "unknown", never a
stale number. `429`/other `503`s follow the same plain-display, no-retry-loop rule as `claude:prompt`.

Auth: `Authorization: Bearer llk_...`; no `anthropic-beta` header. **The Claude OAuth token no
longer needs to exist on the SD card at all** once `claude:prompt` is wired up — extraction's own
framing: this *replaces* the e-reader's direct-to-Anthropic path, not supplements it.

## `claude:prompt` limits the firmware must respect

- One call at a time, server-wide: a second concurrent call gets `429 busy` (not queued).
- 10s minimum gap between calls from the same key (`429 too_soon`).
- 40 calls/key/UTC day, 100/day total server-wide (`429 key_cap` / `daily_cap`); `429 rate_limited`
  is the general case. Every 429 carries `Retry-After`.
- A 24h lockout (`503`, `Retry-After` ≈86400) latches on any account-type failure — show it, don't
  retry in a loop.
- Round trip is ~1.5-3s typically; client timeout should be ≥30s (matches the server's own cap, and
  the device's existing default from the direct path).
- Other errors: `400 bad_prompt`/`bad_request`, `401`, `403`, `413 too_large`, `415 json_only`,
  `502 upstream_error`, `503 unavailable`/`disabled`. All plain-text-displayable, not retriable.

## Firmware impact (`DOCUMENTED` for the contract, `INFERRED` for how `ClaudeClient` absorbs it)

- **Endpoint and key from SD**, e.g. `/claude/bridge.txt` (URL + key), read once at launch like
  `/claude/token.txt` today. `ClaudeClient` gets a second backend; whether direct stays as a
  fallback or gets retired is still the owner's call (unified-api.md's own "decision 1 not
  overturned" — the bridge was optioned around, not forced).
- **TLS trust.** The bridge's chain is Let's Encrypt (ISRG root; a Root X1/X2 migration is
  in progress per extraction). Pin against a CA bundle containing **both** ISRG Root X1 and X2 (or
  pin both roots), not a leaf — this is a different chain from `api.anthropic.com`'s GTS/GlobalSign
  one, so it needs its own wolfSSL cert-chain flags checked, not an assumption that the existing
  `WOLFSSL_ALT_CERT_CHAINS`/`WOLFSSL_SHA384` setup carries over unchanged.
- **Timeouts and sizes.** ≥30s client timeout (above). Prompt cap 4000 chars / 8 KB body; reply cap
  4000 chars — both comfortably inside the device's existing RAM budget for this feature.
- **Model.** `kAskModel` (`claude-haiku-4-5` on-device today) becomes moot for the bridge path — the
  server always picks Haiku itself. The on-device constant stays only for the direct fallback, if
  one is kept.
- **Display safety for any glucose/status screen**: a glucose reading older than 10 minutes is
  never shown as a number (unchanged); a status field with `stale: true` (>120s) is shown as
  unknown, not as a number — same discipline, same reasoning.

## Phases for CrossLight (revised 2026-10-18 against what's actually live)

| # | Do | Gate |
| --- | --- | --- |
| 0 | ~~Nothing in firmware; wait for server scopes/routes.~~ **Cleared 2026-10-18** — server's live. | Done |
| A | ~~Bridge backend in `ClaudeClient`...~~ **Done 2026-10-18** — `ClaudeBridgeClient` (`src/claude/`), confirmed on the X4 Pro over USB serial: `promptViaBridge()`/`fetchUsageViaBridge()` both verified against the live server. | Done |
| B | ~~Usage glance via the bridge...~~ **Done 2026-10-18** — Claude Panel now calls `fetchUsageViaBridge()` instead of the direct path; confirmed on hardware. | Done |
| C | ~~Ask Claude / Bible Passage Q&A actually routed through the bridge...~~ **Done 2026-10-18, bridge-only (no direct fallback kept)** — both now call `promptViaBridge()`. `ClaudeClient.{h,cpp}` (direct `api.anthropic.com` path, SD-card OAuth token) is unreferenced outside its own file; left in place, not deleted. Hardware-testing this also surfaced and fixed a real bug in `BibleReaderActivity::openAskClaude()`'s default verse-range selection (it accumulated every verse since the chapter start instead of just the current page). | Done |
| D | Glucose glance and server-status tiles via `read:glucose`/`read:status` (status route is new; glucose route is a rescope of the existing Libre bridge client, not new code). A process-count tile needs the separate `read:processes` scope — optional, not requested by default. | Stale rule and HIGH/LOW verified |

**Still blocking all of A-D:** the e-reader's own key doesn't exist yet. Ask Logan to run (on his
laptop, in `extraction`): `tools/libre-keys add ereader --scopes
read:glucose,read:status,read:usage,claude:prompt --expires 90d`. This repo cannot mint it.

## Decided / superseded (2026-10-18)

The four "open questions for the owner" below are substantially answered by what shipped:
1. ~~Optional extra or only path?~~ Moot for Ask Claude — extraction built `claude:prompt` as the
   *replacement* for the direct path, not an addition. Still the owner's call whether to keep a
   direct fallback at all for Ask Claude, or retire it outright once C ships.
2. Fallback-to-direct-on-bridge-down: still open — needs the owner's call once A/C are built and
   there's a real "bridge down" case to design against, not before.
3. Glucose/status screens: yes — both routes exist and are in scope now (Phase D).
4. Model/cap: answered by the server for `claude:prompt`, not configurable from the firmware
   side — Haiku, 40/day/key, 100/day total, fixed by extraction. `read:usage` has its own separate
   cadence limit (cached 10 min, probed ≤once/5 min server-side) — poll on demand or every 5-10
   min, not tied to the prompt route's caps.

## Where the truth lives

The server design, risk register, and the account-policy analysis are maintained in the private `extraction` repo. If this doc and that one disagree, that one wins; update this one.
