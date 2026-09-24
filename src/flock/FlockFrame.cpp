#include "FlockSignature.h"

#include <cstring>

namespace flock {

namespace {
// 802.11 frame control, first two bytes of the header.
constexpr uint8_t FC_TYPE_MASK = 0x0C;      // bits 2-3 = type
constexpr uint8_t FC_TYPE_MGMT = 0x00;      // management
constexpr uint8_t FC_SUBTYPE_MASK = 0xF0;   // bits 4-7 = subtype
constexpr uint8_t SUBTYPE_PROBE_REQ = 0x40;  // 4
constexpr uint8_t SUBTYPE_PROBE_RESP = 0x50;  // 5
constexpr uint8_t SUBTYPE_BEACON = 0x80;    // 8

constexpr size_t MAC_HEADER_LEN = 24;   // frame control..addr3 + seq ctrl
constexpr size_t ADDR2_OFFSET = 10;     // source/transmitter address
constexpr size_t FIXED_BODY_LEN = 12;   // beacon/probe-resp: timestamp+interval+cap
constexpr uint8_t TAG_SSID = 0x00;      // element id for SSID
constexpr size_t MAX_SSID_LEN = 32;

// Walk the tagged parameters starting at `body` for the SSID element.
void extractSsid(const uint8_t* body, size_t bodyLen, std::string& ssid) {
  size_t i = 0;
  while (i + 2 <= bodyLen) {
    const uint8_t tag = body[i];
    const uint8_t tagLen = body[i + 1];
    if (i + 2 + tagLen > bodyLen) break;  // truncated element
    if (tag == TAG_SSID) {
      const size_t n = tagLen > MAX_SSID_LEN ? MAX_SSID_LEN : tagLen;
      ssid.assign(reinterpret_cast<const char*>(body + i + 2), n);
      return;
    }
    i += 2 + tagLen;
  }
}
}  // namespace

bool parseManagementFrame(const uint8_t* buf, const size_t len, Observation& out) {
  if (!buf || len < MAC_HEADER_LEN) return false;
  const uint8_t fc = buf[0];
  if ((fc & FC_TYPE_MASK) != FC_TYPE_MGMT) return false;
  const uint8_t subtype = fc & FC_SUBTYPE_MASK;
  if (subtype != SUBTYPE_BEACON && subtype != SUBTYPE_PROBE_RESP && subtype != SUBTYPE_PROBE_REQ) return false;

  std::memcpy(out.mac, buf + ADDR2_OFFSET, 6);
  out.ssid.clear();

  if (subtype == SUBTYPE_PROBE_REQ) {
    // No fixed body: tagged parameters begin right after the header.
    extractSsid(buf + MAC_HEADER_LEN, len - MAC_HEADER_LEN, out.ssid);
  } else {
    const size_t bodyStart = MAC_HEADER_LEN + FIXED_BODY_LEN;
    if (len > bodyStart) extractSsid(buf + bodyStart, len - bodyStart, out.ssid);
  }
  return true;
}

}  // namespace flock
