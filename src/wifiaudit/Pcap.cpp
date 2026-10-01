#include "Pcap.h"

namespace wifiaudit {

namespace {
// Little-endian stores, so the file matches the a1b2c3d4 magic (host byte order,
// microsecond timestamps) regardless of the compiler's native endianness.
void put16(uint8_t* p, const uint16_t v) {
  p[0] = static_cast<uint8_t>(v & 0xFF);
  p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

void put32(uint8_t* p, const uint32_t v) {
  p[0] = static_cast<uint8_t>(v & 0xFF);
  p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
  p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
  p[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}
}  // namespace

void pcapWriteGlobalHeader(uint8_t out[PCAP_GLOBAL_HEADER_LEN], const uint32_t linktype, const uint32_t snaplen) {
  put32(out + 0, 0xA1B2C3D4);  // magic
  put16(out + 4, 2);           // version major
  put16(out + 6, 4);           // version minor
  put32(out + 8, 0);           // thiszone (GMT)
  put32(out + 12, 0);          // sigfigs
  put32(out + 16, snaplen);    // snaplen
  put32(out + 20, linktype);   // network / linktype
}

void pcapWriteRecordHeader(uint8_t out[PCAP_RECORD_HEADER_LEN], const uint32_t tsSec, const uint32_t tsUsec,
                           const uint32_t inclLen, const uint32_t origLen) {
  put32(out + 0, tsSec);
  put32(out + 4, tsUsec);
  put32(out + 8, inclLen);
  put32(out + 12, origLen);
}

}  // namespace wifiaudit
