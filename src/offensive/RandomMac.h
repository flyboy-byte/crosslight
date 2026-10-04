#pragma once

#include <cstdint>

// A synthesized (not manufacturer-assigned) MAC address for active tools that
// need to present as many distinct BSSIDs -- a beacon flood advertises each
// fake AP from its own address rather than repeating one BSSID, which is what
// makes scanners list them as separate networks.
//
// Pure bit-twiddling, so it is host-testable with no RNG/radio dependency: the
// caller supplies 6 already-random bytes (from esp_random() on device), and
// this just legalizes them as a MAC.
namespace wifiaudit {

// Sets the locally-administered bit (bit 1 of the first octet) and clears the
// multicast/group bit (bit 0), turning any 6 bytes into a valid unicast,
// locally-administered address -- the standard way to mint an address that
// cannot collide with a real manufacturer's OUI block.
inline void makeLocallyAdministered(uint8_t mac[6]) { mac[0] = static_cast<uint8_t>((mac[0] | 0x02) & 0xFE); }

}  // namespace wifiaudit
