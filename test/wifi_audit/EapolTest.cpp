#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "offensive/Eapol.h"

namespace {

// Assemble an 802.1X EAPOL-Key frame (version 2, type 3) with the given Key Info
// and key data. Nonce byte 13.. is filled with `nonceFill`, MIC with `micFill`.
std::vector<uint8_t> eapolKey(const uint16_t keyInfo, const std::vector<uint8_t>& keyData, const uint8_t nonceFill = 0,
                              const uint8_t micFill = 0) {
  std::vector<uint8_t> body(95, 0);
  body[0] = 2;                                             // descriptor type = RSN
  body[1] = static_cast<uint8_t>((keyInfo >> 8) & 0xFF);  // key info, big-endian
  body[2] = static_cast<uint8_t>(keyInfo & 0xFF);
  for (int i = 0; i < 32; ++i) body[13 + i] = nonceFill;
  for (int i = 0; i < 16; ++i) body[77 + i] = micFill;
  body[93] = static_cast<uint8_t>((keyData.size() >> 8) & 0xFF);  // key data len, big-endian
  body[94] = static_cast<uint8_t>(keyData.size() & 0xFF);
  body.insert(body.end(), keyData.begin(), keyData.end());

  std::vector<uint8_t> frame = {2, 3};  // 802.1X version 2, type 3 (EAPOL-Key)
  frame.push_back(static_cast<uint8_t>((body.size() >> 8) & 0xFF));
  frame.push_back(static_cast<uint8_t>(body.size() & 0xFF));
  frame.insert(frame.end(), body.begin(), body.end());
  return frame;
}

std::vector<uint8_t> pmkidKde(const uint8_t fill) {
  std::vector<uint8_t> kde = {0xDD, 0x14, 0x00, 0x0F, 0xAC, 0x04};  // vendor, len 20, OUI 00:0F:AC, type 4
  for (int i = 0; i < 16; ++i) kde.push_back(fill);
  return kde;
}

// Key Info values (pairwise, descriptor version 2). ACK=0x80, MIC=0x100,
// SECURE=0x200, INSTALL=0x40, PAIRWISE=0x08.
constexpr uint16_t KI_M1 = 0x008A;  // ack
constexpr uint16_t KI_M2 = 0x010A;  // mic
constexpr uint16_t KI_M3 = 0x03CA;  // ack+mic+secure+install
constexpr uint16_t KI_M4 = 0x030A;  // mic+secure

}  // namespace

TEST(Eapol, ClassifiesFourMessages) {
  EXPECT_EQ(wifiaudit::classifyEapol(KI_M1), wifiaudit::EapolMessage::M1);
  EXPECT_EQ(wifiaudit::classifyEapol(KI_M2), wifiaudit::EapolMessage::M2);
  EXPECT_EQ(wifiaudit::classifyEapol(KI_M3), wifiaudit::EapolMessage::M3);
  EXPECT_EQ(wifiaudit::classifyEapol(KI_M4), wifiaudit::EapolMessage::M4);
}

TEST(Eapol, ParsesKeyFields) {
  const auto f = eapolKey(KI_M2, {}, 0xAA, 0xBB);
  wifiaudit::EapolKey k;
  ASSERT_TRUE(wifiaudit::parseEapolKey(f.data(), f.size(), k));
  EXPECT_EQ(k.descriptorType, 2);
  EXPECT_EQ(k.keyInfo, KI_M2);
  EXPECT_TRUE(k.hasMic);
  EXPECT_EQ(k.nonce[0], 0xAA);
  EXPECT_EQ(k.nonce[31], 0xAA);
  EXPECT_EQ(k.mic[0], 0xBB);
}

TEST(Eapol, RejectsTooShortOrWrongType) {
  wifiaudit::EapolKey k;
  const uint8_t tooShort[10] = {2, 3};
  EXPECT_FALSE(wifiaudit::parseEapolKey(tooShort, sizeof(tooShort), k));
  auto f = eapolKey(KI_M1, {});
  f[1] = 1;  // not EAPOL-Key type
  EXPECT_FALSE(wifiaudit::parseEapolKey(f.data(), f.size(), k));
}

TEST(Eapol, RejectsKeyDataOverrun) {
  auto f = eapolKey(KI_M1, pmkidKde(0x42));
  // Lie about key data length so it claims more than is present.
  f[4 + 93] = 0xFF;  // body offset 93 = key data len high byte, +4 for 802.1X header
  f[4 + 94] = 0xFF;
  wifiaudit::EapolKey k;
  EXPECT_FALSE(wifiaudit::parseEapolKey(f.data(), f.size(), k));
}

TEST(Eapol, ExtractsPmkidFromM1) {
  const auto f = eapolKey(KI_M1, pmkidKde(0x42));
  wifiaudit::EapolKey k;
  ASSERT_TRUE(wifiaudit::parseEapolKey(f.data(), f.size(), k));
  uint8_t pmkid[16];
  ASSERT_TRUE(wifiaudit::extractPmkid(k, pmkid));
  for (int i = 0; i < 16; ++i) EXPECT_EQ(pmkid[i], 0x42);
}

TEST(Eapol, AllZeroPmkidIsRejected) {
  const auto f = eapolKey(KI_M1, pmkidKde(0x00));  // AP offered no PMKID
  wifiaudit::EapolKey k;
  ASSERT_TRUE(wifiaudit::parseEapolKey(f.data(), f.size(), k));
  uint8_t pmkid[16];
  EXPECT_FALSE(wifiaudit::extractPmkid(k, pmkid));
}

TEST(Eapol, NoPmkidWhenKeyDataEmpty) {
  const auto f = eapolKey(KI_M2, {});
  wifiaudit::EapolKey k;
  ASSERT_TRUE(wifiaudit::parseEapolKey(f.data(), f.size(), k));
  uint8_t pmkid[16];
  EXPECT_FALSE(wifiaudit::extractPmkid(k, pmkid));
}

namespace {
// A cleartext 802.11 data frame carrying an EAPOL payload.
std::vector<uint8_t> dataFrameWithEapol(const uint8_t fc0, const uint8_t fc1, const std::vector<uint8_t>& eapol,
                                        bool qos) {
  std::vector<uint8_t> f = {fc0, fc1, 0x00, 0x00};
  const uint8_t a1[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
  const uint8_t a2[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
  const uint8_t a3[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
  f.insert(f.end(), a1, a1 + 6);
  f.insert(f.end(), a2, a2 + 6);
  f.insert(f.end(), a3, a3 + 6);
  f.push_back(0x00);  // seq ctrl
  f.push_back(0x00);
  if (qos) {
    f.push_back(0x00);
    f.push_back(0x00);
  }
  const uint8_t llc[8] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8E};
  f.insert(f.end(), llc, llc + 8);
  f.insert(f.end(), eapol.begin(), eapol.end());
  return f;
}
}  // namespace

TEST(Eapol, FindsEapolInToDsDataFrame) {
  const auto eapol = eapolKey(KI_M2, {});
  const auto f = dataFrameWithEapol(0x08, 0x01, eapol, false);  // data, toDS
  wifiaudit::EapolLocation loc;
  ASSERT_TRUE(wifiaudit::findEapolInDataFrame(f.data(), f.size(), loc));
  const uint8_t expectAp[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};   // a1 = BSSID
  const uint8_t expectSta[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};  // a2 = STA
  EXPECT_EQ(std::memcmp(loc.macAp, expectAp, 6), 0);
  EXPECT_EQ(std::memcmp(loc.macSta, expectSta, 6), 0);
  EXPECT_EQ(loc.eapolLen, eapol.size());
  EXPECT_EQ(loc.eapol[1], 3);  // 802.1X type EAPOL-Key
}

TEST(Eapol, FindsEapolInQosDataFrameWithOffset) {
  const auto eapol = eapolKey(KI_M2, {});
  const auto f = dataFrameWithEapol(0x88, 0x01, eapol, true);  // QoS data, toDS
  wifiaudit::EapolLocation loc;
  ASSERT_TRUE(wifiaudit::findEapolInDataFrame(f.data(), f.size(), loc));
  EXPECT_EQ(loc.eapolLen, eapol.size());
  EXPECT_EQ(loc.eapol[1], 3);
}

TEST(Eapol, SkipsEncryptedAndNonEapolFrames) {
  const auto eapol = eapolKey(KI_M2, {});
  wifiaudit::EapolLocation loc;
  auto enc = dataFrameWithEapol(0x08, 0x41, eapol, false);  // Protected bit set
  EXPECT_FALSE(wifiaudit::findEapolInDataFrame(enc.data(), enc.size(), loc));
  auto mgmt = dataFrameWithEapol(0x80, 0x00, eapol, false);  // beacon FC, not data
  EXPECT_FALSE(wifiaudit::findEapolInDataFrame(mgmt.data(), mgmt.size(), loc));
}

TEST(Eapol, Pmkid22000LineFormat) {
  uint8_t pmkid[16];
  for (int i = 0; i < 16; ++i) pmkid[i] = 0xAB;
  const uint8_t ap[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
  const uint8_t sta[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
  const char* essid = "test";
  const std::string line =
      wifiaudit::formatPmkid22000(pmkid, ap, sta, reinterpret_cast<const uint8_t*>(essid), 4);
  EXPECT_EQ(line, "WPA*01*abababababababababababababababab*001122334455*aabbccddeeff*74657374***");
}

TEST(Eapol, Eapol22000LineFormatShape) {
  uint8_t mic[16] = {};
  uint8_t anonce[32] = {};
  const uint8_t ap[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
  const uint8_t sta[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
  const uint8_t eapol[4] = {0x02, 0x03, 0x00, 0x00};
  const std::string line = wifiaudit::formatEapol22000(mic, ap, sta, nullptr, 0, anonce, eapol, 4, 0x02);
  EXPECT_EQ(line.rfind("WPA*02*", 0), 0u);         // starts with WPA*02*
  EXPECT_NE(line.find("*001122334455*"), std::string::npos);
  EXPECT_EQ(line.substr(line.size() - 3), "*02");  // ends with the message-pair byte
}
