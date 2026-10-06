# Claude-powered reading features — planning doc

**Status: WORKING ON REAL HARDWARE, confirmed 2026-10-05.** Release 26.10.8's TLS layer was broken on
real hardware (two wolfSSL config gaps — see `docs/crosslight/claude-panel.md` "Hardware debugging");
fixed in **26.10.9**, which also fixes the request model: `ask()` was `claude-opus-5-5`, which 429'd
with a genuine `rate_limit_error` every time (Logan's subscription tier doesn't carry Opus access via
this OAuth token, confirmed by swapping models live on-device) — now permanently `claude-haiku-4-5`,
confirmed working end-to-end (HTTP 200) for Ask Claude. Bible Passage Q&A shares the same `ask()` path
so the same fixes apply, though its own on-device confirmation only got as far as the TLS layer working
(handshake ok) before the model swap — re-verify after 26.10.9 if it matters. Token already on the SD
card (format-checked — see `docs/crosslight/claude-panel.md`). Two features, sharing one HTTP/auth
client with Claude Panel
(`docs/crosslight/claude-panel.md`): **Bible Passage Q&A** (in-reader, asks about whatever's on screen)
and **Ask Claude** (a standalone free-prompt Utilities tile, with an on-SD history log). Both use the
**subscription OAuth token**, never a pay-per-token API key — same constraint as Claude Panel, same
reason (Logan's Claude subscription and API billing are two different payments; this only ever touches
the subscription).

## What's built (2026-10-05)

- **`src/claude/ClaudeClient.*`** — replaces `ClaudeUsageClient`. `ask(prompt, maxTokens)` for free-form
  requests (JSON built/parsed with ArduinoJson, not string concatenation); `fetchUsage()` is now a thin
  wrapper calling `ask()`-shaped plumbing with a 1-token probe. Same pinned-CA (GTS Root R4), same
  Bearer+oauth-beta auth path, single source of truth. `kDefaultMaxTokens = 500` is the "generous"
  response cap.
- **`src/claude/ClaudeAnswerPager.*`** — shared paragraph-wrapping pagination (splits on blank lines,
  wraps each paragraph, pages with the same PageBack/PageForward handling Compare Translations uses).
  Used by both features' answer screens.
- **`src/claude/ClaudeHistoryStore.*`** — append-only JSONL log at `/claude/ask-history.jsonl`,
  drop-oldest-wholesale past a 16KB cap (mirrors `src/util/PluginEvents.cpp`'s outbox pattern). Read back
  for display only, never fed into a new request.
- **`src/activities/bible/BiblePassageQaActivity.*`** — Range → Questions → Answer screens. Reached via
  a new "Ask Claude" row in `BibleMenuActivity` (the reader's existing Confirm menu). Defaults to the
  current page's verses (computed in `BibleReaderActivity::openAskClaude()` from `versePages`); page-turn
  buttons page the passage/answer text, long-press widens the verse range (same fix pattern as Compare
  Translations). Presets: Explain this / Historical context / Cross-references / Custom question (opens
  `KeyboardEntryActivity`). Wi-Fi is joined lazily on the first actual question, not on opening the
  screen, so just browsing the range/presets and backing out never touches the radio.
- **`src/activities/utilities/AskClaudeActivity.*`** — new `kGeneral` Utilities tile. Menu → (New
  Question | History). New Question opens the keyboard, asks, shows the paginated answer, and appends it
  to the history log. History lists past questions (most-recent-first, scrollable) and replays a
  selected entry read-only — no new request.

---

## Shared foundation: `ClaudeClient` (refactor before building either feature)

`src/claude/ClaudeUsageClient.*` (built for Claude Panel) currently hardcodes one specific request (the
tiny rate-limit probe). Pull the auth/TLS/request plumbing out into a general `ClaudeClient` that all
three Claude features call — so the "always Bearer + oauth beta header, never x-api-key" guarantee lives
in exactly one place instead of being re-typed three times:

- `ClaudeClient::ask(prompt, maxTokens) -> Result` — one POST to `/v1/messages`, returns the response
  text (or an error). Reuses the pinned-CA `SecureHttpClient` setup, the SD token read, the same error
  enum shape (`NoToken`/`Unauthorized`/`LowMemory`/`Network`/`Http`/`Ok`) already defined for the usage
  probe.
- `ClaudeUsageClient::fetchUsage()` becomes a thin wrapper: call `ClaudeClient` with `max_tokens=1`,
  read the rate-limit headers off the same response. No behavior change for Claude Panel.
- Response cap: **generous (~1-2 paragraphs)** for both new features — enough room for a real answer
  without turning into a long e-ink page-turn scroll. Set via `max_tokens` on the request, not by asking
  the model to "fit the screen" (fragile; model output length doesn't reliably match a given font's
  character width). Whatever comes back, however long within that cap, is paginated for display (see
  below) — pagination handles length, the cap handles cost/scroll-depth, two separate concerns.

## Feature 1: Bible Passage Q&A

**Entry point: the in-reader Confirm menu (`BibleMenuActivity`), not Bible Hub.** Compare Translations'
real friction (confirmed by reading the code) is that it's reached from Bible Hub and only seeds
book+chapter, not verse — you still leave the reader and re-pick verse every time. Passage Q&A avoids
that entirely: add an "Ask Claude" row to the existing Confirm-tap menu the reader already has (same
place Bookmarks/Toggle Bookmark live), so you never leave the reader.

**Default passage: the current page's verses.** The reader already tracks `versePages`/
`currentPageIndex` for bookmarking — same data, no new lookup. Up/Down widens the range before you send,
if you want more context than what's on screen.

**Question picker: preset list + free text.**
- Explain this
- Historical context
- Cross-references
- (original-language nuance was considered but overlaps with the existing Bible Numbers feature — skip
  it as a preset; it's still reachable via free text)
- "Custom question..." opens the on-screen keyboard (`KeyboardEntryActivity` — confirm it's reusable
  here before building) for anything not covered by a preset.

**Translation context sent with the request:** the prompt includes the installed translation's
abbreviation (e.g. "KJV", "NIV") alongside the verse text, so wording-specific questions (translation
differences, archaic phrasing) get an answer that matches what's actually on screen, not a generic
answer assuming a different translation.

**Response display:** reuse the wrap-into-lines + `turnPage()` pagination pattern already built for
`CompareTranslationsActivity`/`BibleNumberDetailActivity` — same page-turn-button handling as the
Compare Translations fix (page-turn buttons page the answer; verify there's no chapter/verse concept
here to conflict with, so no long-press override needed on this screen).

**No conversation history, no multi-question context:** each question is a fresh one-shot request. Not
needed for Phase 1 — see Feature 2 for where history came up and why it's handled differently.

## Feature 2: Ask Claude (standalone utility)

**A generic free-prompt tile**, independent of any open book — the on-screen-keyboard "just ask
something" utility from the original wishlist. New `kGeneral` Utilities entry, built on the same
`ClaudeClient::ask()`.

**One-shot requests, confirmed.** Each question is independent; no conversation sent back to Claude.
Keeps the request body small and avoids needing extra RAM to hold growing context on a memory-
constrained device.

**Logan asked about history/context on SD separately from the one-shot decision — yes, worth doing,
decided as follow-on scope, not blocking v1:** save each Q&A pair to a local SD log (e.g.
`/claude/ask-history.jsonl` or similar — format TBD at build time) so past questions/answers are
browsable later, **without** feeding that history back into new requests. This gets the "I'd like to
see what I asked before" value Logan wants while keeping every request one-shot and cheap. A "History"
row in the utility lists past entries; selecting one just re-displays it (no new request). Build this
after the core ask/answer flow works — it's a local read/write feature, no new network surface.

**Response display:** same pagination pattern as Feature 1.

---

## Phases

**Phase A — `ClaudeClient` refactor.** Generalize `ClaudeUsageClient` as described above. No behavior
change to Claude Panel; confirm Claude Panel still builds and its header-logging still works after the
refactor.

**Phase B — Bible Passage Q&A.** New "Ask Claude" row in `BibleMenuActivity`, passage-range picker
(reusing reader's page/verse data), preset+free-text question UI, request + paginated response.

**Phase C — Ask Claude utility.** New Utilities tile, keyboard entry (confirm `KeyboardEntryActivity`
reuse), request + paginated response.

**Phase D — SD history log for Ask Claude.** Append-on-answer log file, a "History" list row, read-only
replay of past entries. Not conversation context — purely local record-keeping.

## Verify-before-building checklist

- [x] (2026-10-05) Confirm `KeyboardEntryActivity` is reusable as-is — yes, `KeyboardResult{text}` via
      `std::get<KeyboardResult>(result.data).text`, same pattern `CatalogActivity`'s search uses.
- [x] (2026-10-05) Confirm `BibleMenuActivity` has room for a 4th row — yes, it's a plain `UiListActivity`
      list, no layout rework needed; added `AskClaude` as row 4.
- [x] (2026-10-05) On-SD history log format — JSONL, `{q,a,ts}` per line, drop-oldest-wholesale over
      16KB. Mirrors the existing plugin event outbox convention rather than inventing a new one.
- [ ] Re-check `max_tokens` (500) sizing against what it actually renders to in wrapped lines on the X4
      Pro's real screen — not yet verified on hardware.
- [x] (2026-10-05) Simulator stub parity — `simulator_x4_pro` builds clean, no new stubs needed this time.
- [ ] **NOT host-tested.** Everything built this session is UI/activity code exercising real network
      calls and SD I/O, not pure logic, so it wasn't added to `test/` (host tests stayed 591/591,
      unchanged) — on-device verification is the real check here, same as Claude Panel.

## Credit / origin

Grew directly out of the Claude Panel back-and-forth (2026-10-05) and the Compare Translations UX
review in the same session — not a separate idea, a continuation of "what else is Claude useful for on
this device, at this same small scale."
