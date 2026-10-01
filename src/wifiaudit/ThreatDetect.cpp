#include "ThreatDetect.h"

#include <array>
#include <cstring>

namespace wifiaudit {

namespace {
// Working tally for one SSID while grouping sightings.
struct SsidGroup {
  std::string ssid;
  std::vector<std::array<uint8_t, 6>> bssids;  // distinct, in first-seen order
  Encryption firstEnc = Encryption::Open;
  bool encMismatch = false;
};
}  // namespace

std::vector<EvilTwinAlert> findEvilTwins(const std::vector<ApRecord>& aps) {
  std::vector<SsidGroup> groups;
  for (const ApRecord& r : aps) {
    if (r.ap.ssid.empty()) continue;  // hidden networks have no name to clone
    SsidGroup* group = nullptr;
    for (SsidGroup& g : groups) {
      if (g.ssid == r.ap.ssid) {
        group = &g;
        break;
      }
    }
    if (!group) {
      groups.push_back(SsidGroup{r.ap.ssid, {}, r.ap.encryption, false});
      group = &groups.back();
    }
    std::array<uint8_t, 6> bssid{};
    std::memcpy(bssid.data(), r.ap.bssid, 6);
    bool known = false;
    for (const auto& b : group->bssids) {
      if (b == bssid) {
        known = true;
        break;
      }
    }
    if (!known) group->bssids.push_back(bssid);
    if (r.ap.encryption != group->firstEnc) group->encMismatch = true;
  }

  std::vector<EvilTwinAlert> alerts;
  for (const SsidGroup& g : groups) {
    if (g.bssids.size() < 2) continue;
    alerts.push_back(EvilTwinAlert{g.ssid, static_cast<uint32_t>(g.bssids.size()), g.encMismatch});
  }
  return alerts;
}

uint32_t deauthCountInWindow(const std::vector<uint32_t>& timesMs, const uint32_t nowMs, const uint32_t windowMs) {
  const uint32_t cutoff = nowMs >= windowMs ? nowMs - windowMs : 0;
  uint32_t count = 0;
  for (const uint32_t t : timesMs) {
    if (t >= cutoff && t <= nowMs) count++;
  }
  return count;
}

bool deauthFloodActive(const std::vector<uint32_t>& timesMs, const uint32_t nowMs, const uint32_t windowMs,
                       const uint32_t threshold) {
  return deauthCountInWindow(timesMs, nowMs, windowMs) >= threshold;
}

}  // namespace wifiaudit
