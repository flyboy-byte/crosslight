#include "BleAd.h"

#include <cstring>

namespace bleaudit {

namespace {
// AD (Advertising Data) type codes from the Bluetooth Core Supplement. Only the
// ones this toolkit reads are named.
constexpr uint8_t AD_FLAGS = 0x01;
constexpr uint8_t AD_UUID16_INCOMPLETE = 0x02;
constexpr uint8_t AD_UUID16_COMPLETE = 0x03;
constexpr uint8_t AD_UUID128_INCOMPLETE = 0x06;
constexpr uint8_t AD_UUID128_COMPLETE = 0x07;
constexpr uint8_t AD_NAME_SHORT = 0x08;
constexpr uint8_t AD_NAME_COMPLETE = 0x09;
constexpr uint8_t AD_MANUFACTURER = 0xFF;

constexpr size_t UUID16_BYTES = 2;
constexpr size_t UUID128_BYTES = 16;
constexpr size_t COMPANY_ID_BYTES = 2;

// Read a run of 16-bit UUIDs (little-endian) from an AD structure body. A
// trailing odd byte that cannot form a full UUID is ignored -- never over-read.
void readUuids16(const uint8_t* data, size_t dataLen, std::vector<uint16_t>& out) {
  for (size_t j = 0; j + UUID16_BYTES <= dataLen; j += UUID16_BYTES) {
    out.push_back(static_cast<uint16_t>(data[j] | (data[j + 1] << 8)));
  }
}

// Read a run of 128-bit UUIDs (raw 16-byte, advertising/little-endian order). A
// trailing partial UUID is ignored.
void readUuids128(const uint8_t* data, size_t dataLen, std::vector<std::array<uint8_t, 16>>& out) {
  for (size_t j = 0; j + UUID128_BYTES <= dataLen; j += UUID128_BYTES) {
    std::array<uint8_t, 16> uuid{};
    std::memcpy(uuid.data(), data + j, UUID128_BYTES);
    out.push_back(uuid);
  }
}
}  // namespace

bool parseAdvertisement(const uint8_t* buf, const size_t len, BleAdvertisement& out) {
  if (!buf || len == 0) return false;
  out = BleAdvertisement{};

  size_t i = 0;
  while (i < len) {
    const uint8_t adLen = buf[i];
    // A zero length marks the end of significant data; the rest is padding.
    if (adLen == 0) break;
    // The structure occupies the length byte plus `adLen` bytes (1 type +
    // adLen-1 data). If it runs past the buffer the advert is truncated -- stop
    // rather than over-read a claimed-but-absent tail.
    if (i + 1 + adLen > len) break;

    const uint8_t type = buf[i + 1];
    const uint8_t* data = buf + i + 2;
    const size_t dataLen = static_cast<size_t>(adLen) - 1;

    switch (type) {
      case AD_FLAGS:
        if (dataLen >= 1) {
          out.flags = data[0];
          out.hasFlags = true;
        }
        break;
      case AD_UUID16_INCOMPLETE:
      case AD_UUID16_COMPLETE:
        readUuids16(data, dataLen, out.serviceUuids16);
        break;
      case AD_UUID128_INCOMPLETE:
      case AD_UUID128_COMPLETE:
        readUuids128(data, dataLen, out.serviceUuids128);
        break;
      case AD_NAME_COMPLETE:
        // Complete name is authoritative; always take it.
        out.name.assign(reinterpret_cast<const char*>(data), dataLen);
        break;
      case AD_NAME_SHORT:
        // Shortened name only fills in when no (complete) name is set yet, so a
        // complete name already read is not clobbered.
        if (out.name.empty()) out.name.assign(reinterpret_cast<const char*>(data), dataLen);
        break;
      case AD_MANUFACTURER:
        out.hasManufacturerData = true;
        if (dataLen >= COMPANY_ID_BYTES) {
          out.companyId = static_cast<uint16_t>(data[0] | (data[1] << 8));
          out.hasCompanyId = true;
          // Apple packs a message-type byte right after the company ID; it tells
          // FindMy from AirPods from Nearby without decoding the whole payload.
          if (out.companyId == APPLE_COMPANY_ID && dataLen >= COMPANY_ID_BYTES + 1) {
            out.appleType = data[COMPANY_ID_BYTES];
            out.hasAppleType = true;
          }
        }
        break;
      default:
        break;
    }

    i += 1 + adLen;
  }

  return true;
}

}  // namespace bleaudit
