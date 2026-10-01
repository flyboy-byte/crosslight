#include "BleSignatures.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cctype>

namespace bleaudit {

namespace {
constexpr size_t MAX_SIGNATURE_BYTES = 64 * 1024;

// Parse a 16-bit hex string ("0x004C", "004C", "4C", lower or upper case) into
// `out`. Returns false on anything that is not 1-4 hex digits (after an optional
// 0x/0X prefix).
bool parseHex16(const char* text, uint16_t& out) {
  if (!text) return false;
  if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text += 2;
  uint32_t value = 0;
  int nibbles = 0;
  for (const char* p = text; *p; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (!std::isxdigit(c)) return false;
    const int d = c <= '9' ? c - '0' : (std::tolower(c) - 'a' + 10);
    value = (value << 4) | static_cast<uint32_t>(d);
    if (++nibbles > 4) return false;  // more than 16 bits
  }
  if (nibbles == 0) return false;
  out = static_cast<uint16_t>(value);
  return true;
}
}  // namespace

bool loadSignatures(const char* path, std::vector<BleSignature>& out) {
  const String raw = Storage.readFile(path);
  if (raw.isEmpty()) {
    LOG_INF("BLEAUDIT", "No signature file at %s", path);
    return false;
  }
  if (raw.length() > MAX_SIGNATURE_BYTES) {
    LOG_ERR("BLEAUDIT", "Signature file %s too large (%u bytes)", path, static_cast<unsigned>(raw.length()));
    return false;
  }

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, raw.c_str());
  if (err) {
    LOG_ERR("BLEAUDIT", "Signature file %s parse error: %s", path, err.c_str());
    return false;
  }

  std::vector<BleSignature> parsed;
  for (JsonObjectConst entry : doc["signatures"].as<JsonArrayConst>()) {
    BleSignature sig;
    sig.name = entry["name"] | "Unknown";
    sig.category = entry["category"] | "";

    uint16_t companyId = 0;
    if (parseHex16(entry["company_id"].as<const char*>(), companyId)) {
      sig.companyId = companyId;
      sig.hasCompanyId = true;
    } else if (!entry["company_id"].isNull()) {
      LOG_ERR("BLEAUDIT", "Skipping bad company_id in %s", sig.name.c_str());
    }

    for (JsonVariantConst uuid : entry["service_uuids"].as<JsonArrayConst>()) {
      uint16_t value = 0;
      if (parseHex16(uuid.as<const char*>(), value)) {
        sig.serviceUuids16.push_back(value);
      } else {
        LOG_ERR("BLEAUDIT", "Skipping bad service_uuid in %s", sig.name.c_str());
      }
    }

    for (JsonVariantConst name : entry["name_contains"].as<JsonArrayConst>()) {
      if (const char* s = name.as<const char*>()) sig.nameContains.emplace_back(s);
    }

    // A signature with no criteria would match nothing (the matcher guards this),
    // so drop it here rather than carry dead weight.
    if (sig.hasCompanyId || !sig.serviceUuids16.empty() || !sig.nameContains.empty()) {
      parsed.push_back(std::move(sig));
    }
  }

  LOG_INF("BLEAUDIT", "Loaded %u signatures from %s", static_cast<unsigned>(parsed.size()), path);
  out = std::move(parsed);
  return !out.empty();
}

}  // namespace bleaudit
