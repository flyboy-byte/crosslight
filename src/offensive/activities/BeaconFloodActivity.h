#pragma once

#include "activities/Activity.h"
#include "components/themes/BaseTheme.h"
#include "offensive/TxRadio.h"

// The screen behind the "Beacon Flood" utility: transmits a rotating set of
// fake-AP beacon frames (see wifiaudit::buildBeacon), each from its own
// synthesized BSSID, so a Wi-Fi scan on nearby hardware fills with bogus
// networks. This is an ACTIVE tool -- it puts energy on the air -- so it is
// double-gated the same way every wifiaudit transmit path is: the
// CROSSLIGHT_ENABLE_ACTIVE_AUDIT compile flag (AttackTx::buildSupportsActiveAudit())
// and the per-boot ActiveAuditGate, which this screen confirms automatically
// on first transmit (no blocking dialog -- Logan's call, "i dont need a
// babysitter" -- but the on-screen warning line stays up the whole time the
// screen is open, not just once).
//
// No radio on the host/simulator build, so this is simulator-verified for the
// shell states only (no radio / disabled / idle); the actual transmit is
// verified on device.
class BeaconFloodActivity final : public Activity {
 public:
  BeaconFloodActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool preventAutoSleep() override { return true; }

 private:
  enum class State { NoRadio, Disabled, Idle, Running };

  Rect channelRowRect() const;
  void cycleChannel(int delta);
  void sendOneFrame();

  wifiaudit::TxRadio txRadio;
  State state = State::NoRadio;
  int ssidIndex = 0;
  uint32_t framesSent = 0;
  uint32_t startedMs = 0;
  uint32_t lastTxMs = 0;
  uint32_t lastPaintMs = 0;
};
