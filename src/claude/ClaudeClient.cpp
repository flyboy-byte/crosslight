#include "ClaudeClient.h"

#include <ArduinoJson.h>
#include <HalMemory.h>
#include <HalStorage.h>
#include <Logging.h>
#include <SecureHttpClient.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

#include "network/HttpDownloader.h"

namespace claude {

namespace {
constexpr char kUrl[] = "https://api.anthropic.com/v1/messages";
constexpr char kUsageProbeModel[] = "claude-haiku-4-5";  // throwaway request; cheapest model that returns headers
// Opus (claude-opus-5-5) 429'd with a genuine rate_limit_error on every attempt, idle gaps included --
// Logan's subscription tier doesn't carry Opus access via this OAuth token. Haiku confirmed working
// live 2026-10-05; Logan chose it over Sonnet for Ask Claude/Passage Q&A without testing Sonnet further.
constexpr char kAskModel[] = "claude-haiku-4-5";
constexpr uint32_t kTimeoutMs = 30000;  // ask() replies can take longer than the 1-token usage probe

// Google Trust Services "GTS Root R4" (api.anthropic.com chains to it). Pinned
// as the single trust anchor so the bearer token never rides an unverified TLS
// session -- the firmware's own HttpDownloader/KOReaderSyncClient both call
// setInsecure() instead, despite HttpDownloader.h's stale "CA-verified" claim
// (see docs/crosslight/claude-panel.md "Findings"). Expires 2036-06-22; the
// server may present the GlobalSign cross-signed variant, which still
// validates to this subject.
constexpr char kRootCa[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIICCTCCAY6gAwIBAgINAgPlwGjvYxqccpBQUjAKBggqhkjOPQQDAzBHMQswCQYD\n"
    "VQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIG\n"
    "A1UEAxMLR1RTIFJvb3QgUjQwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAwMDAw\n"
    "WjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2Vz\n"
    "IExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjQwdjAQBgcqhkjOPQIBBgUrgQQAIgNi\n"
    "AATzdHOnaItgrkO4NcWBMHtLSZ37wWHO5t5GvWvVYRg1rkDdc/eJkTBa6zzuhXyi\n"
    "QHY7qca4R9gq55KRanPpsXI5nymfopjTX15YhmUPoYRlBtHci8nHc8iMai/lxKvR\n"
    "HYqjQjBAMA4GA1UdDwEB/wQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQW\n"
    "BBSATNbrdP9JNqPV2Py1PsVq8JQdjDAKBggqhkjOPQQDAwNpADBmAjEA6ED/g94D\n"
    "9J+uHXqnLrmvT/aDHQ4thQEd0dlq7A/Cr8deVl5c1RxYIigL9zC2L7F8AjEA8GE8\n"
    "p/SgguMh1YQdc4acLa/KNJvxn7kjNuK8YAOdgLOaVsjh4rsUecrNIdSUtUlD\n"
    "-----END CERTIFICATE-----\n"
;

// Days since 1970-01-01 for a civil date (Howard Hinnant's algorithm), so the
// Date header parses without timegm/strptime portability questions.
long long daysFromCivil(long long y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<long long>(doe) - 719468;
}

// "Sun, 06 Nov 1994 08:49:37 GMT" -> unix seconds, 0 on any mismatch.
long long parseHttpDate(const std::string& s) {
  static const char* kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  char mon[4] = {0};
  int day = 0, year = 0, hh = 0, mm = 0, ss = 0;
  if (sscanf(s.c_str(), "%*3s, %d %3s %d %d:%d:%d", &day, mon, &year, &hh, &mm, &ss) != 6) return 0;
  for (unsigned i = 0; i < 12; i++) {
    if (strncmp(mon, kMonths[i], 3) == 0) {
      return daysFromCivil(year, i + 1, static_cast<unsigned>(day)) * 86400LL + hh * 3600 + mm * 60 + ss;
    }
  }
  return 0;
}

std::string readToken() {
  std::string token = Storage.readFile(kTokenPath).c_str();
  while (!token.empty() && (token.back() == '\n' || token.back() == '\r' || token.back() == ' ')) token.pop_back();
  return token;
}

// Raw outcome of one POST, generalized so ask() and fetchUsage() can each pull
// what they need (body text vs. response headers) from the same request path.
struct RawResponse {
  Error error = Error::Network;
  int httpCode = 0;
  std::string body;
  // Only anthropic-ratelimit-* and date are kept; everything else is noise we
  // don't need to hold onto after the connection closes.
  std::vector<std::pair<std::string, std::string>> headers;
};

std::string findHeader(const RawResponse& r, const std::string& name) {
  for (const auto& h : r.headers) {
    if (h.first == name) return h.second;
  }
  return "";
}

RawResponse sendRequest(const std::string& bodyJson) {
  RawResponse out;
  const std::string token = readToken();
  if (token.empty()) {
    out.error = Error::NoToken;
    return out;
  }
  if (ESP.getFreeHeap() < HttpDownloader::MIN_TLS_FREE_HEAP ||
      ESP.getMaxAllocHeap() < HttpDownloader::MIN_TLS_MAX_ALLOC) {
    out.error = Error::LowMemory;
    return out;
  }

  freeink::SecureHttpClient http;
  http.setCACert(kRootCa);
  http.setTimeout(kTimeoutMs);
  if (!http.begin(std::string(kUrl))) return out;
  http.addHeader("Authorization", "Bearer " + token);
  http.addHeader("anthropic-beta", "oauth-2025-04-20");
  http.addHeader("anthropic-version", "2023-06-01");
  http.addHeader("content-type", "application/json");
  const int code = http.POST(bodyJson);
  out.httpCode = code;
  if (code <= 0) {
    http.end();
    out.error = Error::Network;
    return out;
  }
  for (const auto& h : http.getHeaders()) {
    if (h.first.rfind("anthropic-ratelimit", 0) == 0 || h.first == "date") out.headers.push_back(h);
  }
  out.body = http.getString();
  http.end();

  if (code == 401 || code == 403) {
    out.error = Error::Unauthorized;
  } else if (code >= 200 && code < 300) {
    out.error = Error::Ok;
  } else {
    out.error = Error::Http;
  }
  return out;
}

Window parseWindow(const RawResponse& r, const char* prefix) {
  Window w;
  const std::string p = std::string("anthropic-ratelimit-unified-") + prefix;
  const std::string util = findHeader(r, p + "-utilization");
  const std::string reset = findHeader(r, p + "-reset");
  if (!util.empty()) w.utilization = strtof(util.c_str(), nullptr);
  if (!reset.empty()) w.resetEpoch = strtoll(reset.c_str(), nullptr, 10);
  w.status = findHeader(r, p + "-status");
  return w;
}
}  // namespace

AskResult ask(const std::string& prompt, const int maxTokens) {
  AskResult out;

  JsonDocument doc;
  doc["model"] = kAskModel;
  doc["max_tokens"] = maxTokens;
  JsonArray messages = doc["messages"].to<JsonArray>();
  JsonObject msg = messages.add<JsonObject>();
  msg["role"] = "user";
  msg["content"] = prompt;
  std::string body;
  serializeJson(doc, body);

  const RawResponse raw = sendRequest(body);
  out.error = raw.error;
  out.httpCode = raw.httpCode;
  if (raw.error != Error::Ok) {
    LOG_ERR("CLAUDE", "ask: error=%d http=%d body=%s", static_cast<int>(raw.error), raw.httpCode, raw.body.c_str());
    return out;
  }

  JsonDocument resp;
  const DeserializationError parseErr = deserializeJson(resp, raw.body);
  if (parseErr) {
    LOG_ERR("CLAUDE", "ask: response parse failed: %s", parseErr.c_str());
    out.error = Error::Http;
    return out;
  }
  for (JsonObject block : resp["content"].as<JsonArray>()) {
    if (block["type"] == "text") out.text += block["text"].as<const char*>();
  }
  return out;
}

UsageResult fetchUsage(Usage& usage) {
  UsageResult out;
  JsonDocument doc;
  doc["model"] = kUsageProbeModel;
  doc["max_tokens"] = 1;
  JsonArray messages = doc["messages"].to<JsonArray>();
  JsonObject msg = messages.add<JsonObject>();
  msg["role"] = "user";
  msg["content"] = "hi";
  std::string body;
  serializeJson(doc, body);

  const RawResponse raw = sendRequest(body);
  out.error = raw.error;
  out.httpCode = raw.httpCode;
  LOG_INF("CLAUDE", "fetchUsage: error=%d http=%d", static_cast<int>(raw.error), raw.httpCode);
  for (const auto& h : raw.headers) LOG_INF("CLAUDE", "%s: %s", h.first.c_str(), h.second.c_str());
  if (raw.error != Error::Ok) return out;

  Usage parsed;
  parsed.fiveHour = parseWindow(raw, "5h");
  parsed.sevenDay = parseWindow(raw, "7d");
  parsed.serverNowEpoch = parseHttpDate(findHeader(raw, "date"));
  usage = parsed;
  return out;
}

}  // namespace claude
