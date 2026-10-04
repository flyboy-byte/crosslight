#pragma once

#include "activities/Activity.h"
#include "wifiaudit/ApScanner.h"

// The screen behind the "Wi-Fi Threats" utility: a passive watch for two things
// the radio can hear without transmitting -- possible evil-twin / rogue APs (one
// SSID coming from several BSSIDs, flagged harder when their encryption
// disagrees) and deauth-flood bursts (see ThreatDetect). Receive-only, so it
// ships in every build. Back stops the watch and leaves.
//
// Like the other radio tools, the simulator has no radio, so detection is
// verified on device; the sim only confirms the shell and "no radio" state.
class WifiThreatActivity final : public Activity {
 public:
  WifiThreatActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool preventAutoSleep() override { return true; }

 private:
  enum class State { NoRadio, Scanning };

  wifiaudit::ApScanner scanner;
  State state = State::Scanning;
  uint32_t startedMs = 0;
  uint32_t lastHopMs = 0;
  uint32_t lastPaintMs = 0;
};
