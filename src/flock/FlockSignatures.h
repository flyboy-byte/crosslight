#pragma once

#include <vector>

#include "FlockSignature.h"

// Loading of detection signatures from the SD card. Separate from the matcher
// (FlockMatcher.cpp) so the matcher stays dependency-free and host-testable;
// this side pulls in ArduinoJson and HalStorage and is firmware-only.
namespace flock {

// Where the user maintains the signature list. Kept on SD, not in flash, so it
// can be updated without reflashing as fingerprints change.
constexpr const char* SIGNATURE_PATH = "/flock/signatures.json";

// Parses SIGNATURE_PATH into `out`. False (leaving `out` untouched) if the file
// is missing, unreadable, malformed, or contains no usable signatures. OUIs
// accept "AA:BB:CC", "AABBCC", or "AA-BB-CC"; a bad OUI is skipped, not fatal.
bool loadSignatures(const char* path, std::vector<Signature>& out);

}  // namespace flock
