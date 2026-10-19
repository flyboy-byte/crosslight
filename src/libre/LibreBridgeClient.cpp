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

// "ISRG Root X1" -- Let's Encrypt's self-signed trust anchor. The bridge
// (bridge.flyboybyte.com, live as of 2026-10-13) terminates TLS with a
// Let's Encrypt cert; pinning this root instead of calling setInsecure()
// (HttpDownloader/KOReaderSyncClient's usual fallback) follows the same
// precedent as claude::ClaudeClient -- a bearer token carrying someone's
// real glucose data shouldn't ride an unverified TLS session when the
// actual CA is known. The server's chain (leaf -> intermediate -> a
// cross-signed "ISRG Root X2") terminates here; this root doesn't need to
// change unless the bridge's CA changes. Expires 2035-06-04. Fetched
// directly from https://letsencrypt.org/certs/isrgrootx1.pem, not
// transcribed, to avoid a silent transcription error breaking validation.
constexpr char kRootCa[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
    "TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
    "cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
    "WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
    "ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
    "MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
    "h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
    "0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
    "A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
    "T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
    "B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
    "B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
    "KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
    "OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
    "jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
    "qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
    "rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
    "HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
    "hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
    "ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
    "3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
    "NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
    "ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
    "TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
    "jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
    "oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
    "4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
    "mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
    "emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
    "-----END CERTIFICATE-----\n"
;

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
  http.setCACert(kRootCa);
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

Error fetchHistory(std::vector<HistoryPoint>& out) {
  out.clear();
  const std::string baseUrl = trimTrailingSlash(readTrimmed(kUrlPath));
  const std::string token = readTrimmed(kTokenPath);
  if (baseUrl.empty() || token.empty()) return Error::NoConfig;

  if (ESP.getFreeHeap() < HttpDownloader::MIN_TLS_FREE_HEAP ||
      ESP.getMaxAllocHeap() < HttpDownloader::MIN_TLS_MAX_ALLOC) {
    return Error::LowMemory;
  }

  freeink::SecureHttpClient http;
  http.setCACert(kRootCa);
  http.setTimeout(15000);
  if (!http.begin(baseUrl + "/api/v1/glucose")) return Error::Network;
  http.addHeader("Authorization", "Bearer " + token);
  http.addHeader("Accept", "application/json");

  HistoryScanner scanner(out);
  const int code = http.GET([&scanner](const uint8_t* data, size_t len) {
    scanner.feed(reinterpret_cast<const char*>(data), len);
    return true;
  });
  http.end();

  if (code <= 0) return Error::Network;
  if (code == 401) return Error::Unauthorized;
  if (code == 404) return Error::NoData;
  if (code < 200 || code >= 300) {
    LOG_ERR("LIBRE", "fetchHistory: http=%d", code);
    return Error::Http;
  }
  LOG_INF("LIBRE", "history: %u points", static_cast<unsigned>(out.size()));
  return out.empty() ? Error::NoData : Error::Ok;
}

}  // namespace libre
