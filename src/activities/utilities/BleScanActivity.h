#pragma once

#include "activities/Activity.h"
#include "bleaudit/BleScanner.h"

// The screen behind the "BLE Scan" utility: a passive, receive-only sweep for BLE
// device advertisements matching loaded signatures (see BleScanner). Shows how
// many adverts it has seen and a live list of matches with signal strength. Back
// stops the scan and leaves.
//
// Like Camera Scan, the simulator cannot exercise the scan itself -- it has no
// radio -- so the scan is verified on device with a serial log; the sim only
// confirms the shell renders (including the "no radio" and "no signatures"
// states).
class BleScanActivity final : public Activity {
 public:
  BleScanActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool preventAutoSleep() override { return true; }

 private:
  enum class State { NoSignatures, NoRadio, Scanning };

  bleaudit::BleScanner scanner;
  State state = State::Scanning;
  uint32_t startedMs = 0;
  uint32_t lastPaintMs = 0;
};
