#include "AttackTx.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CROSSLIGHT_ENABLE_ACTIVE_AUDIT)
#include <esp_wifi.h>

#include "ActiveAuditGate.h"
#endif

namespace wifiaudit {

// ===========================================================================
// BUILD NOTE for whoever compiles the active-audit firmware -- READ BEFORE USE
// ===========================================================================
// esp_wifi_80211_tx() works out of the box for BEACON and PROBE frames, so the
// rogue-AP / beacon tools transmit on a stock Espressif SDK with nothing extra.
//
// It does NOT work for DEAUTH or DISASSOC: the closed-source libnet80211 blob
// runs every outgoing raw frame through ieee80211_raw_frame_sanity_check(),
// which REJECTS the deauth/disassoc subtypes -- the call returns an error and
// nothing goes on the air.
//
// To make deauth/disassoc actually transmit you must bypass that check at LINK
// time by adding, to the active-audit build env only:
//
//     build_flags = ... -Wl,-wrap=ieee80211_raw_frame_sanity_check
//
// and supplying __wrap_ieee80211_raw_frame_sanity_check() that returns ESP_OK.
// (A patched libnet80211 achieves the same thing.) This module deliberately does
// NOT define that wrap: the linker bypass is a build-level decision for the
// person assembling the firmware, documented here, not something this code does
// on its own. Without it, buildDeauth()/buildDisassoc() frames are built
// correctly but the driver silently drops them.
// ===========================================================================

#if defined(ARDUINO_ARCH_ESP32) && defined(CROSSLIGHT_ENABLE_ACTIVE_AUDIT)

bool transmitFrame(const uint8_t* buf, const size_t len) {
  if (!buf || len == 0) return false;
  // Defense in depth: even with both compile guards present, refuse until the
  // operator has cleared the one-time warning this boot. This check is
  // independent of the UI flow, so a missed UI gate still cannot transmit.
  if (!ActiveAuditGate::confirmed()) return false;
  // WIFI_IF_STA: transmit on the station interface. The final `false` tells the
  // driver not to insert/maintain a sequence number for us.
  return esp_wifi_80211_tx(WIFI_IF_STA, buf, len, false) == ESP_OK;
}

#else

// Host, simulator, or any release build: no radio and/or active audit disabled.
// Compiles everywhere, transmits nowhere.
bool transmitFrame(const uint8_t*, size_t) { return false; }

#endif

}  // namespace wifiaudit
