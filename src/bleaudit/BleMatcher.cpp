#include <algorithm>
#include <cctype>

#include "BleSignature.h"

namespace bleaudit {

namespace {
// ASCII case-insensitive substring test. Advertised names are bytes, not
// guaranteed UTF-8, so this folds only ASCII -- which is all the vendor strings
// we match against are -- and treats everything else literally.
bool containsInsensitive(const std::string& haystack, const std::string& needle) {
  if (needle.empty()) return false;
  const auto lower = [](const unsigned char c) { return static_cast<char>(std::tolower(c)); };
  const auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                              [&](const char a, const char b) { return lower(a) == lower(b); });
  return it != haystack.end();
}
}  // namespace

int matchBle(const std::vector<BleSignature>& signatures, const BleAdvertisement& adv) {
  for (size_t i = 0; i < signatures.size(); ++i) {
    const BleSignature& sig = signatures[i];
    // A signature with no criteria matches nothing -- guards against a blank
    // entry flagging every device on the air.
    if (!sig.hasCompanyId && sig.serviceUuids16.empty() && sig.nameContains.empty()) continue;

    if (sig.hasCompanyId && adv.hasCompanyId && sig.companyId == adv.companyId) return static_cast<int>(i);

    for (const uint16_t uuid : sig.serviceUuids16) {
      if (std::find(adv.serviceUuids16.begin(), adv.serviceUuids16.end(), uuid) != adv.serviceUuids16.end()) {
        return static_cast<int>(i);
      }
    }

    for (const std::string& sub : sig.nameContains) {
      if (containsInsensitive(adv.name, sub)) return static_cast<int>(i);
    }
  }
  return -1;
}

}  // namespace bleaudit
