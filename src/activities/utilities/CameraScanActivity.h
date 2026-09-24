#pragma once

#include "activities/Activity.h"
#include "flock/FlockScanner.h"

// The screen behind the "Camera Scan" utility: a passive, receive-only sweep
// for surveillance-device Wi-Fi signatures (see FlockScanner). Shows the
// channel it is on, how many frames it has seen, and a live list of matches
// with signal strength. Back stops the scan and leaves.
//
// This is the one utility the simulator cannot exercise -- it has no radio --
// so the scan itself is verified on device with a serial log; the sim only
// confirms the shell renders (including the "no radio" and "no signatures"
// states).
class CameraScanActivity final : public Activity {
 public:
  CameraScanActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool preventAutoSleep() override { return true; }

 private:
  enum class State { NoSignatures, NoRadio, Scanning };

  flock::FlockScanner scanner;
  State state = State::Scanning;
  uint32_t startedMs = 0;
  uint32_t lastHopMs = 0;
  uint32_t lastPaintMs = 0;
};
