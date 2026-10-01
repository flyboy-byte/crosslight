#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "wifiaudit/WifiFrame.h"

namespace {

using wifiaudit::Encryption;

// Builds an 802.11 beacon with a chosen capability field and a list of tagged
// information elements, so each test can assemble exactly the frame it needs.
class BeaconBuilder {
 public:
  explicit BeaconBuilder(const uint16_t capability = 0) {
    frame_ = {0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // FC, dur, addr1
    const uint8_t src[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    frame_.insert(frame_.end(), src, src + 6);          // addr2 (BSSID)
    for (int i = 0; i < 6; ++i) frame_.push_back(0xAB);  // addr3
    frame_.push_back(0x00);                              // seq ctrl
    frame_.push_back(0x00);
    for (int i = 0; i < 8; ++i) frame_.push_back(0x00);                      // timestamp
    frame_.push_back(0x64);                                                  // beacon interval
    frame_.push_back(0x00);
    frame_.push_back(static_cast<uint8_t>(capability & 0xFF));               // capability (LE)
    frame_.push_back(static_cast<uint8_t>((capability >> 8) & 0xFF));
  }

  BeaconBuilder& ie(const uint8_t tag, const std::vector<uint8_t>& value) {
    frame_.push_back(tag);
    frame_.push_back(static_cast<uint8_t>(value.size()));
    frame_.insert(frame_.end(), value.begin(), value.end());
    return *this;
  }

  BeaconBuilder& ssid(const std::string& s) { return ie(0x00, {s.begin(), s.end()}); }
  BeaconBuilder& dsChannel(const uint8_t ch) { return ie(0x03, {ch}); }

  const std::vector<uint8_t>& bytes() const { return frame_; }

 private:
  std::vector<uint8_t> frame_;
};

constexpr uint16_t CAP_PRIVACY = 0x0010;

// An RSN element body (what follows tag+len) with the given AKM suite types.
std::vector<uint8_t> rsnBody(const std::vector<uint8_t>& akmTypes) {
  std::vector<uint8_t> b = {0x01, 0x00};                    // version
  b.insert(b.end(), {0x00, 0x0F, 0xAC, 0x04});             // group cipher (CCMP)
  b.insert(b.end(), {0x01, 0x00});                         // pairwise count
  b.insert(b.end(), {0x00, 0x0F, 0xAC, 0x04});             // pairwise (CCMP)
  b.push_back(static_cast<uint8_t>(akmTypes.size()));      // AKM count (LE)
  b.push_back(0x00);
  for (const uint8_t t : akmTypes) b.insert(b.end(), {0x00, 0x0F, 0xAC, t});
  b.insert(b.end(), {0x00, 0x00});                         // RSN capabilities
  return b;
}

// A WPA v1 vendor IE body (OUI 00:50:F2, type 1).
std::vector<uint8_t> wpaBody() {
  std::vector<uint8_t> b = {0x00, 0x50, 0xF2, 0x01, 0x01, 0x00};  // OUI+type, version
  b.insert(b.end(), {0x00, 0x50, 0xF2, 0x02});                    // group (TKIP)
  b.insert(b.end(), {0x01, 0x00});                                // pairwise count
  b.insert(b.end(), {0x00, 0x50, 0xF2, 0x02});                    // pairwise (TKIP)
  b.insert(b.end(), {0x01, 0x00});                                // AKM count
  b.insert(b.end(), {0x00, 0x50, 0xF2, 0x02});                    // AKM (PSK)
  return b;
}

wifiaudit::AccessPoint parse(const std::vector<uint8_t>& f) {
  wifiaudit::AccessPoint ap;
  EXPECT_TRUE(wifiaudit::parseBeacon(f.data(), f.size(), ap));
  return ap;
}

}  // namespace

TEST(WifiFrame, ParsesBssidSsidAndChannel) {
  const auto f = BeaconBuilder().ssid("HomeNet").dsChannel(6).bytes();
  const auto ap = parse(f);
  const uint8_t expect[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
  EXPECT_EQ(std::memcmp(ap.bssid, expect, 6), 0);
  EXPECT_EQ(ap.ssid, "HomeNet");
  EXPECT_EQ(ap.channel, 6);
}

TEST(WifiFrame, HiddenSsidIsEmptyNotFailure) {
  const auto f = BeaconBuilder().ssid("").dsChannel(1).bytes();
  const auto ap = parse(f);
  EXPECT_TRUE(ap.ssid.empty());
}

TEST(WifiFrame, NoDsParamLeavesChannelZero) {
  const auto f = BeaconBuilder().ssid("NoChan").bytes();
  const auto ap = parse(f);
  EXPECT_EQ(ap.channel, 0);  // caller falls back to the heard-on channel
}

TEST(WifiFrame, OpenNetworkNoPrivacyNoIe) {
  const auto f = BeaconBuilder(0).ssid("Cafe").dsChannel(1).bytes();
  EXPECT_EQ(parse(f).encryption, Encryption::Open);
}

TEST(WifiFrame, WepIsPrivacyBitWithoutRsnOrWpa) {
  const auto f = BeaconBuilder(CAP_PRIVACY).ssid("OldRouter").dsChannel(11).bytes();
  EXPECT_EQ(parse(f).encryption, Encryption::Wep);
}

TEST(WifiFrame, WpaV1VendorIe) {
  const auto f = BeaconBuilder(CAP_PRIVACY).ssid("Legacy").ie(0xDD, wpaBody()).bytes();
  EXPECT_EQ(parse(f).encryption, Encryption::Wpa);
}

TEST(WifiFrame, Wpa2WhenRsnHasPskOnly) {
  const auto f = BeaconBuilder(CAP_PRIVACY).ssid("Secure").ie(0x30, rsnBody({2})).bytes();
  EXPECT_EQ(parse(f).encryption, Encryption::Wpa2);
}

TEST(WifiFrame, Wpa3WhenRsnHasSae) {
  const auto f = BeaconBuilder(CAP_PRIVACY).ssid("Modern").ie(0x30, rsnBody({8})).bytes();
  EXPECT_EQ(parse(f).encryption, Encryption::Wpa3);
}

TEST(WifiFrame, TransitionWhenRsnHasPskAndSae) {
  const auto f = BeaconBuilder(CAP_PRIVACY).ssid("Both").ie(0x30, rsnBody({2, 8})).bytes();
  EXPECT_EQ(parse(f).encryption, Encryption::Wpa2Wpa3);
}

TEST(WifiFrame, RsnOutranksWpaVendorIe) {
  // An AP advertising both an RSN and a WPA v1 IE is really WPA2+, not WPA.
  const auto f = BeaconBuilder(CAP_PRIVACY).ssid("Mixed").ie(0x30, rsnBody({2})).ie(0xDD, wpaBody()).bytes();
  EXPECT_EQ(parse(f).encryption, Encryption::Wpa2);
}

TEST(WifiFrame, VendorIeThatIsNotWpaDoesNotCountAsWpa) {
  // A non-WPA vendor IE (e.g. WPS, OUI 00:50:F2 type 4) must not read as WPA.
  const auto f = BeaconBuilder(CAP_PRIVACY).ssid("WithWps").ie(0xDD, {0x00, 0x50, 0xF2, 0x04, 0x10, 0x4A}).bytes();
  EXPECT_EQ(parse(f).encryption, Encryption::Wep);  // privacy set, no real RSN/WPA
}

TEST(WifiFrame, TruncatedRsnIeDoesNotOverreadAndReadsAsWpa2) {
  // An RSN IE whose declared AKM list runs past the element: the walk aborts
  // mid-RSN, so it reads as "RSN present, scheme unknown" -> WPA2, never crashes.
  auto body = rsnBody({8});       // would be WPA3 if fully parsed
  body.resize(6);                 // chop after the group cipher
  const auto f = BeaconBuilder(CAP_PRIVACY).ssid("Chopped").ie(0x30, body).bytes();
  EXPECT_EQ(parse(f).encryption, Encryption::Wpa2);
}

TEST(WifiFrame, ParsesProbeResponse) {
  auto f = BeaconBuilder(CAP_PRIVACY).ssid("PR").ie(0x30, rsnBody({8})).bytes();
  f[0] = 0x50;  // flip subtype beacon -> probe response
  const auto ap = parse(f);
  EXPECT_EQ(ap.ssid, "PR");
  EXPECT_EQ(ap.encryption, Encryption::Wpa3);
}

TEST(WifiFrame, RejectsNonBeaconSubtypesAndShortFrames) {
  wifiaudit::AccessPoint ap;
  auto f = BeaconBuilder().ssid("X").bytes();
  f[0] = 0x40;  // probe request: a client frame, not an AP advert
  EXPECT_FALSE(wifiaudit::parseBeacon(f.data(), f.size(), ap));
  f[0] = 0x08;  // data frame
  EXPECT_FALSE(wifiaudit::parseBeacon(f.data(), f.size(), ap));
  const uint8_t tooShort[20] = {0x80};
  EXPECT_FALSE(wifiaudit::parseBeacon(tooShort, sizeof(tooShort), ap));
}

TEST(WifiFrame, EncryptionLabels) {
  EXPECT_STREQ(wifiaudit::encryptionLabel(Encryption::Open), "Open");
  EXPECT_STREQ(wifiaudit::encryptionLabel(Encryption::Wep), "WEP");
  EXPECT_STREQ(wifiaudit::encryptionLabel(Encryption::Wpa), "WPA");
  EXPECT_STREQ(wifiaudit::encryptionLabel(Encryption::Wpa2), "WPA2");
  EXPECT_STREQ(wifiaudit::encryptionLabel(Encryption::Wpa3), "WPA3");
  EXPECT_STREQ(wifiaudit::encryptionLabel(Encryption::Wpa2Wpa3), "WPA2/WPA3");
}

TEST(WifiFrame, ManagementKindClassifiesSubtypes) {
  using wifiaudit::MgmtKind;
  const uint8_t beacon[1] = {0x80};
  const uint8_t probeResp[1] = {0x50};
  const uint8_t deauth[1] = {0xC0};
  const uint8_t disassoc[1] = {0xA0};
  const uint8_t probeReq[1] = {0x40};  // management, but not one we track
  const uint8_t dataFrame[1] = {0x08};
  EXPECT_EQ(wifiaudit::managementKind(beacon, 1), MgmtKind::Beacon);
  EXPECT_EQ(wifiaudit::managementKind(probeResp, 1), MgmtKind::ProbeResponse);
  EXPECT_EQ(wifiaudit::managementKind(deauth, 1), MgmtKind::Deauth);
  EXPECT_EQ(wifiaudit::managementKind(disassoc, 1), MgmtKind::Disassoc);
  EXPECT_EQ(wifiaudit::managementKind(probeReq, 1), MgmtKind::Other);
  EXPECT_EQ(wifiaudit::managementKind(dataFrame, 1), MgmtKind::Other);
  EXPECT_EQ(wifiaudit::managementKind(nullptr, 0), MgmtKind::Other);
}
