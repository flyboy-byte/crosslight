#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "BleSignature.h"

// Passive BLE device scanner. Puts the controller into a passive observer scan
// and matches advertisements against loaded signatures. It only ever receives:
// passive scanning transmits NO scan requests, and this never advertises,
// connects, or pairs. It is purely awareness of what is already in the air.
//
// Shaped after FlockScanner: the NimBLE scan callback (BLE host task) does the
// minimum -- copy the raw advertising payload, RSSI, and address into a fixed
// queue, no allocation. drain() runs on the UI task and does the real work with
// the host-tested parseAdvertisement()/matchBle(), so nothing untested runs in
// the radio callback. begin() returns false on the host (no radio), so the UI can
// show that rather than a dead scan.
namespace bleaudit {

struct Detection {
  std::string name;         // curated-signature label, or empty when only a company id identified it
  std::string serviceInfo;  // compact "what was seen": local name, company, service, Apple type
  uint16_t companyId = 0;   // advertised BLE company id, for manufacturer lookup (VendorDb btcid.bin)
  bool hasCompanyId = false;
  uint8_t address[6] = {};  // BLE device address, MSB-first for display
  int8_t rssi = 0;          // strongest RSSI seen for this device (closer = higher)
  uint32_t count = 0;       // advertisements matched from this device
  uint32_t firstSeenMs = 0;
  uint32_t lastSeenMs = 0;
};

class BleScanner {
 public:
  // Starts a passive scan with `signatures`. False if the radio is unavailable
  // (e.g. the desktop simulator has none), in which case the caller should show
  // that rather than a dead scan.
  bool begin(std::vector<BleSignature> signatures);
  // Stops the scan and releases the radio. Safe to call if begin() failed or was
  // never called.
  void end();
  bool running() const { return active; }

  // Drain queued advertisements, update the detection set, and return true if
  // anything changed (new device or updated RSSI/count) since the last drain.
  bool drain();

  const std::vector<Detection>& detections() const { return found; }
  uint32_t advertsSeen() const { return adverts; }

 private:
  void record(const uint8_t* address, int8_t rssi, const BleAdvertisement& adv, int matchedIndex);

  std::vector<BleSignature> signatures;
  std::vector<Detection> found;
  bool active = false;
  uint32_t adverts = 0;
};

}  // namespace bleaudit
