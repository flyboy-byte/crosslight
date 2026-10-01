#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

// Pure builder for an iBeacon manufacturer-data payload -- the inverse of the
// BLE advert parser, the same way wifiaudit's FrameBuilder is the inverse of
// WifiFrame. No radio, no heap, so the exact byte layout is host-tested; the
// actual broadcast is isolated in BleSpoofer (NimBLE) and gated.
//
// iBeacon layout (the de-facto standard): company id 0x004C little-endian,
// beacon type 0x02, length 0x15 (21), 16-byte proximity UUID, 2-byte major and
// 2-byte minor (both big-endian, as iBeacon specifies), and a 1-byte measured
// TX power. Company id 0x004C is simply what makes a scanner recognize the
// frame AS an iBeacon -- it is the format, not an impersonation of a specific
// device.
namespace bleaudit {

// 2 (company id) + 1 (type) + 1 (len) + 16 (uuid) + 2 (major) + 2 (minor) + 1 (tx).
constexpr size_t IBEACON_MANUF_LEN = 25;

// Serialize an iBeacon manufacturer-data blob into `out` (>= IBEACON_MANUF_LEN).
// major/minor are written big-endian per the iBeacon convention. Returns the
// number of bytes written, or 0 if `cap` is too small.
inline size_t buildIBeaconManufacturerData(uint8_t* out, const size_t cap, const uint8_t uuid[16], const uint16_t major,
                                           const uint16_t minor, const int8_t txPower) {
  if (!out || cap < IBEACON_MANUF_LEN) return 0;
  size_t i = 0;
  out[i++] = 0x4C;  // company id 0x004C, little-endian
  out[i++] = 0x00;
  out[i++] = 0x02;  // iBeacon type
  out[i++] = 0x15;  // remaining length (21 bytes)
  std::memcpy(out + i, uuid, 16);
  i += 16;
  out[i++] = static_cast<uint8_t>(major >> 8);  // major, big-endian
  out[i++] = static_cast<uint8_t>(major & 0xFF);
  out[i++] = static_cast<uint8_t>(minor >> 8);  // minor, big-endian
  out[i++] = static_cast<uint8_t>(minor & 0xFF);
  out[i++] = static_cast<uint8_t>(txPower);
  return i;  // == IBEACON_MANUF_LEN
}

}  // namespace bleaudit
