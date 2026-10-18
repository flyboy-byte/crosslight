# Unified API: what it means for Claude Panel and the e-reader

**Status: server side is LIVE as of 2026-10-18.** `https://bridge.flyboybyte.com` now serves scoped
Bearer keys (`llk_...`), Claude prompts, host status, and the existing glucose routes. Reported by
the `extraction` session over cross-session message, 2026-10-18 — tagged `DOCUMENTED` below where
it quotes that message directly, since it's the authoritative server-side source, not `INFERRED`.
Firmware-side work (this repo) has not started yet. The shipped Claude Panel design in
`docs/crosslight/claude-panel.md` is unchanged until Phase C below actually ships: Ask Claude and
Bible Passage Q&A still go device-direct to `api.anthropic.com` today.

**Blocked on one thing:** the e-reader's own key. Keys are minted only by Logan, from his laptop,
via `extraction`'s `tools/libre-keys` (`add ereader --scopes read:glucose,read:status,claude:prompt
--expires 90d`) — not something this session can do. `docs/crosslight/firmware-profiles.md` (where
this bridge client lands as a `PersonalIntegration` module) is unaffected and not blocked by this.

Server-side design, security and risk analysis live in the owner's private `extraction` repo (`docs/unified-api.md`, `docs/claude-account-risk.md`, PR #2 there). This doc covers only the e-reader's view. Evidence tags: `DOCUMENTED` = read in a repo file, `INFERRED` = reasoning, `UNKNOWN` = must be checked.

## The idea in one paragraph

One authenticated HTTPS API on the owner's own server that the phone app, the laptop, agents and this e-reader all use, for: Libre glucose (exists today), Claude usage, server status, and Claude prompting. The e-reader would hold a small, scoped, revocable **API key for that server** instead of the Claude OAuth setup-token, and the server would be the only place the Claude credential lives and the only program that talks to Anthropic. The watch never talks to it directly; it goes through the phone app.

## Why consider it

1. **One place holds the Claude token.** Today the OAuth token sits in plaintext at `/claude/token.txt` on the SD card (accepted, decision 3 in `claude-panel.md`). With the unified API the card would hold only a scoped key; losing the device means revoking one key, not rotating the Claude credential.
2. **Account-policy exposure goes through one gate.** Anthropic's Claude Code legal page says OAuth subscription tokens are for ordinary use of Claude Code and native apps, and that developers' own apps should use API keys (https://code.claude.com/docs/en/legal-and-compliance.md, read 2026-10-16). A custom client sending the setup-token (as `ClaudeClient.cpp` does with the `oauth-2025-04-20` beta header) is outside the documented carve-out. **The owner asked Anthropic support on 2026-10-16** (chat, answered by an AI agent, so not a binding ruling): personal use of `claude setup-token` on the owner's own server is **allowed** when billed to the owner and not resold or intermediated, but there is **no supported programmatic way** to read Pro-plan usage limits. So the on-device direct path is the part the reply did not cover, and the server-side usage check is the one gray item. Funnelling Claude calls through one server-side program with one queue and caps keeps the footprint small. Stated as fact, not as a reason to stop; the owner owns the call, as with the SD-card stance. The gating record (conditions, unknowns, what to do if rejected) is in the private `extraction` repo, `docs/claude-account-risk.md`.
3. **More than Claude on the one device.** A glucose glance and a server-status screen, using the same key system.

## Scale: what actually shipped vs. what was scoped

The 2026-10-16 scoping below (usage-first, prompt deferred) predates the server work. What
`extraction` actually built and reported live 2026-10-18 is different in one important way:
**`claude:prompt` shipped, `read:usage` did not** — the usage glance (5h/7d) is explicitly
**not built** ("unknown whether the binary can report it", per extraction). So for CrossLight,
Ask Claude / Bible Passage Q&A via the bridge (old Phase C) is now the thing that's actually
ready to build, while the usage-glance tile (old Phase B) has nothing to call yet.

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
| Server status | `GET /api/v1/status`, `/api/v1/processes` | `read:status` | Fixed-schema JSON; `age_s` + `stale` flag (stale = >120s old, show as unknown not a number). Other apps are counts only, never names. |
| Usage glance | — | — | **Not built.** Extraction: "unknown whether the binary can report it." No route to call; drop this tile from scope until/unless one exists. |

The e-reader key carries exactly `read:glucose,read:status,claude:prompt` — no admin/process/action
scope. A valid key missing a route's scope gets 403; expired/revoked gets 401. Keys expire (90d is
the planned e-reader cap), so this needs a re-issue path, not a one-time setup.

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
| A | Bridge backend in `ClaudeClient`: URL+key from SD, pin the bridge's ISRG root(s), POST `/api/v1/claude/prompt`, handle the 429/503/4xx set above. | Works on the X4 Pro over USB serial |
| B | ~~Usage glance via the bridge.~~ **Dropped — no route exists.** Revisit only if extraction ships one. | N/A |
| C | Ask Claude / Bible Passage Q&A actually routed through the bridge call from A, replacing the direct `api.anthropic.com` call; SD-card OAuth token retired if the owner drops the direct fallback. | Quota/error paths behave on real hardware |
| D | Glucose glance and server-status tiles via `read:glucose`/`read:status` (status route is new; glucose route is a rescope of the existing Libre bridge client, not new code). | Stale rule and HIGH/LOW verified |

**Still blocking all of A-D:** the e-reader's own key doesn't exist yet. Ask Logan to run (on his
laptop, in `extraction`): `tools/libre-keys add ereader --scopes read:glucose,read:status,claude:prompt
--expires 90d`. This repo cannot mint it.

## Decided / superseded (2026-10-18)

The four "open questions for the owner" below are substantially answered by what shipped:
1. ~~Optional extra or only path?~~ Moot for Ask Claude — extraction built `claude:prompt` as the
   *replacement* for the direct path, not an addition. Still the owner's call whether to keep a
   direct fallback at all for Ask Claude, or retire it outright once C ships.
2. Fallback-to-direct-on-bridge-down: still open — needs the owner's call once A/C are built and
   there's a real "bridge down" case to design against, not before.
3. Glucose/status screens: yes — both routes exist and are in scope now (Phase D).
4. Model/cap: answered by the server, not configurable from the firmware side — Haiku, 40/day/key,
   100/day total, fixed by extraction.

## Where the truth lives

The server design, risk register, and the account-policy analysis are maintained in the private `extraction` repo. If this doc and that one disagree, that one wins; update this one.
