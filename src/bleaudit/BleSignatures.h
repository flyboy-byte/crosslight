#pragma once

#include <vector>

#include "BleSignature.h"

// Loading of BLE detection signatures from the SD card. Separate from the matcher
// (BleMatcher.cpp) so the matcher stays dependency-free and host-testable; this
// side pulls in ArduinoJson and HalStorage and is firmware-only.
namespace bleaudit {

// Where the user maintains the signature list. Kept on SD, not in flash, so it
// can be updated without reflashing as fingerprints change.
constexpr const char* SIGNATURE_PATH = "/bleaudit/signatures.json";

// Parses `path` into `out`. False (leaving `out` untouched) if the file is
// missing, unreadable, malformed, or contains no usable signatures. company_id
// and service_uuids accept hex strings ("0x004C", "004C", "4C"); a bad value is
// skipped, not fatal.
bool loadSignatures(const char* path, std::vector<BleSignature>& out);

}  // namespace bleaudit
