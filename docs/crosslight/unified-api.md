# Unified API: what it would mean for Claude Panel and the e-reader (PROPOSAL)

**Status: proposal only. Nothing is decided and nothing here is built for CrossLight.** It records a plan being scoped on the server side so the firmware side is not surprised. The shipped Claude Panel design in `docs/crosslight/claude-panel.md` is unchanged and keeps working: this is additive and optional, and the owner decides each step.

Server-side design, security and risk analysis live in the owner's private `extraction` repo (`docs/unified-api.md`, `docs/claude-account-risk.md`, PR #2 there). This doc covers only the e-reader's view. Evidence tags: `DOCUMENTED` = read in a repo file, `INFERRED` = reasoning, `UNKNOWN` = must be checked.

## The idea in one paragraph

One authenticated HTTPS API on the owner's own server that the phone app, the laptop, agents and this e-reader all use, for: Libre glucose (exists today), Claude usage, server status, and Claude prompting. The e-reader would hold a small, scoped, revocable **API key for that server** instead of the Claude OAuth setup-token, and the server would be the only place the Claude credential lives and the only program that talks to Anthropic. The watch never talks to it directly; it goes through the phone app.

## Why consider it

1. **One place holds the Claude token.** Today the OAuth token sits in plaintext at `/claude/token.txt` on the SD card (accepted, decision 3 in `claude-panel.md`). With the unified API the card would hold only a scoped key; losing the device means revoking one key, not rotating the Claude credential.
2. **Account-policy exposure goes through one gate.** Anthropic's Claude Code legal page says OAuth subscription tokens are for ordinary use of Claude Code and native apps, and that developers' own apps should use API keys (https://code.claude.com/docs/en/legal-and-compliance.md, read 2026-10-16). A custom client sending the setup-token (as `ClaudeClient.cpp` does with the `oauth-2025-04-20` beta header) is outside the documented carve-out. **The owner asked Anthropic support on 2026-10-16** (chat, answered by an AI agent, so not a binding ruling): personal use of `claude setup-token` on the owner's own server is **allowed** when billed to the owner and not resold or intermediated, but there is **no supported programmatic way** to read Pro-plan usage limits. So the on-device direct path is the part the reply did not cover, and the server-side usage check is the one gray item. Funnelling Claude calls through one server-side program with one queue and caps keeps the footprint small. Stated as fact, not as a reason to stop; the owner owns the call, as with the SD-card stance. The gating record (conditions, unknowns, what to do if rejected) is in the private `extraction` repo, `docs/claude-account-risk.md`.
3. **More than Claude on the one device.** A glucose glance and a server-status screen, using the same key system.

## Scale (owner, 2026-10-16): usage-first, prompts rare

The owner expects to prompt Claude rarely and mostly wants an occasional usage reading. So the server side is **usage-only first** (on-demand, cached, capped) and the prompt route is **deferred**. For CrossLight that means: Phases A and B (optional bridge backend, usage via the bridge) are the useful core; Phase C (Ask Claude via the bridge) is optional and later, and **until it exists the SD-card token stays** because Ask Claude keeps its direct path.

## What would NOT change

- Claude Panel v1 (usage glance, Ask Claude, Bible Passage Q&A) works as built, directly device to `api.anthropic.com`. Nothing here removes or breaks that path.
- Foreground-only power model (decision 5): Wi-Fi and polling only while the activity is open; the e-ink frame holds between refreshes.
- Native e-ink UI, no LVGL (decision 4).
- The owner's security stance (decision 3): no modals, no babysitting.

## The decision it touches

Decision 1 says "direct device to Anthropic, **no bridge**", rejected because a carried reader should not depend on the owner's PC being up. That reason concerned an always-on PC. An always-on server is a different dependency, but the device would still need Wi-Fi **and** the server, where direct needs only Wi-Fi. So this proposal keeps **direct as the default and the fallback**: the bridge is used only when a key file is present, and the device falls back to direct if the bridge is unreachable (owner's choice whether to enable that fallback). Reopening decision 1 is only needed if the owner later wants the bridge to become the only path.

## What the e-reader would call (proposed)

| Screen / feature | Route | Scope | Notes |
| --- | --- | --- | --- |
| Usage glance | `GET /api/v1/latest/claude-usage` | `read:usage` | Cached on the server; same 5h/7d fields the panel shows today |
| Ask Claude / Bible Q&A | `POST /api/v1/claude/prompt` | `claude:prompt` | One-shot, stateless; history stays on the device; model chosen by the server |
| Glucose glance (new) | `GET /api/v1/latest/glucose` | `read:glucose` | Read-only |
| Server status (new) | `GET /api/v1/status` | `read:status` | Small summary only, never per-process detail |

The e-reader key is **never** given an admin, process or action scope. A carried device is the likeliest to be lost.

Auth would be `Authorization: Bearer <server key>`; no `anthropic-beta` header, no Claude token on the device. Exact JSON shapes are `UNKNOWN` until the server phases land.

## Firmware impact (`INFERRED`, from reading the current code)

- **Endpoint and key from SD**, for example `/claude/bridge.txt` (URL and key), presence-read at launch like `/claude/token.txt`. `ClaudeClient` gets a second backend; the direct backend stays.
- **TLS trust.** Today `SecureClient` pins the chain for `api.anthropic.com` (GTS Root R4 via the GlobalSign cross-sign; `WOLFSSL_ALT_CERT_CHAINS` and `WOLFSSL_SHA384` were needed, per `claude-panel.md` "Hardware debugging"). The server's certificate chain is different (the issuer is `UNKNOWN` until read from the server with `openssl s_client -showcerts`). Pin the **root** of whatever chain it serves, not the leaf (leaves rotate about every 90 days), and re-check the wolfSSL flags for that chain. Do not use `setInsecure()` for the bridge.
- **Timeouts and sizes.** The device uses a 30 s request timeout today; the server should answer faster than that. Keep prompt and reply caps small (RAM).
- **Model.** `kAskModel` (`claude-haiku-4-5`, decided 2026-10-05 after Opus 429'd on this token tier) would become a server-side choice. The server may use a Console API key for prompts instead of the subscription token, which would also sidestep that tier limit (`INFERRED`).
- **Display safety for any glucose screen** (these come from the bridge's rules and apply on the device too): a reading older than 10 minutes is never shown as a number; HIGH and LOW are shown as words; no invented, smoothed or "fixed" value; no alarms, dosing, or interpretive thresholds. This is a data display, not a medical tool.

## Suggested phases for CrossLight (each gated on the server side and on the owner)

| # | Do | Gate |
| --- | --- | --- |
| 0 | Nothing in firmware. Wait for the server's scopes, usage route and prompt route to exist and be verified. | Server phases 1 to 3 pass |
| A | Optional bridge backend in `ClaudeClient` (URL and key from SD), direct stays default. Pin the server's chain root. | Works on the X4 Pro over USB serial, as the 26.10.9 fix was verified |
| B | Usage glance via the bridge. | Matches the direct numbers by eye |
| C | Ask Claude / Bible Q&A via the bridge. | Quota and error paths behave; token no longer needed on the card |
| D | Glucose glance and server-status tiles. | Stale rule and HIGH/LOW verified |

Stopping after A or B is a fine outcome.

## Open questions for the owner

1. Should the bridge be an **optional extra** (recommended) or eventually the only path?
2. If the bridge is down, should the device **fall back to direct** (needs the token on the card again) or just show "unavailable"?
3. Do you want the glucose and server-status screens at all on the e-reader?
4. Which model and daily cap for e-reader prompts (server-side settings)?

## Where the truth lives

The server design, risk register, and the account-policy analysis are maintained in the private `extraction` repo. If this doc and that one disagree, that one wins; update this one.
