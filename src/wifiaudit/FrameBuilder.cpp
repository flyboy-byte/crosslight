#include "FrameBuilder.h"

#include <cstring>

namespace wifiaudit {

namespace {
// Frame-control byte 0: management type (bits 2-3 = 00) with the subtype in the
// high nibble. These match WifiFrame.cpp's SUBTYPE_* so the builder and parser
// agree. FC byte 1 (flags) is 0 for a locally generated management frame.
constexpr uint8_t FC_DEAUTH = 0xC0;    // subtype 12
constexpr uint8_t FC_DISASSOC = 0xA0;  // subtype 10
constexpr uint8_t FC_BEACON = 0x80;    // subtype 8

constexpr size_t MAC_HEADER_LEN = 24;  // frame control..addr3 + sequence control
constexpr size_t FIXED_BODY_LEN = 12;  // timestamp(8) + beacon interval(2) + capability(2)

// Beacon fixed-body values. Interval 0x0064 = 100 TU (~102ms), the usual rate.
// Capability 0x0401 = ESS (infrastructure AP) + Short Slot Time, with the
// Privacy bit clear so the advertised network reads as Open.
constexpr uint8_t BEACON_INTERVAL_LO = 0x64;
constexpr uint8_t BEACON_INTERVAL_HI = 0x00;
constexpr uint8_t CAPABILITY_LO = 0x01;
constexpr uint8_t CAPABILITY_HI = 0x04;

// Information-element ids (same numbering the parser walks).
constexpr uint8_t IE_SSID = 0x00;
constexpr uint8_t IE_SUPPORTED_RATES = 0x01;
constexpr uint8_t IE_DS_PARAM = 0x03;

// Supported Rates IE value: 1/2/5.5/11 Mbps as basic rates (high bit set) plus
// 6/12/24/54 Mbps -- a plausible 802.11b/g rate set that clients accept.
constexpr uint8_t SUPPORTED_RATES[8] = {0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c};

constexpr uint8_t BROADCAST_ADDR[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Write the 24-byte MAC header common to every management frame we build:
//   FC(2) | duration(2) | addr1(6) | addr2(6) | addr3(6) | seq ctrl(2)
// Duration and sequence control are left 0: we do not need to set them for these
// frames, and the driver assigns the real sequence number on transmit.
void writeMacHeader(uint8_t* out, const uint8_t fc0, const uint8_t addr1[6], const uint8_t addr2[6],
                    const uint8_t addr3[6]) {
  out[0] = fc0;
  out[1] = 0x00;  // FC flags
  out[2] = 0x00;  // duration
  out[3] = 0x00;
  std::memcpy(out + 4, addr1, 6);    // addr1 (destination)
  std::memcpy(out + 10, addr2, 6);   // addr2 (source / transmitter)
  std::memcpy(out + 16, addr3, 6);   // addr3 (BSSID)
  out[22] = 0x00;                    // sequence control
  out[23] = 0x00;
}

// Deauth and disassoc are identical 26-byte frames apart from the subtype, so
// both route through here.
size_t buildDeauthLike(uint8_t* out, const size_t cap, const uint8_t fc0, const uint8_t bssid[6],
                       const uint8_t client[6], const uint16_t reasonCode) {
  if (!out || cap < DEAUTH_FRAME_LEN) return 0;
  // addr1 = destination (the client, or broadcast); addr2 = addr3 = the BSSID we
  // are impersonating, so the client believes its AP disconnected it.
  writeMacHeader(out, fc0, client, bssid, bssid);
  out[24] = static_cast<uint8_t>(reasonCode & 0xFF);         // reason code, little-endian
  out[25] = static_cast<uint8_t>((reasonCode >> 8) & 0xFF);
  return DEAUTH_FRAME_LEN;
}
}  // namespace

size_t buildDeauth(uint8_t* out, const size_t cap, const uint8_t bssid[6], const uint8_t client[6],
                   const uint16_t reasonCode) {
  return buildDeauthLike(out, cap, FC_DEAUTH, bssid, client, reasonCode);
}

size_t buildDisassoc(uint8_t* out, const size_t cap, const uint8_t bssid[6], const uint8_t client[6],
                     const uint16_t reasonCode) {
  return buildDeauthLike(out, cap, FC_DISASSOC, bssid, client, reasonCode);
}

size_t buildBeacon(uint8_t* out, const size_t cap, const uint8_t bssid[6], const char* ssid, const size_t ssidLen,
                   const uint8_t channel) {
  if (!out || !ssid) return 0;
  if (ssidLen > MAX_SSID_LEN) return 0;  // a 1-byte IE length cannot exceed the SSID cap
  // header + fixed body + SSID IE(2+n) + Supported Rates IE(2+8) + DS Param IE(2+1)
  const size_t total = MAC_HEADER_LEN + FIXED_BODY_LEN + (2 + ssidLen) + (2 + sizeof(SUPPORTED_RATES)) + 3;
  if (cap < total) return 0;

  // addr1 broadcast: a beacon is addressed to every station in range.
  writeMacHeader(out, FC_BEACON, BROADCAST_ADDR, bssid, bssid);
  size_t i = MAC_HEADER_LEN;

  std::memset(out + i, 0x00, 8);  // timestamp: the driver fills the real TSF on tx
  i += 8;
  out[i++] = BEACON_INTERVAL_LO;
  out[i++] = BEACON_INTERVAL_HI;
  out[i++] = CAPABILITY_LO;
  out[i++] = CAPABILITY_HI;

  out[i++] = IE_SSID;
  out[i++] = static_cast<uint8_t>(ssidLen);
  std::memcpy(out + i, ssid, ssidLen);
  i += ssidLen;

  out[i++] = IE_SUPPORTED_RATES;
  out[i++] = static_cast<uint8_t>(sizeof(SUPPORTED_RATES));
  std::memcpy(out + i, SUPPORTED_RATES, sizeof(SUPPORTED_RATES));
  i += sizeof(SUPPORTED_RATES);

  out[i++] = IE_DS_PARAM;
  out[i++] = 0x01;  // DS Parameter Set length is always 1 (the channel)
  out[i++] = channel;

  return i;  // == total
}

}  // namespace wifiaudit
