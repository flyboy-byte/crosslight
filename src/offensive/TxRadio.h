#pragma once

#include <cstdint>

// Minimal radio bring-up for raw-frame transmit tools (beacon flood, and later
// targeted deauth): brings the STA interface up without associating, so
// AttackTx's esp_wifi_80211_tx(WIFI_IF_STA, ...) has a live interface to send
// on, and lets the caller pick which channel to transmit on.
//
// Mirrors ApScanner's begin()/end()/channel shape but for TX instead of
// promiscuous RX. The two radio modes are mutually exclusive on one chip, so a
// TX tool and a scan tool cannot run at once -- same constraint ApScanner
// already documents for RX vs normal Wi-Fi use.
//
// This only brings the STA interface up; it never calls WiFi.begin() or
// supplies credentials, so it never attempts to associate with anything.
// AttackTx itself still double-gates every actual transmit (compile flag +
// ActiveAuditGate) independently of this class being active.
namespace wifiaudit {

class TxRadio {
 public:
  // False if there is no radio (host/simulator build).
  bool begin();
  void end();
  bool running() const { return active; }

  void setChannel(uint8_t channel);
  uint8_t currentChannel() const { return channel; }

 private:
  bool active = false;
  uint8_t channel = 1;
};

}  // namespace wifiaudit
