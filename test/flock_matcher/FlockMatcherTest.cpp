#include <gtest/gtest.h>

#include "flock/FlockSignature.h"

namespace {

flock::Observation obs(const uint8_t a, const uint8_t b, const uint8_t c, const std::string& ssid = "") {
  flock::Observation o;
  o.mac[0] = a;
  o.mac[1] = b;
  o.mac[2] = c;
  o.mac[3] = 0x11;
  o.mac[4] = 0x22;
  o.mac[5] = 0x33;
  o.ssid = ssid;
  return o;
}

flock::Signature sig(const std::string& name, const std::vector<uint32_t>& ouis,
                     const std::vector<std::string>& ssids) {
  return flock::Signature{name, "test", ouis, ssids};
}

}  // namespace

TEST(FlockMatcher, OuiExtraction) {
  const uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0x01, 0x02, 0x03};
  EXPECT_EQ(flock::ouiOf(mac), 0xAABBCCu);
}

TEST(FlockMatcher, MatchesByOui) {
  const std::vector<flock::Signature> sigs = {sig("Cam", {0xAABBCC}, {})};
  EXPECT_EQ(flock::match(sigs, obs(0xAA, 0xBB, 0xCC)), 0);
  EXPECT_EQ(flock::match(sigs, obs(0xAA, 0xBB, 0xCD)), -1);
}

TEST(FlockMatcher, MatchesBySsidSubstringCaseInsensitive) {
  const std::vector<flock::Signature> sigs = {sig("Cam", {}, {"flock"})};
  EXPECT_EQ(flock::match(sigs, obs(0x00, 0x00, 0x00, "FLOCK-1234")), 0);
  EXPECT_EQ(flock::match(sigs, obs(0x00, 0x00, 0x00, "myFlockNet")), 0);
  EXPECT_EQ(flock::match(sigs, obs(0x00, 0x00, 0x00, "HomeWiFi")), -1);
}

TEST(FlockMatcher, EitherCriterionMatches) {
  // OUI and SSID both listed: a device matching only one still flags, because
  // vendors randomize one field but not the other.
  const std::vector<flock::Signature> sigs = {sig("Cam", {0xAABBCC}, {"flock"})};
  EXPECT_EQ(flock::match(sigs, obs(0xAA, 0xBB, 0xCC, "randomized-ssid")), 0);
  EXPECT_EQ(flock::match(sigs, obs(0xDE, 0xAD, 0xBE, "flock-cam")), 0);
}

TEST(FlockMatcher, EmptySignatureNeverMatches) {
  // A blank entry must not flag every device on the air.
  const std::vector<flock::Signature> sigs = {sig("Blank", {}, {})};
  EXPECT_EQ(flock::match(sigs, obs(0xAA, 0xBB, 0xCC, "anything")), -1);
}

TEST(FlockMatcher, EmptySsidObservationDoesNotMatchSsidRule) {
  const std::vector<flock::Signature> sigs = {sig("Cam", {}, {"flock"})};
  EXPECT_EQ(flock::match(sigs, obs(0x00, 0x00, 0x00, "")), -1);
}

TEST(FlockMatcher, ReturnsFirstMatchingIndex) {
  const std::vector<flock::Signature> sigs = {sig("A", {0x111111}, {}), sig("B", {0xAABBCC}, {}),
                                              sig("C", {0xAABBCC}, {})};
  EXPECT_EQ(flock::match(sigs, obs(0xAA, 0xBB, 0xCC)), 1);
}

TEST(FlockMatcher, NoSignaturesNoMatch) {
  const std::vector<flock::Signature> sigs;
  EXPECT_EQ(flock::match(sigs, obs(0xAA, 0xBB, 0xCC, "flock")), -1);
}

namespace {
// Build a beacon frame: FC=0x80,0x00, duration, addr1(bcast), addr2(src),
// addr3, seq, then 12-byte fixed body, then an SSID element.
std::vector<uint8_t> beacon(const uint8_t src[6], const std::string& ssid) {
  std::vector<uint8_t> f = {0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  f.insert(f.end(), src, src + 6);                                      // addr2
  for (int i = 0; i < 6; ++i) f.push_back(0xAB);                        // addr3
  f.push_back(0x00);                                                    // seq ctrl
  f.push_back(0x00);
  for (int i = 0; i < 12; ++i) f.push_back(0x00);                       // fixed body
  f.push_back(0x00);                                                    // SSID tag
  f.push_back(static_cast<uint8_t>(ssid.size()));                       // length
  f.insert(f.end(), ssid.begin(), ssid.end());
  return f;
}
}  // namespace

TEST(FlockFrame, ParsesBeaconMacAndSsid) {
  const uint8_t src[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
  const auto f = beacon(src, "FlockCam");
  flock::Observation o;
  ASSERT_TRUE(flock::parseManagementFrame(f.data(), f.size(), o));
  EXPECT_EQ(flock::ouiOf(o.mac), 0xDEADBEu);
  EXPECT_EQ(o.ssid, "FlockCam");
}

TEST(FlockFrame, ProbeRequestSsidRightAfterHeader) {
  // Probe request: subtype 0x40, no fixed body, SSID element at offset 24.
  std::vector<uint8_t> f = {0x40, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  const uint8_t src[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  f.insert(f.end(), src, src + 6);
  for (int i = 0; i < 6; ++i) f.push_back(0xAB);
  f.push_back(0x00);
  f.push_back(0x00);
  f.push_back(0x00);            // SSID tag
  f.push_back(0x03);            // length 3
  f.insert(f.end(), {'a', 'b', 'c'});
  flock::Observation o;
  ASSERT_TRUE(flock::parseManagementFrame(f.data(), f.size(), o));
  EXPECT_EQ(o.ssid, "abc");
}

TEST(FlockFrame, HiddenSsidIsEmptyNotFailure) {
  const uint8_t src[6] = {0x01, 0x02, 0x03, 0, 0, 0};
  const auto f = beacon(src, "");  // zero-length SSID element
  flock::Observation o;
  ASSERT_TRUE(flock::parseManagementFrame(f.data(), f.size(), o));
  EXPECT_TRUE(o.ssid.empty());
}

TEST(FlockFrame, RejectsNonManagementAndShortFrames) {
  flock::Observation o;
  const uint8_t dataFrame[24] = {0x08};  // type=data
  EXPECT_FALSE(flock::parseManagementFrame(dataFrame, sizeof(dataFrame), o));
  const uint8_t tooShort[10] = {0x80};
  EXPECT_FALSE(flock::parseManagementFrame(tooShort, sizeof(tooShort), o));
}

TEST(FlockFrame, TruncatedSsidElementDoesNotOverread) {
  const uint8_t src[6] = {0xAA, 0xBB, 0xCC, 0, 0, 0};
  auto f = beacon(src, "Hello");
  f.resize(f.size() - 2);  // chop 2 SSID bytes: element claims 5, only 3 present
  flock::Observation o;
  ASSERT_TRUE(flock::parseManagementFrame(f.data(), f.size(), o));
  // The claimed length overruns the buffer, so the element is skipped -> empty.
  EXPECT_TRUE(o.ssid.empty());
}
