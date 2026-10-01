#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Passive BLE advertisement parsing for the security toolkit. This header is the
// pure, host-tested half: it turns a raw advertising payload (the sequence of
// [length][AD type][data...] AD structures) into a BleAdvertisement. It never
// touches the radio -- capture lives in BleScanner.{h,cpp}, guarded for the
// simulator -- so the fiddly length-walk (which is exactly the kind of off-by-one
// that over-reads a malformed advert) stays dependency-free and testable
// off-device.
//
// Everything here is receive-side: an advertisement is already being broadcast
// into the air, so reading it is awareness, not interference. We never advertise,
// connect, or transmit.
namespace bleaudit {

// One advertising payload reduced to the fields matching and display need. Every
// `has*` flag guards an optional field so a caller can tell "absent" from "zero".
struct BleAdvertisement {
  // AD type 0x08 (shortened) / 0x09 (complete) local name. Complete wins when
  // both appear. Bytes are copied verbatim and are not guaranteed valid UTF-8.
  std::string name;

  // AD type 0xFF manufacturer-specific data. The first two bytes are the
  // company identifier, little-endian (Bluetooth SIG assigned numbers).
  uint16_t companyId = 0;
  bool hasCompanyId = false;
  bool hasManufacturerData = false;  // a 0xFF structure was present at all

  // AD types 0x02 (incomplete) / 0x03 (complete): 16-bit service UUIDs, each two
  // bytes little-endian in the advert, decoded to host order here.
  std::vector<uint16_t> serviceUuids16;

  // AD types 0x06 (incomplete) / 0x07 (complete): 128-bit service UUIDs. Stored
  // as the raw 16 bytes in advertising (little-endian) order -- matching here
  // only ever needs the 16-bit list, so this is kept for display/forensics.
  std::vector<std::array<uint8_t, 16>> serviceUuids128;

  // AD type 0x01 flags byte (LE General/Limited Discoverable, BR/EDR not
  // supported, etc.).
  uint8_t flags = 0;
  bool hasFlags = false;

  // When companyId == 0x004C (Apple), the first byte of the Apple payload after
  // the two company-ID bytes selects the message type (e.g. 0x12 = FindMy /
  // Offline Finding, 0x07 = AirPods proximity pairing, 0x10 = Nearby).
  uint8_t appleType = 0;
  bool hasAppleType = false;
};

// Apple's Bluetooth SIG company identifier, used to decode the type byte above.
constexpr uint16_t APPLE_COMPANY_ID = 0x004C;

// Parse a raw advertising payload (`buf`/`len` are the AD structures as delivered
// by the scanner, no header) into `out`. False only if the payload is null or
// empty; any content yields a best-effort BleAdvertisement. Every read is
// bounds-checked, so a truncated AD structure (a claimed length that runs past
// the buffer, or an odd trailing byte in a UUID list) is skipped rather than
// over-read. `out` is reset before parsing.
bool parseAdvertisement(const uint8_t* buf, size_t len, BleAdvertisement& out);

}  // namespace bleaudit
