#pragma once

#include <string>

// Shared Claude API client for every CrossLight Claude feature (Claude Panel,
// Bible Passage Q&A, Ask Claude). One POST to the Messages API per call, over
// the subscription OAuth token read from /claude/token.txt -- NEVER a
// pay-per-token API key (two different payments on Logan's account; this
// constraint is enforced in exactly one place, here). See
// docs/crosslight/claude-panel.md and docs/crosslight/claude-features.md.
namespace claude {

constexpr char kTokenPath[] = "/claude/token.txt";

// "Generous (~1-2 paragraphs)" cap, decided for both Bible Passage Q&A and Ask
// Claude: enough room for a real answer without turning into a long e-ink
// page-turn scroll. This bounds cost/length; display-side pagination (see
// each caller) handles whatever length comes back within it.
constexpr int kDefaultMaxTokens = 500;

enum class Error { Ok, NoToken, LowMemory, Network, Unauthorized, Http };

// A free-form question/answer request. `text` holds the full response body
// text (concatenation of every text content block) on Ok; untouched otherwise.
struct AskResult {
  Error error = Error::Network;
  int httpCode = 0;
  std::string text;
};

// One authed request: `prompt` becomes a single user message. `maxTokens`
// caps the reply length (and therefore cost) -- callers should size this to
// roughly what will paginate sensibly, not rely on the model to self-limit.
AskResult ask(const std::string& prompt, int maxTokens = kDefaultMaxTokens);

// One rate-limit window. utilization is a 0..1 fraction; resetEpoch is unix
// seconds (0 = unknown); status is the raw header ("allowed",
// "allowed_warning", "rejected", ...).
struct Window {
  float utilization = 0.0f;
  long long resetEpoch = 0;
  std::string status;
};

struct Usage {
  Window fiveHour;
  Window sevenDay;
  // Server clock from the response Date header, so "resets in" needs no
  // on-device clock. 0 = unknown.
  long long serverNowEpoch = 0;
};

struct UsageResult {
  Error error = Error::Network;
  int httpCode = 0;
};

// Thin wrapper over ask(): a 1-token probe request, reading the
// anthropic-ratelimit-unified-* response headers instead of the body.
UsageResult fetchUsage(Usage& usage);

}  // namespace claude
