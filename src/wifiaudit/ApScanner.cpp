#include "ApScanner.h"

#include <Logging.h>

#include <cstring>

#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#endif

namespace wifiaudit {

namespace {
// 1-13 covers the world on 2.4GHz; 14 is Japan-only, 5GHz is out of scope.
constexpr uint8_t MAX_CHANNEL = 13;

// One row per BSSID no matter how many beacons it sends. Cap the list so a busy
// RF environment can't grow it without bound.
constexpr size_t MAX_APS = 96;

#if defined(ARDUINO_ARCH_ESP32)
// Raw frame handed from the WiFi task to the UI task. Fixed size: the callback
// must not allocate. 512 bytes covers a beacon's header plus the security
// information elements (RSN/WPA usually appear in the first ~80 bytes); a longer
// beacon is truncated and parseBeacon() bounds-checks what it got.
struct RawFrame {
  uint8_t bytes[512];
  uint16_t len;
  int8_t rssi;
  uint8_t channel;
};

constexpr size_t QUEUE_DEPTH = 24;
QueueHandle_t g_queue = nullptr;

// Runs in the WiFi task. Minimal: management frames only, copy, push. No
// allocation, no blocking -- a full queue drops the frame (the next beacon
// arrives in ~100ms).
void IRAM_ATTR rxCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT || !g_queue) return;
  const auto* pkt = static_cast<wifi_promiscuous_pkt_t*>(buf);
  const uint16_t len = pkt->rx_ctrl.sig_len;
  if (len < 36) return;  // too short to hold the fixed body

  RawFrame frame;
  frame.len = len > sizeof(frame.bytes) ? sizeof(frame.bytes) : len;
  std::memcpy(frame.bytes, pkt->payload, frame.len);
  frame.rssi = pkt->rx_ctrl.rssi;
  frame.channel = pkt->rx_ctrl.channel;
  BaseType_t woken = pdFALSE;
  xQueueSendFromISR(g_queue, &frame, &woken);
  if (woken) portYIELD_FROM_ISR();
}
#endif  // ARDUINO_ARCH_ESP32
}  // namespace

bool ApScanner::begin() {
  found.clear();
  frames = 0;
  channel = 1;

#if defined(ARDUINO_ARCH_ESP32)
  if (!g_queue) g_queue = xQueueCreate(QUEUE_DEPTH, sizeof(RawFrame));
  if (!g_queue) {
    LOG_ERR("WIFIAUDIT", "Failed to create frame queue");
    return false;
  }
  // Bring the stack up without associating, then switch on promiscuous.
  WiFi.mode(WIFI_MODE_NULL);
  esp_wifi_set_promiscuous(false);
  wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
  esp_wifi_set_promiscuous_filter(&filter);
  if (esp_wifi_set_promiscuous(true) != ESP_OK) {
    LOG_ERR("WIFIAUDIT", "Could not enter promiscuous mode");
    return false;
  }
  esp_wifi_set_promiscuous_rx_cb(&rxCallback);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  active = true;
  LOG_INF("WIFIAUDIT", "AP scanner started");
  return true;
#else
  LOG_INF("WIFIAUDIT", "No radio on this platform; scanner inactive");
  return false;
#endif
}

void ApScanner::end() {
#if defined(ARDUINO_ARCH_ESP32)
  if (active) {
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    esp_wifi_set_promiscuous(false);
    WiFi.mode(WIFI_MODE_NULL);
  }
  if (g_queue) {
    xQueueReset(g_queue);
  }
#endif
  active = false;
}

void ApScanner::hopChannel() {
  if (!active) return;
  channel = channel >= MAX_CHANNEL ? 1 : channel + 1;
#if defined(ARDUINO_ARCH_ESP32)
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
#endif
}

void ApScanner::record(const AccessPoint& ap, const int8_t rssi) {
  const uint32_t now = millis();
  for (ApRecord& r : found) {
    if (std::memcmp(r.ap.bssid, ap.bssid, 6) == 0) {
      r.count++;
      r.lastSeenMs = now;
      if (rssi > r.rssi) r.rssi = rssi;  // keep the closest reading
      if (r.ap.ssid.empty() && !ap.ssid.empty()) r.ap.ssid = ap.ssid;
      if (ap.channel != 0) r.ap.channel = ap.channel;
      r.ap.encryption = ap.encryption;
      return;
    }
  }
  if (found.size() >= MAX_APS) return;
  ApRecord r;
  r.ap = ap;
  r.rssi = rssi;
  r.count = 1;
  r.firstSeenMs = now;
  r.lastSeenMs = now;
  found.push_back(std::move(r));
}

bool ApScanner::drain() {
  bool changed = false;
#if defined(ARDUINO_ARCH_ESP32)
  if (!g_queue) return false;
  RawFrame frame;
  while (xQueueReceive(g_queue, &frame, 0) == pdTRUE) {
    frames++;
    AccessPoint ap;
    if (!parseBeacon(frame.bytes, frame.len, ap)) continue;
    if (ap.channel == 0) ap.channel = frame.channel;  // fall back to the heard-on channel
    record(ap, frame.rssi);
    changed = true;
  }
#endif
  return changed;
}

}  // namespace wifiaudit
