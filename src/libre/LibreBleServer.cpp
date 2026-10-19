#include "LibreBleServer.h"

#include <Logging.h>

#include <cstring>

#if defined(ARDUINO_ARCH_ESP32)
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#endif

namespace libre {

#if defined(ARDUINO_ARCH_ESP32)
namespace {

// Raw bytes handed from the BLE host task to the UI task. Fixed size, no
// allocation in the callback -- same discipline as bleaudit::BleScanner's
// RawAdv queue.
struct RawWrite {
  uint8_t bytes[kBlePacketSize];
  uint16_t len;
};

constexpr size_t QUEUE_DEPTH = 4;  // writes land ~once a minute; never a flood
QueueHandle_t g_queue = nullptr;
NimBLEServer* g_server = nullptr;
bool g_connected = false;
bool g_advertisedUuid = false;

// Long interval + slave latency: the companion only writes about once a
// minute, so there's no reason to hold the link awake faster than that
// (crosslight/PLAN.md power design, option 3). 1000ms interval, latency 4
// (peripheral may skip up to 4 connection events), 20s supervision timeout
// -- comfortably above the BLE-spec floor of (1+latency)*maxInterval*2.
constexpr uint16_t CONN_INTERVAL_UNITS = 800;  // 800 * 1.25ms = 1000ms
constexpr uint16_t CONN_LATENCY = 4;
constexpr uint16_t CONN_TIMEOUT_UNITS = 2000;  // 2000 * 10ms = 20s

class ServerCallbacks final : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override {
    g_connected = true;
    server->updateConnParams(connInfo.getConnHandle(), CONN_INTERVAL_UNITS, CONN_INTERVAL_UNITS, CONN_LATENCY,
                              CONN_TIMEOUT_UNITS);
    LOG_INF("LIBRE_BLE", "Companion connected");
  }
  void onDisconnect(NimBLEServer* /*server*/, NimBLEConnInfo& /*connInfo*/, int reason) override {
    g_connected = false;
    LOG_INF("LIBRE_BLE", "Companion disconnected (reason=%d)", reason);
  }
};

ServerCallbacks g_serverCallbacks;

class CharCallbacks final : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* chr, NimBLEConnInfo& /*connInfo*/) override {
    const NimBLEAttValue value = chr->getValue();
    if (!g_queue) return;
    RawWrite w;
    w.len = value.size() > sizeof(w.bytes) ? static_cast<uint16_t>(sizeof(w.bytes))
                                            : static_cast<uint16_t>(value.size());
    std::memcpy(w.bytes, value.data(), w.len);
    // Log the raw bytes as hex so a write can be traced end to end from the
    // serial console without guessing whether it reached this callback.
    char hex[kBlePacketSize * 3 + 1];
    size_t pos = 0;
    for (uint16_t i = 0; i < w.len && pos + 3 < sizeof(hex); ++i) {
      pos += snprintf(hex + pos, sizeof(hex) - pos, "%02X ", w.bytes[i]);
    }
    LOG_INF("LIBRE_BLE", "onWrite %u bytes: %s", static_cast<unsigned>(w.len), hex);
    xQueueSend(g_queue, &w, 0);
  }
};

CharCallbacks g_charCallbacks;

}  // namespace
#endif  // ARDUINO_ARCH_ESP32

bool LibreBleServer::begin() {
  if (active) return true;

#if defined(ARDUINO_ARCH_ESP32)
  if (!g_queue) g_queue = xQueueCreate(QUEUE_DEPTH, sizeof(RawWrite));
  if (!g_queue) {
    LOG_ERR("LIBRE_BLE", "Failed to create write queue");
    return false;
  }

  if (!NimBLEDevice::isInitialized()) {
    NimBLEDevice::init("CrossLight");
    // Bring-up: NO encryption/bonding yet. The held-connection + WRITE_ENC
    // design failed on hardware (2026-10-18); this rebuild confirms the plain
    // write path end to end first, then Just-Works bonding comes back as a
    // separate, deliberate step. See PLAN.md "BLE rework".
  }

  // Server/service/characteristic are created exactly once and reused across
  // every begin()/end() cycle (the screen opening and closing repeatedly) --
  // NimBLE has no "remove service" call, so creating a fresh one on every
  // begin() left stale duplicate services (same UUIDs) piling up in the GATT
  // table, confirmed on real hardware 2026-10-18 (6 identical services after
  // a few screen reopens).
  if (!g_server) {
    g_server = NimBLEDevice::createServer();
    if (!g_server) {
      LOG_ERR("LIBRE_BLE", "Could not create GATT server");
      return false;
    }
    g_server->setCallbacks(&g_serverCallbacks, /*deleteCallbacks=*/false);
    // NimBLEServer::m_advertiseOnDisconnect defaults to false (confirmed by
    // reading NimBLE-Arduino's own source, 2026-10-18) -- without this, the
    // very first disconnect after begin() permanently kills advertising
    // until the screen is closed and reopened (which re-runs begin()), with
    // no visible symptom: the screen just sits there silently unreachable.
    // Real-hardware-confirmed root cause of this session's "nothing reaches
    // the device" failures.
    g_server->advertiseOnDisconnect(true);

    NimBLEService* service = g_server->createService(kBleServiceUuid);
    if (!service) {
      LOG_ERR("LIBRE_BLE", "Could not create glucose relay service");
      return false;
    }
    // WRITE + WRITE_NR, no WRITE_ENC during bring-up: any client may write,
    // so a plain unbonded write is enough to prove the path. Encryption is
    // added back with bonding later (PLAN.md "BLE rework").
    NimBLECharacteristic* chr = service->createCharacteristic(
        kBleCharacteristicUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    if (!chr) {
      LOG_ERR("LIBRE_BLE", "Could not create glucose characteristic");
      return false;
    }
    chr->setCallbacks(&g_charCallbacks);
    service->start();
  }

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  if (!adv) {
    LOG_ERR("LIBRE_BLE", "No advertising handle");
    return false;
  }
  if (!g_advertisedUuid) {
    adv->addServiceUUID(kBleServiceUuid);
    g_advertisedUuid = true;
  }
  if (!adv->start()) {
    LOG_ERR("LIBRE_BLE", "Could not start advertising");
    return false;
  }

  active = true;
  LOG_INF("LIBRE_BLE", "Glucose relay GATT server up, advertising");
  return true;
#else
  LOG_INF("LIBRE_BLE", "No radio on this platform; server inactive");
  return false;
#endif
}

void LibreBleServer::end() {
#if defined(ARDUINO_ARCH_ESP32)
  if (active) {
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    if (adv) adv->stop();
    if (g_server) g_server->stopAdvertising();
    // Drop any live connection so the companion re-bonds cleanly next time
    // the screen opens, rather than holding a stale handle open.
    if (g_connected && g_server) {
      for (const uint16_t connHandle : g_server->getPeerDevices()) g_server->disconnect(connHandle);
    }
  }
  if (g_queue) xQueueReset(g_queue);
  g_connected = false;
#endif
  active = false;
}

bool LibreBleServer::connected() const {
#if defined(ARDUINO_ARCH_ESP32)
  return g_connected;
#else
  return false;
#endif
}

void LibreBleServer::forgetBond() {
#if defined(ARDUINO_ARCH_ESP32)
  if (g_connected && g_server) {
    for (const uint16_t connHandle : g_server->getPeerDevices()) g_server->disconnect(connHandle);
  }
  NimBLEDevice::deleteAllBonds();
  LOG_INF("LIBRE_BLE", "Cleared all bonds (%d remaining); advertising for a fresh pairing",
          NimBLEDevice::getNumBonds());
#endif
}

bool LibreBleServer::takeLatest(BlePacket& out) {
#if defined(ARDUINO_ARCH_ESP32)
  if (!g_queue) return false;
  RawWrite w;
  bool any = false;
  BlePacket decoded;
  // Drain the whole queue; only the last valid packet matters.
  while (xQueueReceive(g_queue, &w, 0) == pdTRUE) {
    if (decodeBlePacket(w.bytes, w.len, decoded)) any = true;
  }
  if (any) out = decoded;
  return any;
#else
  (void)out;
  return false;
#endif
}

}  // namespace libre
