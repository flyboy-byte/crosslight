#include "TxRadio.h"

#include <Logging.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <esp_wifi.h>
#endif

namespace wifiaudit {

bool TxRadio::begin() {
  channel = 1;
#if defined(ARDUINO_ARCH_ESP32)
  // STA mode brings the radio up without starting an AP or attempting to
  // associate (no SSID/credentials are ever supplied) -- just a live
  // interface for esp_wifi_80211_tx() to send raw frames through.
  WiFi.mode(WIFI_MODE_STA);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  active = true;
  LOG_INF("WIFIAUDIT", "TX radio started");
  return true;
#else
  LOG_INF("WIFIAUDIT", "No radio on this platform; TX radio inactive");
  return false;
#endif
}

void TxRadio::end() {
#if defined(ARDUINO_ARCH_ESP32)
  if (active) WiFi.mode(WIFI_MODE_NULL);
#endif
  active = false;
}

void TxRadio::setChannel(const uint8_t newChannel) {
  channel = newChannel;
#if defined(ARDUINO_ARCH_ESP32)
  if (active) esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
#endif
}

}  // namespace wifiaudit
