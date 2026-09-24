#include "FlockScanner.h"

#include <Logging.h>

#include <cstring>
#include <utility>

#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#endif

namespace flock {

namespace {
// The highest 2.4GHz channel to sweep. 1-13 covers the world; 14 is Japan-only
// and 5GHz is out of scope (management-frame sniffing here is 2.4GHz).
constexpr uint8_t MAX_CHANNEL = 13;

// A device is one row no matter how many frames it sends. Cap the list so a
// noisy RF environment can't grow it without bound.
constexpr size_t MAX_DETECTIONS = 64;

#if defined(ARDUINO_ARCH_ESP32)
// Raw frame handed from the WiFi task to the UI task. Fixed size: the callback
// must not allocate. 256 bytes covers any beacon/probe management frame's
// header plus the early tagged parameters we read.
struct RawFrame {
  uint8_t bytes[256];
  uint16_t len;
  int8_t rssi;
  uint8_t channel;
};

constexpr size_t QUEUE_DEPTH = 24;
QueueHandle_t g_queue = nullptr;

// Runs in the WiFi task. Kept minimal: filter to management frames, copy, push.
// No allocation, no blocking -- a full queue drops the frame, which is fine
// (the next beacon comes in ~100ms).
void IRAM_ATTR rxCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT || !g_queue) return;
  const auto* pkt = static_cast<wifi_promiscuous_pkt_t*>(buf);
  const uint16_t len = pkt->rx_ctrl.sig_len;
  if (len < 24) return;

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

bool FlockScanner::begin(std::vector<Signature> sigs) {
  signatures = std::move(sigs);
  found.clear();
  frames = 0;
  channel = 1;

#if defined(ARDUINO_ARCH_ESP32)
  if (!g_queue) g_queue = xQueueCreate(QUEUE_DEPTH, sizeof(RawFrame));
  if (!g_queue) {
    LOG_ERR("FLOCK", "Failed to create frame queue");
    return false;
  }
  // Bring the stack up without associating, then switch on promiscuous.
  WiFi.mode(WIFI_MODE_NULL);
  esp_wifi_set_promiscuous(false);
  wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
  esp_wifi_set_promiscuous_filter(&filter);
  if (esp_wifi_set_promiscuous(true) != ESP_OK) {
    LOG_ERR("FLOCK", "Could not enter promiscuous mode");
    return false;
  }
  esp_wifi_set_promiscuous_rx_cb(&rxCallback);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  active = true;
  LOG_INF("FLOCK", "Scanner started (%u signatures)", static_cast<unsigned>(signatures.size()));
  return true;
#else
  // No radio on the host: report unavailable so the UI says so plainly.
  LOG_INF("FLOCK", "No radio on this platform; scanner inactive");
  return false;
#endif
}

void FlockScanner::end() {
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

void FlockScanner::hopChannel() {
  if (!active) return;
  channel = channel >= MAX_CHANNEL ? 1 : channel + 1;
#if defined(ARDUINO_ARCH_ESP32)
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
#endif
}

void FlockScanner::record(const Observation& obs, const int8_t rssi, const int matchedIndex) {
  const std::string& name = signatures[matchedIndex].name;
  const uint32_t now = millis();
  for (Detection& d : found) {
    if (std::memcmp(d.mac, obs.mac, 6) == 0) {
      d.count++;
      d.lastSeenMs = now;
      d.channel = channel;
      if (rssi > d.rssi) d.rssi = rssi;  // keep the closest reading
      if (d.ssid.empty() && !obs.ssid.empty()) d.ssid = obs.ssid;
      return;
    }
  }
  if (found.size() >= MAX_DETECTIONS) return;
  Detection d;
  d.name = name;
  d.ssid = obs.ssid;
  std::memcpy(d.mac, obs.mac, 6);
  d.rssi = rssi;
  d.channel = channel;
  d.count = 1;
  d.firstSeenMs = now;
  d.lastSeenMs = now;
  found.push_back(std::move(d));
  LOG_INF("FLOCK", "Match: %s (%02X:%02X:%02X:%02X:%02X:%02X) rssi=%d ch=%d", name.c_str(), obs.mac[0], obs.mac[1],
          obs.mac[2], obs.mac[3], obs.mac[4], obs.mac[5], rssi, channel);
}

bool FlockScanner::drain() {
  bool changed = false;
#if defined(ARDUINO_ARCH_ESP32)
  if (!g_queue) return false;
  RawFrame frame;
  while (xQueueReceive(g_queue, &frame, 0) == pdTRUE) {
    frames++;
    Observation obs;
    if (!parseManagementFrame(frame.bytes, frame.len, obs)) continue;
    const int idx = match(signatures, obs);
    if (idx < 0) continue;
    record(obs, frame.rssi, idx);
    changed = true;
  }
#endif
  return changed;
}

}  // namespace flock
