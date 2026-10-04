#include "BleSpoofer.h"

#include <Logging.h>

#include "offensive/ActiveAuditGate.h"
#include "offensive/AttackTx.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <NimBLEDevice.h>

#include "BleBeacon.h"
#endif

namespace bleaudit {

namespace {
struct Profile {
  const char* name;
};
// Generic test transmitters only -- see the header comment.
constexpr Profile kProfiles[] = {
    {"Test Device"},  // 0: a plainly-named connectable-looking advertiser
    {"iBeacon"},      // 1: an example iBeacon (all-zero UUID, major/minor 0)
};
constexpr int kProfileCount = sizeof(kProfiles) / sizeof(kProfiles[0]);
}  // namespace

int BleSpoofer::profileCount() { return kProfileCount; }

const char* BleSpoofer::profileName(const int index) {
  if (index < 0 || index >= kProfileCount) return "";
  return kProfiles[index].name;
}

bool BleSpoofer::begin() {
  advertising = false;
#if defined(ARDUINO_ARCH_ESP32)
  if (!NimBLEDevice::isInitialized()) NimBLEDevice::init("");
  initialized = true;
  LOG_INF("BLEAUDIT", "BLE spoofer ready");
  return true;
#else
  LOG_INF("BLEAUDIT", "No radio on this platform; BLE spoofer inactive");
  return false;
#endif
}

bool BleSpoofer::advertise(const int profileIndex) {
  if (profileIndex < 0 || profileIndex >= kProfileCount) return false;
  // Same double gate as every active tool: do nothing unless this build can
  // transmit AND the operator has cleared the per-boot audit gate.
  if (!wifiaudit::buildSupportsActiveAudit() || !wifiaudit::ActiveAuditGate::confirmed()) return false;

#if defined(ARDUINO_ARCH_ESP32)
  if (!initialized) return false;
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  if (!adv) return false;
  adv->stop();

  NimBLEAdvertisementData data;
  data.setFlags(0x06);  // LE General Discoverable + BR/EDR not supported

  if (profileIndex == 1) {
    // iBeacon: example all-zero proximity UUID, major/minor 0, measured power -59.
    uint8_t uuid[16] = {0};
    uint8_t manuf[IBEACON_MANUF_LEN];
    const size_t n = buildIBeaconManufacturerData(manuf, sizeof(manuf), uuid, 0, 0, -59);
    data.setManufacturerData(manuf, n);
  } else {
    data.setName(kProfiles[profileIndex].name);
  }

  adv->setAdvertisementData(data);
  adv->setConnectableMode(0);  // non-connectable: a broadcaster, not a server
  if (!adv->start()) {
    LOG_ERR("BLEAUDIT", "Could not start advertising");
    return false;
  }
  advertising = true;
  LOG_INF("BLEAUDIT", "BLE advertising: %s", kProfiles[profileIndex].name);
  return true;
#else
  return false;
#endif
}

void BleSpoofer::stop() {
#if defined(ARDUINO_ARCH_ESP32)
  if (initialized) {
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    if (adv) adv->stop();
  }
#endif
  advertising = false;
}

void BleSpoofer::end() {
  stop();
  // Leave the controller initialized (cheap to re-enter), matching BleScanner.
}

}  // namespace bleaudit
