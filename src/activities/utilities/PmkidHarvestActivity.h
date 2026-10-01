#pragma once

#include <string>

#include "activities/Activity.h"
#include "wifiaudit/HarvestScanner.h"

// The screen behind the "PMKID Harvest" utility: a passive, receive-only sweep
// that catches RSN PMKIDs from EAPOL message-1 frames and appends hashcat 22000
// lines to /wifiaudit/pmkidNNN.22000 on the SD card (see HarvestScanner).
// Capturing is passive; cracking is a separate offline step. Back stops it.
//
// The simulator has no radio, so this is verified on device; the sim only
// confirms the shell and the unavailable state.
class PmkidHarvestActivity final : public Activity {
 public:
  PmkidHarvestActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool preventAutoSleep() override { return true; }

 private:
  enum class State { Unavailable, Harvesting };

  wifiaudit::HarvestScanner scanner;
  State state = State::Harvesting;
  std::string path;
  uint32_t startedMs = 0;
  uint32_t lastHopMs = 0;
  uint32_t lastPaintMs = 0;
};
