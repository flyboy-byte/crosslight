#include "LibreBridgeClient.h"

#include <ArduinoJson.h>
#include <HalMemory.h>
#include <HalStorage.h>
#include <Logging.h>
#include <SecureHttpClient.h>

#include <cstring>

#include "network/HttpDownloader.h"

namespace libre {

namespace {

std::string readTrimmed(const char* path) {
  std::string s = Storage.readFile(path).c_str();
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
  return s;
}

// "https://host" or "https://host/" -> "https://host" (no trailing slash,
// so the request path concatenation below never doubles one up).
std::string trimTrailingSlash(std::string s) {
  while (!s.empty() && s.back() == '/') s.pop_back();
  return s;
}

Trend parseTrend(const char* s) {
  if (!s) return Trend::Unknown;
  if (strcmp(s, "falling_fast") == 0) return Trend::FallingFast;
  if (strcmp(s, "falling") == 0) return Trend::Falling;
  if (strcmp(s, "steady") == 0) return Trend::Steady;
  if (strcmp(s, "rising") == 0) return Trend::Rising;
  if (strcmp(s, "rising_fast") == 0) return Trend::RisingFast;
  return Trend::Unknown;
}

ReadingStatus parseStatus(const char* s) {
  if (!s) return ReadingStatus::Unavailable;
  if (strcmp(s, "OK") == 0) return ReadingStatus::Ok;
  if (strcmp(s, "HIGH") == 0) return ReadingStatus::High;
  if (strcmp(s, "LOW") == 0) return ReadingStatus::Low;
  if (strcmp(s, "STALE") == 0) return ReadingStatus::Stale;
  return ReadingStatus::Unavailable;
}

}  // namespace

FetchResult fetchLatest() {
  FetchResult out;

  const std::string baseUrl = trimTrailingSlash(readTrimmed(kUrlPath));
  const std::string token = readTrimmed(kTokenPath);
  if (baseUrl.empty() || token.empty()) {
    out.error = Error::NoConfig;
    return out;
  }

  if (ESP.getFreeHeap() < HttpDownloader::MIN_TLS_FREE_HEAP ||
      ESP.getMaxAllocHeap() < HttpDownloader::MIN_TLS_MAX_ALLOC) {
    out.error = Error::LowMemory;
    return out;
  }

  freeink::SecureHttpClient http;
  // No pinned CA: the bridge's VPS cert isn't known yet (not deployed as of
  // 2026-10-12). Matches HttpDownloader/KOReaderSyncClient's existing
  // setInsecure() precedent for firmware HTTP clients without a fixed host.
  http.setInsecure();
  http.setTimeout(15000);
  if (!http.begin(baseUrl + "/api/v1/latest/glucose")) {
    out.error = Error::Network;
    return out;
  }
  http.addHeader("Authorization", "Bearer " + token);
  http.addHeader("Accept", "application/json");
  const int code = http.GET();
  out.httpCode = code;
  if (code <= 0) {
    http.end();
    out.error = Error::Network;
    return out;
  }
  const std::string body = http.getString();
  http.end();

  if (code == 401) {
    out.error = Error::Unauthorized;
    return out;
  }
  if (code == 404) {
    out.error = Error::NoData;
    return out;
  }
  if (code < 200 || code >= 300) {
    out.error = Error::Http;
    LOG_ERR("LIBRE", "fetchLatest: http=%d body=%s", code, body.c_str());
    return out;
  }

  JsonDocument doc;
  const DeserializationError parseErr = deserializeJson(doc, body);
  if (parseErr) {
    LOG_ERR("LIBRE", "fetchLatest: parse failed: %s", parseErr.c_str());
    out.error = Error::Http;
    return out;
  }

  Reading r;
  r.status = parseStatus(doc["status"].as<const char*>());
  if (!doc["value"].isNull()) r.value = doc["value"].as<int>();
  r.unit = doc["unit"].is<const char*>() ? doc["unit"].as<const char*>() : "";
  r.trend = parseTrend(doc["trend"].as<const char*>());
  r.ageS = doc["age_s"].as<int>();

  out.error = Error::Ok;
  out.reading = r;
  return out;
}

}  // namespace libre
