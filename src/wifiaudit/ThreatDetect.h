#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ApScanner.h"

// Passive Wi-Fi threat heuristics that run on top of what the radio already
// hears -- no transmitting, so this ships in every build. Two awareness signals:
//
//   * Evil-twin / rogue AP: one SSID advertised by more than one BSSID. That is
//     normal for mesh/extender setups (which usually share a vendor OUI), but it
//     is also exactly what a cloned "evil twin" looks like, so it is surfaced
//     for the operator to judge -- more suspicious when the copies disagree on
//     encryption (e.g. your WPA2 network suddenly also appears Open).
//   * Deauth flood: a burst of deauthentication/disassociation frames, the
//     signature of a deauth attack knocking clients off a network.
//
// Everything here is pure and host-tested; the capture that feeds it lives in
// ApScanner, and the thresholds are the caller's to choose.
namespace wifiaudit {

struct EvilTwinAlert {
  std::string ssid;
  uint32_t bssidCount = 0;          // distinct BSSIDs advertising this SSID
  bool encryptionMismatch = false;  // the copies disagree on encryption
};

// SSIDs seen from two or more distinct BSSIDs, strongest aggregate first-ish
// (input order is preserved by SSID of first sighting). Hidden (empty) SSIDs are
// ignored -- they share no name to clone.
std::vector<EvilTwinAlert> findEvilTwins(const std::vector<ApRecord>& aps);

// How many of `timesMs` fall within the last `windowMs` before `nowMs`.
// Timestamps at or after (nowMs - windowMs) and not in the future count.
uint32_t deauthCountInWindow(const std::vector<uint32_t>& timesMs, uint32_t nowMs, uint32_t windowMs);

// True when the deauth/disassoc count in the window reaches `threshold` --
// i.e. a plausible deauth flood rather than the occasional legitimate frame.
bool deauthFloodActive(const std::vector<uint32_t>& timesMs, uint32_t nowMs, uint32_t windowMs, uint32_t threshold);

}  // namespace wifiaudit
