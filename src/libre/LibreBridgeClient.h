#pragma once

#include <string>

// Client for Logan's own Libre glucose bridge (personal VPS project, see
// ~/projects/extraction/docs/agent-api.md for the full API contract this
// mirrors). Read-only, GET-only, one bearer key.
//
// Config lives on the SD card, same pattern as claude::kTokenPath: the
// bridge's own base URL isn't fixed (unlike api.anthropic.com) because the
// VPS deploy is still pending as of 2026-10-12, so it has to be settable
// without a firmware rebuild.
namespace libre {

constexpr char kUrlPath[] = "/libre/url.txt";
constexpr char kTokenPath[] = "/libre/token.txt";

enum class Error { Ok, NoConfig, LowMemory, Network, Unauthorized, Http, NoData };

// Mirrors GET /api/v1/latest/glucose's status field exactly -- see
// agent-api.md "Status": never turn HIGH/LOW/UNAVAILABLE into a number, and
// STALE must not be presented as current.
enum class ReadingStatus { Ok, High, Low, Stale, Unavailable };

enum class Trend { FallingFast, Falling, Steady, Rising, RisingFast, Unknown };

struct Reading {
  int value = 0;  // meaningless unless status == Ok or Stale
  std::string unit;
  Trend trend = Trend::Unknown;
  ReadingStatus status = ReadingStatus::Unavailable;
  int ageS = 0;  // seconds since source_time, as of the request
};

struct FetchResult {
  Error error = Error::Network;
  int httpCode = 0;
  Reading reading;
};

// One GET against <base-url>/api/v1/latest/glucose. Never calls this more
// than the caller asks it to -- the server already enforces a per-key/IP
// rate limit (120 req/min) and a 10-bad-key lockout, so callers must not
// retry on Error::Unauthorized (see agent-api.md "Etiquette and limits").
FetchResult fetchLatest();

}  // namespace libre
