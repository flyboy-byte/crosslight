#pragma once

#include <cstdint>

#include "activities/Activity.h"
#include "bleaudit/BleSpoofer.h"
#include "components/themes/BaseTheme.h"

// The screen behind the "BLE Spoof" utility: broadcasts a chosen generic BLE
// profile (a named test device, or an example iBeacon) so you can exercise your
// own BLE scanner / detector -- the active counterpart to the passive BLE scan,
// the way beacon-flood/evil-twin pair with their detectors. Active tool, so it
// is gated like every wifiaudit/bleaudit transmit path: buildSupportsActiveAudit()
// plus the per-boot ActiveAuditGate (auto-confirmed on start, no modal, with the
// on-screen "your own gear only" warning kept up the whole time).
//
// No radio on host/simulator, so this is simulator-verified for the shell states
// only; the actual broadcast is device-verified.
class BleSpoofActivity final : public Activity {
 public:
  BleSpoofActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool preventAutoSleep() override { return true; }

 private:
  enum class State { NoRadio, Disabled, Idle, Running };

  Rect profileRowRect() const;
  void cycleProfile(int delta);

  bleaudit::BleSpoofer spoofer;
  State state = State::NoRadio;
  int profile = 0;
  uint32_t startedMs = 0;
  uint32_t lastPaintMs = 0;
};
