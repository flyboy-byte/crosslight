#include "CaptureScanner.h"

#include <Logging.h>

#include <cstring>

#include "Pcap.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#endif

namespace wifiaudit {

namespace {
constexpr uint8_t MAX_CHANNEL = 13;

#if defined(ARDUINO_ARCH_ESP32)
// One queued frame, captured verbatim up to the pcap snap length. Larger than
// the mgmt-only scanners because data frames are captured too.
struct RawFrame {
  uint8_t bytes[PCAP_SNAPLEN];
  uint16_t len;
  uint16_t origLen;
  uint32_t tsUs;  // rx timestamp, microseconds since boot
};

// Depth balances SD write latency against DRAM: a shallow queue just drops
// frames under a burst, which pcap capture on constrained hardware always does.
constexpr size_t QUEUE_DEPTH = 8;
QueueHandle_t g_queue = nullptr;

void IRAM_ATTR rxCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  (void)type;
  if (!g_queue) return;
  const auto* pkt = static_cast<wifi_promiscuous_pkt_t*>(buf);
  const uint16_t len = pkt->rx_ctrl.sig_len;
  if (len == 0) return;

  RawFrame frame;
  frame.origLen = len;
  frame.len = len > sizeof(frame.bytes) ? sizeof(frame.bytes) : len;
  std::memcpy(frame.bytes, pkt->payload, frame.len);
  frame.tsUs = pkt->rx_ctrl.timestamp;
  BaseType_t woken = pdFALSE;
  xQueueSendFromISR(g_queue, &frame, &woken);
  if (woken) portYIELD_FROM_ISR();
}
#endif  // ARDUINO_ARCH_ESP32
}  // namespace

bool CaptureScanner::begin(const char* pcapPath) {
  seen = 0;
  channel = 1;
  if (!sink.open(pcapPath)) return false;

#if defined(ARDUINO_ARCH_ESP32)
  if (!g_queue) g_queue = xQueueCreate(QUEUE_DEPTH, sizeof(RawFrame));
  if (!g_queue) {
    LOG_ERR("WIFIAUDIT", "Failed to create capture queue");
    sink.close();
    return false;
  }
  WiFi.mode(WIFI_MODE_NULL);
  esp_wifi_set_promiscuous(false);
  wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_ALL};
  esp_wifi_set_promiscuous_filter(&filter);
  if (esp_wifi_set_promiscuous(true) != ESP_OK) {
    LOG_ERR("WIFIAUDIT", "Could not enter promiscuous mode");
    sink.close();
    return false;
  }
  esp_wifi_set_promiscuous_rx_cb(&rxCallback);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  active = true;
  return true;
#else
  LOG_INF("WIFIAUDIT", "No radio on this platform; capture inactive");
  sink.close();
  return false;
#endif
}

void CaptureScanner::end() {
#if defined(ARDUINO_ARCH_ESP32)
  if (active) {
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    esp_wifi_set_promiscuous(false);
    WiFi.mode(WIFI_MODE_NULL);
  }
  if (g_queue) xQueueReset(g_queue);
#endif
  active = false;
  sink.close();
}

void CaptureScanner::hopChannel() {
  if (!active) return;
  channel = channel >= MAX_CHANNEL ? 1 : channel + 1;
#if defined(ARDUINO_ARCH_ESP32)
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
#endif
}

bool CaptureScanner::drain() {
  bool wrote = false;
#if defined(ARDUINO_ARCH_ESP32)
  if (!g_queue) return false;
  RawFrame frame;
  while (xQueueReceive(g_queue, &frame, 0) == pdTRUE) {
    seen++;
    const uint32_t tsSec = frame.tsUs / 1000000u;
    const uint32_t tsUsec = frame.tsUs % 1000000u;
    if (sink.appendFrame(frame.bytes, frame.len, frame.origLen, tsSec, tsUsec)) wrote = true;
  }
#endif
  return wrote;
}

}  // namespace wifiaudit
