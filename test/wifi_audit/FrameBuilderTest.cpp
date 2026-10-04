#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

#include "offensive/FrameBuilder.h"
#include "wifiaudit/WifiFrame.h"  // cross-check: frames we build must parse back

namespace {

// Distinctive addresses so a misplaced field is obvious in a failure.
constexpr uint8_t BSSID[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
constexpr uint8_t CLIENT[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
constexpr uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

}  // namespace

TEST(FrameBuilder, DeauthExactHeaderBytes) {
  uint8_t out[64] = {};
  const size_t n = wifiaudit::buildDeauth(out, sizeof(out), BSSID, CLIENT, wifiaudit::REASON_CLASS3_FROM_NONASSOC);
  ASSERT_EQ(n, 26u);
  EXPECT_EQ(out[0], 0xC0);  // frame control: deauth subtype + management type
  EXPECT_EQ(out[1], 0x00);  // FC flags
  EXPECT_EQ(out[2], 0x00);  // duration
  EXPECT_EQ(out[3], 0x00);
  EXPECT_EQ(std::memcmp(out + 4, CLIENT, 6), 0);   // addr1 = destination client
  EXPECT_EQ(std::memcmp(out + 10, BSSID, 6), 0);   // addr2 = BSSID (spoofed source)
  EXPECT_EQ(std::memcmp(out + 16, BSSID, 6), 0);   // addr3 = BSSID
  EXPECT_EQ(out[22], 0x00);                        // sequence control
  EXPECT_EQ(out[23], 0x00);
  EXPECT_EQ(out[24], 0x07);  // reason code 7, little-endian
  EXPECT_EQ(out[25], 0x00);
}

TEST(FrameBuilder, DeauthBroadcastDestinationKicksEveryone) {
  uint8_t out[26] = {};
  const size_t n = wifiaudit::buildDeauth(out, sizeof(out), BSSID, BROADCAST, wifiaudit::REASON_CLASS3_FROM_NONASSOC);
  ASSERT_EQ(n, 26u);
  EXPECT_EQ(std::memcmp(out + 4, BROADCAST, 6), 0);  // addr1 broadcast
}

TEST(FrameBuilder, ReasonCodeIsLittleEndian) {
  uint8_t out[26] = {};
  wifiaudit::buildDeauth(out, sizeof(out), BSSID, CLIENT, 0x1234);
  EXPECT_EQ(out[24], 0x34);  // low byte first
  EXPECT_EQ(out[25], 0x12);
}

TEST(FrameBuilder, DisassocDiffersFromDeauthOnlyInSubtype) {
  uint8_t deauth[26] = {};
  uint8_t disassoc[26] = {};
  wifiaudit::buildDeauth(deauth, sizeof(deauth), BSSID, CLIENT, wifiaudit::REASON_DISASSOC_LEAVING);
  const size_t n = wifiaudit::buildDisassoc(disassoc, sizeof(disassoc), BSSID, CLIENT,
                                            wifiaudit::REASON_DISASSOC_LEAVING);
  ASSERT_EQ(n, 26u);
  EXPECT_EQ(disassoc[0], 0xA0);  // disassoc subtype
  EXPECT_EQ(disassoc[1], 0x00);
  // Everything after the frame-control bytes is byte-for-byte the deauth.
  EXPECT_EQ(std::memcmp(deauth + 2, disassoc + 2, 24), 0);
}

TEST(FrameBuilder, DeauthAndDisassocRejectTooSmallBuffer) {
  uint8_t out[25] = {};  // one short of the required 26
  EXPECT_EQ(wifiaudit::buildDeauth(out, sizeof(out), BSSID, CLIENT, 0), 0u);
  EXPECT_EQ(wifiaudit::buildDisassoc(out, sizeof(out), BSSID, CLIENT, 0), 0u);
}

TEST(FrameBuilder, BeaconLayoutAndInformationElements) {
  uint8_t out[128] = {};
  const char* ssid = "TestNet";
  const size_t ssidLen = std::strlen(ssid);  // 7
  const uint8_t channel = 6;
  const size_t n = wifiaudit::buildBeacon(out, sizeof(out), BSSID, ssid, ssidLen, channel);
  // 24 header + 12 fixed + (2+7) SSID + (2+8) rates + (2+1) DS = 58
  ASSERT_EQ(n, 24u + 12u + (2u + ssidLen) + 10u + 3u);

  EXPECT_EQ(out[0], 0x80);  // frame control: beacon
  EXPECT_EQ(out[1], 0x00);
  EXPECT_EQ(std::memcmp(out + 4, BROADCAST, 6), 0);  // addr1 broadcast
  EXPECT_EQ(std::memcmp(out + 10, BSSID, 6), 0);     // addr2 = BSSID
  EXPECT_EQ(std::memcmp(out + 16, BSSID, 6), 0);     // addr3 = BSSID

  // Fixed body: timestamp (8 zero bytes), interval 0x0064, capability 0x0401.
  for (int i = 24; i < 32; ++i) EXPECT_EQ(out[i], 0x00);
  EXPECT_EQ(out[32], 0x64);
  EXPECT_EQ(out[33], 0x00);
  EXPECT_EQ(out[34], 0x01);
  EXPECT_EQ(out[35], 0x04);

  // SSID IE at offset 36.
  EXPECT_EQ(out[36], 0x00);                            // element id
  EXPECT_EQ(out[37], static_cast<uint8_t>(ssidLen));   // length
  EXPECT_EQ(std::memcmp(out + 38, ssid, ssidLen), 0);  // the name

  // Supported Rates IE follows the SSID.
  size_t off = 38 + ssidLen;
  EXPECT_EQ(out[off], 0x01);      // element id
  EXPECT_EQ(out[off + 1], 0x08);  // length
  const uint8_t rates[8] = {0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c};
  EXPECT_EQ(std::memcmp(out + off + 2, rates, 8), 0);

  // DS Parameter Set IE follows the rates.
  off += 2 + 8;
  EXPECT_EQ(out[off], 0x03);          // element id
  EXPECT_EQ(out[off + 1], 0x01);      // length
  EXPECT_EQ(out[off + 2], channel);   // the channel
}

TEST(FrameBuilder, BeaconRejectsTooSmallBufferAndOversizeSsid) {
  uint8_t out[128] = {};
  // "TestNet" needs 58 bytes exactly.
  EXPECT_EQ(wifiaudit::buildBeacon(out, 57, BSSID, "TestNet", 7, 6), 0u);
  EXPECT_EQ(wifiaudit::buildBeacon(out, 58, BSSID, "TestNet", 7, 6), 58u);
  // An SSID longer than the 32-byte cap cannot be encoded.
  char big[40];
  std::memset(big, 'A', sizeof(big));
  EXPECT_EQ(wifiaudit::buildBeacon(out, sizeof(out), BSSID, big, 33, 6), 0u);
}

TEST(FrameBuilder, BeaconParsesBackThroughWifiFrame) {
  // Round-trip: a beacon we serialize must decode via the passive parser to the
  // same BSSID, SSID and channel -- proof the builder and parser share a layout.
  uint8_t out[128] = {};
  const size_t n = wifiaudit::buildBeacon(out, sizeof(out), BSSID, "RoundTrip", 9, 11);
  ASSERT_GT(n, 0u);
  wifiaudit::AccessPoint ap;
  ASSERT_TRUE(wifiaudit::parseBeacon(out, n, ap));
  EXPECT_EQ(std::memcmp(ap.bssid, BSSID, 6), 0);
  EXPECT_EQ(ap.ssid, "RoundTrip");
  EXPECT_EQ(ap.channel, 11);
  EXPECT_EQ(ap.encryption, wifiaudit::Encryption::Open);  // capability has no Privacy bit
}

TEST(FrameBuilder, BuiltFramesClassifyThroughManagementKind) {
  using wifiaudit::MgmtKind;
  uint8_t deauth[26] = {};
  wifiaudit::buildDeauth(deauth, sizeof(deauth), BSSID, CLIENT, 0);
  EXPECT_EQ(wifiaudit::managementKind(deauth, 26), MgmtKind::Deauth);

  uint8_t disassoc[26] = {};
  wifiaudit::buildDisassoc(disassoc, sizeof(disassoc), BSSID, CLIENT, 0);
  EXPECT_EQ(wifiaudit::managementKind(disassoc, 26), MgmtKind::Disassoc);

  uint8_t beacon[128] = {};
  const size_t bn = wifiaudit::buildBeacon(beacon, sizeof(beacon), BSSID, "X", 1, 1);
  ASSERT_GT(bn, 0u);
  EXPECT_EQ(wifiaudit::managementKind(beacon, bn), MgmtKind::Beacon);
}
