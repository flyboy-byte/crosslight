#include "ClaudeBridgeClient.h"

#include <ArduinoJson.h>
#include <HalMemory.h>
#include <HalStorage.h>
#include <Logging.h>
#include <SecureHttpClient.h>

#include <cstdlib>
#include <cstring>

#include "network/HttpDownloader.h"

namespace claude {

namespace {

// ISRG Root X1 + X2, concatenated -- the bridge's Let's Encrypt chain
// (libre::LibreBridgeClient's own pin, same server) and the X2 cross-sign
// extraction flagged as "migration in progress" as of 2026-10-18. wolfSSL's
// PEM loader (wolfSSL_CTX_load_verify_buffer) accepts more than one
// CERTIFICATE block per buffer, same as any CA bundle file, so pinning both
// up front means a server-side root switch mid-migration doesn't need a
// firmware update. Both fetched directly from letsencrypt.org/certs/ and
// fingerprint-verified (sha256) against Let's Encrypt's published values,
// not transcribed, for the same reason LibreBridgeClient's comment gives.
constexpr char kRootCa[] =
    // ISRG Root X1 -- expires 2035-06-04.
    "-----BEGIN CERTIFICATE-----\n"
    "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\n"
    "-----END CERTIFICATE-----\n"
    // ISRG Root X2 -- expires 2040-09-17.
    "-----BEGIN CERTIFICATE-----\n"
    "MIICGzCCAaGgAwIBAgIQQdKd0XLq7qeAwSxs6S+HUjAKBggqhkjOPQQDAzBPMQsw\n"
    "CQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJuZXQgU2VjdXJpdHkgUmVzZWFyY2gg\n"
    "R3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBYMjAeFw0yMDA5MDQwMDAwMDBaFw00\n"
    "MDA5MTcxNjAwMDBaME8xCzAJBgNVBAYTAlVTMSkwJwYDVQQKEyBJbnRlcm5ldCBT\n"
    "ZWN1cml0eSBSZXNlYXJjaCBHcm91cDEVMBMGA1UEAxMMSVNSRyBSb290IFgyMHYw\n"
    "EAYHKoZIzj0CAQYFK4EEACIDYgAEzZvVn4CDCuwJSvMWSj5cz3es3mcFDR0HttwW\n"
    "+1qLFNvicWDEukWVEYmO6gbf9yoWHKS5xcUy4APgHoIYOIvXRdgKam7mAHf7AlF9\n"
    "ItgKbppbd9/w+kHsOdx1ymgHDB/qo0IwQDAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0T\n"
    "AQH/BAUwAwEB/zAdBgNVHQ4EFgQUfEKWrt5LSDv6kviejM9ti6lyN5UwCgYIKoZI\n"
    "zj0EAwMDaAAwZQIwe3lORlCEwkSHRhtFcP9Ymd70/aTSVaYgLXTWNLxBo1BfASdW\n"
    "tL4ndQavEi51mI38AjEAi/V3bNTIZargCyzuFJ0nN6T5U6VR5CmD1/iQMVtCnwr1\n"
    "/q4AaOeMSQ+2b1tbFfLn\n"
    "-----END CERTIFICATE-----\n"
;

// Server's own cap (unified-api.md "claude:prompt limits"); client timeout
// matches it rather than guessing shorter, since a real call can legitimately
// take the server's full budget under load.
constexpr uint32_t kBridgeTimeoutMs = 30000;

std::string readTrimmed(const char* path) {
  std::string s = Storage.readFile(path).c_str();
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
  return s;
}

std::string trimTrailingSlash(std::string s) {
  while (!s.empty() && s.back() == '/') s.pop_back();
  return s;
}

// Both bridge::prompt and bridge::usage need the same SD read + heap check;
// factored once so each call site just checks baseUrl/key non-empty.
struct BridgeConfig {
  std::string baseUrl;
  std::string key;
  bool ok() const { return !baseUrl.empty() && !key.empty(); }
};

BridgeConfig readConfig() {
  BridgeConfig cfg;
  cfg.baseUrl = trimTrailingSlash(readTrimmed(kBridgeUrlPath));
  cfg.key = readTrimmed(kBridgeKeyPath);
  return cfg;
}

// Shared outcome of one request, before the two callers (prompt/usage)
// extract what they each need from the body.
struct RawResponse {
  BridgeError error = BridgeError::Network;
  int httpCode = 0;
  std::string body;
  int retryAfterS = 0;
};

// Maps one HTTP status (plus, for ambiguous codes, the body's `error` field)
// to a BridgeError -- unified-api.md's own error table, not a local guess.
// `errorCode` out-param carries the body's raw `error` string when present,
// so callers can show the server's own wording if they want to.
BridgeError classify(int code, const std::string& body, std::string& errorCode) {
  JsonDocument doc;
  if (!body.empty() && !deserializeJson(doc, body)) {
    if (doc["error"].is<const char*>()) errorCode = doc["error"].as<const char*>();
  }
  switch (code) {
    case 400:
      return BridgeError::BadRequest;
    case 401:
      return BridgeError::Unauthorized;
    case 403:
      return BridgeError::Forbidden;
    case 413:
      return BridgeError::TooLarge;
    case 415:
      return BridgeError::UnsupportedMediaType;
    case 429:
      return BridgeError::RateLimited;
    case 502:
      return BridgeError::UpstreamError;
    case 503:
      return BridgeError::Unavailable;
    default:
      return BridgeError::Http;
  }
}

RawResponse sendRequest(const BridgeConfig& cfg, const char* path, const char* method, const std::string& body) {
  RawResponse out;
  if (ESP.getFreeHeap() < HttpDownloader::MIN_TLS_FREE_HEAP ||
      ESP.getMaxAllocHeap() < HttpDownloader::MIN_TLS_MAX_ALLOC) {
    out.error = BridgeError::LowMemory;
    return out;
  }

  freeink::SecureHttpClient http;
  http.setCACert(kRootCa);
  http.setTimeout(kBridgeTimeoutMs);
  if (!http.begin(cfg.baseUrl + path)) {
    out.error = BridgeError::Network;
    return out;
  }
  http.addHeader("Authorization", "Bearer " + cfg.key);
  http.addHeader("Accept", "application/json");
  int code;
  if (strcmp(method, "POST") == 0) {
    http.addHeader("Content-Type", "application/json");
    code = http.POST(body);
  } else {
    code = http.GET();
  }
  out.httpCode = code;
  if (code <= 0) {
    http.end();
    out.error = BridgeError::Network;
    return out;
  }
  const std::string retryAfter = http.getHeader("retry-after");
  if (!retryAfter.empty()) out.retryAfterS = atoi(retryAfter.c_str());
  out.body = http.getString();
  http.end();

  if (code >= 200 && code < 300) {
    out.error = BridgeError::Ok;
  } else {
    std::string errorCode;
    out.error = classify(code, out.body, errorCode);
  }
  return out;
}

}  // namespace

bool bridgeConfigured() { return readConfig().ok(); }

PromptResult promptViaBridge(const std::string& prompt) {
  PromptResult out;
  const BridgeConfig cfg = readConfig();
  if (!cfg.ok()) {
    out.error = BridgeError::NoConfig;
    return out;
  }

  JsonDocument doc;
  doc["prompt"] = prompt;
  std::string body;
  serializeJson(doc, body);

  const RawResponse raw = sendRequest(cfg, "/api/v1/claude/prompt", "POST", body);
  out.error = raw.error;
  out.httpCode = raw.httpCode;
  out.retryAfterS = raw.retryAfterS;
  if (raw.error != BridgeError::Ok) {
    JsonDocument errDoc;
    if (!raw.body.empty() && !deserializeJson(errDoc, raw.body) && errDoc["error"].is<const char*>()) {
      out.errorCode = errDoc["error"].as<const char*>();
    }
    LOG_ERR("CLAUDE_BRIDGE", "prompt: error=%d http=%d code=%s", static_cast<int>(raw.error), raw.httpCode,
            out.errorCode.c_str());
    return out;
  }

  JsonDocument resp;
  if (deserializeJson(resp, raw.body) || !resp["reply"].is<const char*>()) {
    LOG_ERR("CLAUDE_BRIDGE", "prompt: response parse failed");
    out.error = BridgeError::Http;
    return out;
  }
  out.reply = resp["reply"].as<const char*>();
  return out;
}

namespace {
UsageWindow parseUsageWindow(JsonVariantConst v) {
  UsageWindow w;
  if (!v["used_pct"].isNull()) {
    w.hasUsedPct = true;
    w.usedPct = v["used_pct"].as<int>();
  }
  if (v["resets_at"].is<const char*>()) w.resetsAt = v["resets_at"].as<const char*>();
  return w;
}
}  // namespace

UsageResult fetchUsageViaBridge(BridgeUsage& usage) {
  UsageResult out;
  const BridgeConfig cfg = readConfig();
  if (!cfg.ok()) {
    out.error = BridgeError::NoConfig;
    return out;
  }

  const RawResponse raw = sendRequest(cfg, "/api/v1/latest/claude-usage", "GET", "");
  out.error = raw.error;
  out.httpCode = raw.httpCode;
  out.retryAfterS = raw.retryAfterS;
  if (raw.error != BridgeError::Ok) {
    JsonDocument errDoc;
    if (!raw.body.empty() && !deserializeJson(errDoc, raw.body) && errDoc["error"].is<const char*>()) {
      out.errorCode = errDoc["error"].as<const char*>();
    }
    LOG_ERR("CLAUDE_BRIDGE", "usage: error=%d http=%d code=%s", static_cast<int>(raw.error), raw.httpCode,
            out.errorCode.c_str());
    return out;
  }

  JsonDocument resp;
  if (deserializeJson(resp, raw.body)) {
    LOG_ERR("CLAUDE_BRIDGE", "usage: response parse failed");
    out.error = BridgeError::Http;
    return out;
  }
  BridgeUsage parsed;
  parsed.fiveHour = parseUsageWindow(resp["five_hour"]);
  parsed.sevenDay = parseUsageWindow(resp["seven_day"]);
  parsed.state = resp["state"].is<const char*>() ? resp["state"].as<const char*>() : "";
  parsed.ageS = resp["age_s"].as<int>();
  usage = parsed;
  return out;
}

}  // namespace claude
