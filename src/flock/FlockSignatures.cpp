#include "FlockSignatures.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cctype>

namespace flock {

namespace {
constexpr size_t MAX_SIGNATURE_BYTES = 64 * 1024;

// Parse "AA:BB:CC" (or "AABBCC", or with '-') into a 24-bit OUI. Returns false
// on anything that is not exactly three hex bytes.
bool parseOui(const char* text, uint32_t& out) {
  if (!text) return false;
  uint32_t value = 0;
  int nibbles = 0;
  for (const char* p = text; *p; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (c == ':' || c == '-') continue;
    if (!std::isxdigit(c)) return false;
    const int d = c <= '9' ? c - '0' : (std::tolower(c) - 'a' + 10);
    value = (value << 4) | static_cast<uint32_t>(d);
    if (++nibbles > 6) return false;
  }
  if (nibbles != 6) return false;
  out = value;
  return true;
}
}  // namespace

bool loadSignatures(const char* path, std::vector<Signature>& out) {
  const String raw = Storage.readFile(path);
  if (raw.isEmpty()) {
    LOG_INF("FLOCK", "No signature file at %s", path);
    return false;
  }
  if (raw.length() > MAX_SIGNATURE_BYTES) {
    LOG_ERR("FLOCK", "Signature file %s too large (%u bytes)", path, static_cast<unsigned>(raw.length()));
    return false;
  }

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, raw.c_str());
  if (err) {
    LOG_ERR("FLOCK", "Signature file %s parse error: %s", path, err.c_str());
    return false;
  }

  std::vector<Signature> parsed;
  for (JsonObjectConst entry : doc["signatures"].as<JsonArrayConst>()) {
    Signature sig;
    sig.name = entry["name"] | "Unknown";
    sig.category = entry["category"] | "";
    for (JsonVariantConst oui : entry["ouis"].as<JsonArrayConst>()) {
      uint32_t value = 0;
      if (parseOui(oui.as<const char*>(), value)) {
        sig.ouis.push_back(value);
      } else {
        LOG_ERR("FLOCK", "Skipping bad OUI in %s", sig.name.c_str());
      }
    }
    for (JsonVariantConst ssid : entry["ssid_contains"].as<JsonArrayConst>()) {
      if (const char* s = ssid.as<const char*>()) sig.ssidContains.emplace_back(s);
    }
    // A signature with no criteria would match nothing (the matcher guards
    // this), so drop it here rather than carry dead weight.
    if (!sig.ouis.empty() || !sig.ssidContains.empty()) parsed.push_back(std::move(sig));
  }

  LOG_INF("FLOCK", "Loaded %u signatures from %s", static_cast<unsigned>(parsed.size()), path);
  out = std::move(parsed);
  return !out.empty();
}

}  // namespace flock
