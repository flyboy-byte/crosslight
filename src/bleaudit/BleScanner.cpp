#include "BleScanner.h"

#include <Logging.h>

#include <cstdio>
#include <cstring>
#include <utility>

#if defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#endif

namespace bleaudit {

namespace {
// A device is one row no matter how many advertisements it sends. Cap the list so
// a crowded RF environment can't grow it without bound.
constexpr size_t MAX_DETECTIONS = 64;

// Build the compact "what was seen" line from a parsed advert: local name, then
// company ID, Apple message type, and the first service UUID when present. Runs on
// the UI task (from record()), so a little std::string work is fine.
std::string describe(const BleAdvertisement& adv) {
  std::string s;
  if (!adv.name.empty()) s += adv.name;
  char tmp[24];
  if (adv.hasCompanyId) {
    std::snprintf(tmp, sizeof(tmp), "%sco 0x%04X", s.empty() ? "" : " ", adv.companyId);
    s += tmp;
  }
  if (adv.hasAppleType) {
    std::snprintf(tmp, sizeof(tmp), " t0x%02X", adv.appleType);
    s += tmp;
  }
  if (!adv.serviceUuids16.empty()) {
    std::snprintf(tmp, sizeof(tmp), "%ssvc 0x%04X", s.empty() ? "" : " ", adv.serviceUuids16[0]);
    s += tmp;
  }
  return s;
}

#if defined(ARDUINO_ARCH_ESP32)
// A legacy advertising payload is at most 31 bytes; 62 leaves room for a scan
// response should one ever be concatenated. The parser bounds-checks whatever
// arrives, so a longer extended advert is simply truncated here.
constexpr size_t RAW_ADV_BYTES = 62;

// Raw advertisement handed from the BLE host task to the UI task. Fixed size: the
// callback must not allocate.
struct RawAdv {
  uint8_t bytes[RAW_ADV_BYTES];
  uint16_t len;
  int8_t rssi;
  uint8_t address[6];  // NimBLE native order (little-endian), reversed for display in record()
};

constexpr size_t QUEUE_DEPTH = 32;
QueueHandle_t g_queue = nullptr;

// Passive scan timing in 0.625ms BLE units. Window == interval means we listen
// continuously on the active channel; the controller rotates the three primary
// advertising channels for us.
constexpr uint16_t SCAN_INTERVAL = 160;  // 100 ms
constexpr uint16_t SCAN_WINDOW = 160;    // 100 ms

// Runs in the NimBLE host task (NOT an ISR, unlike the Wi-Fi promiscuous path --
// so a plain xQueueSend, no FromISR/IRAM_ATTR). Kept minimal: copy the payload,
// RSSI, and address into the fixed queue; no allocation, no blocking. A full queue
// drops the advert, which is fine -- the device re-advertises within ~100ms.
class ScanCallbacks final : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
    if (!g_queue || !advertisedDevice) return;
    RawAdv adv;
    const std::vector<uint8_t>& payload = advertisedDevice->getPayload();
    adv.len = payload.size() > sizeof(adv.bytes) ? static_cast<uint16_t>(sizeof(adv.bytes))
                                                 : static_cast<uint16_t>(payload.size());
    std::memcpy(adv.bytes, payload.data(), adv.len);
    adv.rssi = static_cast<int8_t>(advertisedDevice->getRSSI());
    std::memcpy(adv.address, advertisedDevice->getAddress().getVal(), 6);
    xQueueSend(g_queue, &adv, 0);
  }
};

ScanCallbacks g_callbacks;
#endif  // ARDUINO_ARCH_ESP32
}  // namespace

bool BleScanner::begin(std::vector<BleSignature> sigs) {
  signatures = std::move(sigs);
  found.clear();
  adverts = 0;

#if defined(ARDUINO_ARCH_ESP32)
  if (!g_queue) g_queue = xQueueCreate(QUEUE_DEPTH, sizeof(RawAdv));
  if (!g_queue) {
    LOG_ERR("BLEAUDIT", "Failed to create advert queue");
    return false;
  }
  if (!NimBLEDevice::isInitialized()) NimBLEDevice::init("");
  NimBLEScan* scan = NimBLEDevice::getScan();
  if (!scan) {
    LOG_ERR("BLEAUDIT", "No BLE scan handle");
    return false;
  }
  // wantDuplicates = true: count repeat adverts from the same device so RSSI and
  // the hit counter keep updating.
  scan->setScanCallbacks(&g_callbacks, true);
  scan->setActiveScan(false);  // PASSIVE: never transmit a scan request
  scan->setInterval(SCAN_INTERVAL);
  scan->setWindow(SCAN_WINDOW);
  // Use the callback only; don't accumulate an internal results table.
  scan->setMaxResults(0);
  if (!scan->start(0, false)) {  // duration 0 = scan continuously
    LOG_ERR("BLEAUDIT", "Could not start passive scan");
    return false;
  }
  active = true;
  LOG_INF("BLEAUDIT", "Passive BLE scanner started (%u signatures)", static_cast<unsigned>(signatures.size()));
  return true;
#else
  // No radio on the host: report unavailable so the UI says so plainly.
  LOG_INF("BLEAUDIT", "No radio on this platform; scanner inactive");
  return false;
#endif
}

void BleScanner::end() {
#if defined(ARDUINO_ARCH_ESP32)
  if (active) {
    NimBLEScan* scan = NimBLEDevice::getScan();
    if (scan) {
      scan->stop();
      scan->clearResults();
    }
  }
  // Leave the controller initialized (cheap to re-enter); only drain our queue.
  if (g_queue) xQueueReset(g_queue);
#endif
  active = false;
}

void BleScanner::record(const uint8_t* address, const int8_t rssi, const BleAdvertisement& adv,
                        const int matchedIndex) {
  // NimBLE hands addresses little-endian; reverse to the conventional MSB-first
  // form so the display reads like a normal BLE address.
  uint8_t disp[6];
  for (int i = 0; i < 6; ++i) disp[i] = address[5 - i];

  const uint32_t now = millis();
  for (Detection& d : found) {
    if (std::memcmp(d.address, disp, 6) == 0) {
      d.count++;
      d.lastSeenMs = now;
      if (rssi > d.rssi) d.rssi = rssi;  // keep the closest reading
      return;
    }
  }
  if (found.size() >= MAX_DETECTIONS) return;

  Detection d;
  d.name = matchedIndex >= 0 ? signatures[matchedIndex].name : "";  // empty => label by manufacturer in the UI
  d.serviceInfo = describe(adv);
  d.hasCompanyId = adv.hasCompanyId;
  d.companyId = adv.companyId;
  std::memcpy(d.address, disp, 6);
  d.rssi = rssi;
  d.count = 1;
  d.firstSeenMs = now;
  d.lastSeenMs = now;
  found.push_back(std::move(d));
  LOG_INF("BLEAUDIT", "Match: %s (%02X:%02X:%02X:%02X:%02X:%02X) rssi=%d", d.name.c_str(), disp[0], disp[1], disp[2],
          disp[3], disp[4], disp[5], rssi);
}

bool BleScanner::drain() {
  bool changed = false;
#if defined(ARDUINO_ARCH_ESP32)
  if (!g_queue) return false;
  RawAdv adv;
  while (xQueueReceive(g_queue, &adv, 0) == pdTRUE) {
    adverts++;
    BleAdvertisement parsed;
    if (!parseAdvertisement(adv.bytes, adv.len, parsed)) continue;
    const int idx = matchBle(signatures, parsed);
    // Record a curated-signature match, or any advertiser carrying a company id
    // (the UI labels those by manufacturer via the BLE company-id db).
    if (idx < 0 && !parsed.hasCompanyId) continue;
    record(adv.address, adv.rssi, parsed, idx);
    changed = true;
  }
#endif
  return changed;
}

}  // namespace bleaudit
