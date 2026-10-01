#pragma once

#include <cstdint>

// Active BLE advertiser for the security toolkit -- the transmit counterpart to
// the passive BleScanner. It BROADCASTS, so unlike everything else in bleaudit
// it is gated the same way wifiaudit's active tools are: it does nothing on a
// build without CROSSLIGHT_ENABLE_ACTIVE_AUDIT (AttackTx::buildSupportsActiveAudit()),
// and the UI confirms the per-boot ActiveAuditGate before starting it.
//
// Profiles are deliberately GENERIC test transmitters -- a named device and an
// example iBeacon -- for exercising your own BLE scanner / detector (it pairs
// with the passive BleScanner the way evil-twin pairs with the Wi-Fi threat
// detector). They do not impersonate any specific real product, person, or
// safety-relevant tracker.
//
// No radio on the host/simulator, so begin() returns false there and the UI
// shows that rather than a dead advertiser.
namespace bleaudit {

class BleSpoofer {
 public:
  static int profileCount();
  static const char* profileName(int index);

  // Brings the BLE controller up (shared NimBLE init). False if there is no
  // radio. Does not start advertising yet.
  bool begin();
  // Stops advertising and releases the advertiser. Safe if begin() failed.
  void end();
  bool running() const { return advertising; }

  // Start (or restart) advertising the given profile. False on a build/platform
  // that cannot transmit, so the UI can stay truthful about what happened.
  bool advertise(int profileIndex);
  // Stop advertising but keep the controller up.
  void stop();

 private:
  bool initialized = false;
  bool advertising = false;
};

}  // namespace bleaudit
