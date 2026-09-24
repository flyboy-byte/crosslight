#include "FlockSignature.h"

#include <algorithm>
#include <cctype>

namespace flock {

uint32_t ouiOf(const uint8_t mac[6]) {
  return (static_cast<uint32_t>(mac[0]) << 16) | (static_cast<uint32_t>(mac[1]) << 8) | mac[2];
}

namespace {
// ASCII case-insensitive substring test. SSIDs are bytes, not guaranteed
// UTF-8, so this only folds ASCII -- which is all the vendor strings we match
// against are -- and treats everything else literally.
bool containsInsensitive(const std::string& haystack, const std::string& needle) {
  if (needle.empty()) return false;
  const auto lower = [](const unsigned char c) { return static_cast<char>(std::tolower(c)); };
  const auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                              [&](const char a, const char b) { return lower(a) == lower(b); });
  return it != haystack.end();
}
}  // namespace

int match(const std::vector<Signature>& signatures, const Observation& obs) {
  const uint32_t oui = ouiOf(obs.mac);
  for (size_t i = 0; i < signatures.size(); ++i) {
    const Signature& sig = signatures[i];
    // A signature with no criteria matches nothing -- guards against a blank
    // entry flagging every device on the air.
    if (sig.ouis.empty() && sig.ssidContains.empty()) continue;

    if (std::find(sig.ouis.begin(), sig.ouis.end(), oui) != sig.ouis.end()) return static_cast<int>(i);

    for (const std::string& sub : sig.ssidContains) {
      if (containsInsensitive(obs.ssid, sub)) return static_cast<int>(i);
    }
  }
  return -1;
}

}  // namespace flock
