#pragma once

#include <string>
#include <unordered_map>

#include "activities/Activity.h"
#include "util/VendorDb.h"
#include "wifiaudit/ApScanner.h"

// The screen behind the "Wi-Fi Scan" utility: a passive, receive-only sweep of
// 2.4GHz access points (see ApScanner). Shows the channel it is on, frames
// seen, elapsed time, and a live list of APs sorted by signal, each with SSID,
// BSSID, channel, encryption, and RSSI. Back stops the scan and leaves.
//
// Like the camera scanner, the simulator has no radio, so the scan itself is
// verified on device; the sim only confirms the shell renders (including the
// "no radio" state).
class WifiScanActivity final : public Activity {
 public:
  WifiScanActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool preventAutoSleep() override { return true; }

 private:
  enum class State { NoRadio, Scanning };

  // Resolve a BSSID's OUI to a vendor name, cached so each OUI is a single
  // on-SD lookup. Empty vendor (db absent or unknown OUI) just shows no label.
  const std::string& vendorFor(const uint8_t bssid[6]);

  wifiaudit::ApScanner scanner;
  State state = State::Scanning;
  uint32_t startedMs = 0;
  uint32_t lastHopMs = 0;
  uint32_t lastPaintMs = 0;

  VendorDb ouiDb;
  std::unordered_map<uint32_t, std::string> ouiCache;
};
