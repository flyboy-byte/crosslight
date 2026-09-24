#include "DeviceLock.h"

#include <mbedtls/sha256.h>

#include <cstdio>

namespace {
// Fixed application salt. It does not slow down an attacker who has the card
// (see the header), but it does stop the stored value from matching a plain
// SHA-256 rainbow table of common passwords.
constexpr char SALT[] = "CrossLight-device-lock-v1:";
}  // namespace

std::string DeviceLock::hashOf(const std::string& passphrase) {
  const std::string salted = std::string(SALT) + passphrase;
  uint8_t digest[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, /*is224=*/0);
  mbedtls_sha256_update(&ctx, reinterpret_cast<const uint8_t*>(salted.data()), salted.size());
  mbedtls_sha256_finish(&ctx, digest);
  mbedtls_sha256_free(&ctx);

  char hex[65];
  for (int i = 0; i < 32; ++i) snprintf(hex + i * 2, 3, "%02x", digest[i]);
  return hex;
}

void DeviceLock::toJson(JsonDocument& doc) const {
  doc["hash"] = hash;
  doc["enabled"] = enabled;
  doc["onWake"] = onWake;
}

bool DeviceLock::fromJson(const JsonVariantConst doc) {
  hash = doc["hash"] | "";
  enabled = doc["enabled"] | false;
  onWake = doc["onWake"] | true;
  return true;
}

bool DeviceLock::setPassphrase(const std::string& passphrase) {
  if (passphrase.size() < MIN_LENGTH || passphrase.size() > MAX_LENGTH) return false;
  hash = hashOf(passphrase);
  enabled = true;
  saveToFile();
  return true;
}

void DeviceLock::clear() {
  hash.clear();
  enabled = false;
  saveToFile();
}

bool DeviceLock::verify(const std::string& passphrase) const {
  if (hash.empty()) return true;
  return hashOf(passphrase) == hash;
}

void DeviceLock::setEnabled(const bool on) {
  // Switching the lock on without a passphrase would lock the device with
  // nothing that opens it.
  enabled = on && !hash.empty();
  saveToFile();
}

void DeviceLock::setLockOnWake(const bool on) {
  onWake = on;
  saveToFile();
}
