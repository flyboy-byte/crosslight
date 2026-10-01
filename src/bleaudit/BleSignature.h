#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "BleAd.h"

// Passive identification of BLE devices by their advertisement fingerprint.
// Purely receive-side: the scanner listens and never transmits, so this observes
// what is already being broadcast -- it is awareness, not interference.
//
// A signature matches an advertisement by its manufacturer company ID, any of a
// set of 16-bit service UUIDs, and/or a substring of its advertised name.
// Fingerprints shift as vendors rotate firmware, so signatures live in an SD
// file the user maintains (/bleaudit/signatures.json), never baked into the
// image. This header is the pure model plus the matcher; JSON loading is separate
// (BleSignatures.cpp) so the matcher stays dependency-free and host-testable.
namespace bleaudit {

struct BleSignature {
  std::string name;      // human label, e.g. "Apple device" / "Tile tracker"
  std::string category;  // free-form tag, e.g. "tracker" / "wearable"

  // Manufacturer company identifier. A frame matches if its 0xFF company ID
  // equals this. hasCompanyId == false means company ID is not part of this
  // signature.
  uint16_t companyId = 0;
  bool hasCompanyId = false;

  // 16-bit service UUIDs. A frame matches if it advertises any of these. Empty =
  // service UUID is not part of this signature.
  std::vector<uint16_t> serviceUuids16;

  // Case-insensitive name substrings. A frame matches if its advertised name
  // contains any of these. Empty = name is not part of this signature.
  std::vector<std::string> nameContains;
};

// Index of the first matching signature, or -1 if none match. A signature with
// several criteria matches on ANY of them (the company ID, OR any listed service
// UUID, OR any listed name substring): devices rotate or omit one field but not
// the others, so requiring all would miss real hardware. A signature with NO
// criteria never matches, so a blank/malformed entry cannot flag every device on
// the air.
int matchBle(const std::vector<BleSignature>& signatures, const BleAdvertisement& adv);

}  // namespace bleaudit
