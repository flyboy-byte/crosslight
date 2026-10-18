#pragma once

#include <string>

// Second ClaudeClient backend: Logan's own unified API on bridge.flyboybyte.com
// (private `extraction` repo; firmware-side contract mirrored in
// docs/crosslight/unified-api.md), live as of 2026-10-18. Mirrors
// libre::LibreBridgeClient's shape -- same server, same SD-config pattern,
// same ISRG-root pin -- but a different key (scoped read:usage/claude:prompt,
// not read:glucose) and a different pair of routes.
//
// This is additive: claude::ask()/fetchUsage() (direct to api.anthropic.com,
// ClaudeClient.h) are untouched. Nothing calls into this yet -- wiring Ask
// Claude / Bible Passage Q&A to prefer this over the direct path is a
// separate step (unified-api.md Phase C), once this backend itself is
// verified on real hardware (Phase A/B).
namespace claude {

constexpr char kBridgeUrlPath[] = "/bridge/url.txt";
constexpr char kBridgeKeyPath[] = "/bridge/key.txt";

// Mirrors the server's own fixed error codes (unified-api.md "claude:prompt
// limits") so callers can show the right thing without re-deriving it from an
// HTTP status alone -- several codes (429, 503) are only distinguishable by
// the body's `error` field, not the status line.
enum class BridgeError {
  Ok,
  NoConfig,              // /bridge/url.txt or key.txt missing/empty
  LowMemory,
  Network,
  Unauthorized,          // 401: expired or revoked key
  Forbidden,             // 403: valid key, missing scope
  BadRequest,            // 400: bad_prompt / bad_request
  TooLarge,              // 413: prompt over 4000 chars / body over 8 KB
  UnsupportedMediaType,  // 415: json_only
  RateLimited,           // 429: see errorCode (busy/too_soon/key_cap/daily_cap/rate_limited)
  Unavailable,           // 503: unavailable/disabled, or a 24h account-failure lockout
  UpstreamError,         // 502: upstream_error
  Http,                  // any other non-2xx, or a response body that didn't parse
};

// `reply` is the model's text, up to 4000 chars (bridge cap, well inside
// device RAM). `errorCode` is the server's short word (e.g. "too_soon",
// "daily_cap") when the body carried one; `retryAfterS` is the Retry-After
// header's value on a 429/503 lockout -- show it, never retry in a loop
// (unified-api.md's own instruction, not a local guess).
struct PromptResult {
  BridgeError error = BridgeError::Network;
  int httpCode = 0;
  std::string reply;
  std::string errorCode;
  int retryAfterS = 0;
};

// POST /api/v1/claude/prompt. One-shot, stateless -- the server has no memory
// of earlier calls, so `prompt` must carry whatever context the caller wants
// answered. Caps enforced server-side, not re-checked here: 4000 chars / 8 KB
// body in, 4000 chars back. Client timeout matches the server's own 30s cap
// (kBridgeTimeoutMs below), comfortably above the typical 1.5-3s round trip.
PromptResult promptViaBridge(const std::string& prompt);

// One rate-limit-style window from GET /api/v1/latest/claude-usage. usedPct
// absent (hasUsedPct=false) and resetsAt empty both mean the server reported
// `null` for that field -- show "unknown", not a stale number, same as the
// age_s/stale rule everywhere else in this contract.
struct UsageWindow {
  bool hasUsedPct = false;
  int usedPct = 0;       // 0-100, meaningless unless hasUsedPct
  std::string resetsAt;  // ISO 8601 UTC, e.g. "2026-10-18T03:50:00.000Z"; empty = unknown
};

struct BridgeUsage {
  UsageWindow fiveHour;
  UsageWindow sevenDay;
  std::string state;  // "allowed", or a short word when limited
  int ageS = 0;
};

struct UsageResult {
  BridgeError error = BridgeError::Network;
  int httpCode = 0;
  std::string errorCode;  // e.g. "unknown" on a 503 -- show as unknown, not stale
  int retryAfterS = 0;
};

// GET /api/v1/latest/claude-usage. Cached ~10 min server-side, probed at most
// once per 5 min upstream of that -- callers should poll on demand or every
// 5-10 min, not faster; nothing is gained and it just burns the device's own
// battery/radio budget for no fresher data.
UsageResult fetchUsageViaBridge(BridgeUsage& usage);

// True when both /bridge/url.txt and /bridge/key.txt are present and
// non-empty. Callers (once Phase C wires this in) use this to decide whether
// the bridge path is even worth attempting before falling back to direct.
bool bridgeConfigured();

}  // namespace claude
