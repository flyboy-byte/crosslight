#include "HarvestScanner.h"

#include <Logging.h>

#include <cstring>

#include "Eapol.h"
#include "wifiaudit/WifiFrame.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#endif

namespace wifiaudit {

namespace {
constexpr uint8_t MAX_CHANNEL = 13;
constexpr size_t MAX_SSIDS = 128;

#if defined(ARDUINO_ARCH_ESP32)
struct RawFrame {
  uint8_t bytes[512];
  uint16_t len;
  uint8_t channel;
};

constexpr size_t QUEUE_DEPTH = 16;
QueueHandle_t g_queue = nullptr;

void IRAM_ATTR rxCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  (void)type;
  if (!g_queue) return;
  const auto* pkt = static_cast<wifi_promiscuous_pkt_t*>(buf);
  const uint16_t len = pkt->rx_ctrl.sig_len;
  if (len < 24) return;

  RawFrame frame;
  frame.len = len > sizeof(frame.bytes) ? sizeof(frame.bytes) : len;
  std::memcpy(frame.bytes, pkt->payload, frame.len);
  frame.channel = pkt->rx_ctrl.channel;
  BaseType_t woken = pdFALSE;
  xQueueSendFromISR(g_queue, &frame, &woken);
  if (woken) portYIELD_FROM_ISR();
}
#endif  // ARDUINO_ARCH_ESP32
}  // namespace

void HarvestScanner::noteBeacon(const uint8_t bssid[6], const std::string& ssid) {
  if (ssid.empty()) return;
  for (SsidEntry& e : ssids) {
    if (std::memcmp(e.bssid, bssid, 6) == 0) {
      if (e.ssid.empty()) e.ssid = ssid;
      return;
    }
  }
  if (ssids.size() >= MAX_SSIDS) return;
  SsidEntry e;
  std::memcpy(e.bssid, bssid, 6);
  e.ssid = ssid;
  ssids.push_back(std::move(e));
}

const std::string* HarvestScanner::lookupSsid(const uint8_t bssid[6]) const {
  for (const SsidEntry& e : ssids) {
    if (std::memcmp(e.bssid, bssid, 6) == 0) return &e.ssid;
  }
  return nullptr;
}

bool HarvestScanner::pmkidIsNew(const uint8_t pmkid[16]) {
  std::array<uint8_t, 16> key{};
  std::memcpy(key.data(), pmkid, 16);
  for (const auto& p : seenPmkids) {
    if (p == key) return false;
  }
  seenPmkids.push_back(key);
  return true;
}

bool HarvestScanner::begin(const char* outPath) {
  seen = 0;
  channel = 1;
  ssids.clear();
  seenPmkids.clear();
  if (!Storage.openFileForWrite("WIFIAUDIT", outPath, out)) {
    LOG_ERR("WIFIAUDIT", "Could not open %s for PMKID output", outPath);
    return false;
  }

#if defined(ARDUINO_ARCH_ESP32)
  if (!g_queue) g_queue = xQueueCreate(QUEUE_DEPTH, sizeof(RawFrame));
  if (!g_queue) {
    out.close();
    return false;
  }
  // STA, not NULL: WiFi.mode(NULL) no-ops when Wi-Fi is already off, leaving
  // esp_wifi uninitialized so promiscuous fails. STA inits+starts the driver
  // without associating (no WiFi.begin()). end() returns to NULL to tear down.
  WiFi.mode(WIFI_MODE_STA);
  esp_wifi_set_promiscuous(false);
  wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA};
  esp_wifi_set_promiscuous_filter(&filter);
  if (esp_wifi_set_promiscuous(true) != ESP_OK) {
    out.close();
    return false;
  }
  esp_wifi_set_promiscuous_rx_cb(&rxCallback);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  active = true;
  return true;
#else
  LOG_INF("WIFIAUDIT", "No radio on this platform; harvester inactive");
  out.close();
  return false;
#endif
}

void HarvestScanner::end() {
#if defined(ARDUINO_ARCH_ESP32)
  if (active) {
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    esp_wifi_set_promiscuous(false);
    WiFi.mode(WIFI_MODE_NULL);
  }
  if (g_queue) xQueueReset(g_queue);
#endif
  active = false;
  out.close();
}

void HarvestScanner::hopChannel() {
  if (!active) return;
  channel = channel >= MAX_CHANNEL ? 1 : channel + 1;
#if defined(ARDUINO_ARCH_ESP32)
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
#endif
}

bool HarvestScanner::drain() {
  bool caught = false;
#if defined(ARDUINO_ARCH_ESP32)
  if (!g_queue) return false;
  RawFrame frame;
  while (xQueueReceive(g_queue, &frame, 0) == pdTRUE) {
    seen++;
    const MgmtKind kind = managementKind(frame.bytes, frame.len);
    if (kind == MgmtKind::Beacon || kind == MgmtKind::ProbeResponse) {
      AccessPoint ap;
      if (parseBeacon(frame.bytes, frame.len, ap)) noteBeacon(ap.bssid, ap.ssid);
      continue;
    }

    EapolLocation loc;
    if (!findEapolInDataFrame(frame.bytes, frame.len, loc)) continue;
    EapolKey key;
    if (!parseEapolKey(loc.eapol, loc.eapolLen, key)) continue;
    if (classifyEapol(key.keyInfo) != EapolMessage::M1) continue;
    uint8_t pmkid[16];
    if (!extractPmkid(key, pmkid)) continue;
    if (!pmkidIsNew(pmkid)) continue;

    const std::string* ssid = lookupSsid(loc.macAp);
    const std::string line =
        formatPmkid22000(pmkid, loc.macAp, loc.macSta, ssid ? reinterpret_cast<const uint8_t*>(ssid->data()) : nullptr,
                         ssid ? ssid->size() : 0);
    out.write(line.data(), line.size());
    out.write(static_cast<uint8_t>('\n'));
    caught = true;
    LOG_INF("WIFIAUDIT", "PMKID caught (%u total)", static_cast<unsigned>(seenPmkids.size()));
  }
#endif
  return caught;
}

}  // namespace wifiaudit
