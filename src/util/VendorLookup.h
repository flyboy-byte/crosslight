#pragma once

#include <cstdint>
#include <string>

// Pure record parsing + binary search for the on-SD vendor databases
// (scripts/gen_vendor_db.py output). No I/O, no HAL -- so it is host-tested
// (test/vendor_lookup/). VendorDb (src/util/VendorDb.{h,cpp}) wraps the same
// format with HalFile seeks for the real multi-MB-on-SD lookup; this header is
// the correctness core both share.
//
// File format (little-endian): 16-byte header
//   'C''L''V''1' | uint32 count | uint32 record_size(=36) | uint32 reserved
// then `count` records, SORTED ascending by key, each 36 bytes:
//   uint32 key | 32-byte name (UTF-8, NUL-padded)
namespace vendordb {

constexpr int HEADER_SIZE = 16;
constexpr int NAME_LEN = 32;
constexpr int RECORD_SIZE = 4 + NAME_LEN;  // 36

// True if `header` (>= HEADER_SIZE bytes) is a valid CLV1 header; fills count.
inline bool parseHeader(const uint8_t* header, uint32_t& count) {
  if (!header) return false;
  if (header[0] != 'C' || header[1] != 'L' || header[2] != 'V' || header[3] != '1') return false;
  count = static_cast<uint32_t>(header[4]) | (static_cast<uint32_t>(header[5]) << 8) |
          (static_cast<uint32_t>(header[6]) << 16) | (static_cast<uint32_t>(header[7]) << 24);
  const uint32_t recordSize = static_cast<uint32_t>(header[8]) | (static_cast<uint32_t>(header[9]) << 8) |
                              (static_cast<uint32_t>(header[10]) << 16) | (static_cast<uint32_t>(header[11]) << 24);
  return recordSize == RECORD_SIZE;
}

// The uint32 key of a 36-byte record (little-endian).
inline uint32_t keyOf(const uint8_t* record) {
  return static_cast<uint32_t>(record[0]) | (static_cast<uint32_t>(record[1]) << 8) |
         (static_cast<uint32_t>(record[2]) << 16) | (static_cast<uint32_t>(record[3]) << 24);
}

// The name of a 36-byte record (NUL-terminated within its 32-byte field).
inline std::string nameOf(const uint8_t* record) {
  const char* name = reinterpret_cast<const char*>(record + 4);
  size_t len = 0;
  while (len < NAME_LEN && name[len] != '\0') ++len;
  return std::string(name, len);
}

// Binary search a fully-in-memory record block (`records` = first record, not the
// header) of `count` sorted records. Returns the name, or "" if not found.
inline std::string lookupInBuffer(const uint8_t* records, const int count, const uint32_t key) {
  int lo = 0;
  int hi = count - 1;
  while (lo <= hi) {
    const int mid = lo + (hi - lo) / 2;
    const uint32_t k = keyOf(records + static_cast<size_t>(mid) * RECORD_SIZE);
    if (k == key) return nameOf(records + static_cast<size_t>(mid) * RECORD_SIZE);
    if (k < key)
      lo = mid + 1;
    else
      hi = mid - 1;
  }
  return "";
}

// OUI key from the first three bytes of a MAC/BSSID (0x00AABBCC).
inline uint32_t ouiKey(const uint8_t mac[6]) {
  return (static_cast<uint32_t>(mac[0]) << 16) | (static_cast<uint32_t>(mac[1]) << 8) | static_cast<uint32_t>(mac[2]);
}

}  // namespace vendordb
