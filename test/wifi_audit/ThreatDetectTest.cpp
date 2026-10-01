#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "wifiaudit/ThreatDetect.h"

namespace {

using wifiaudit::Encryption;

wifiaudit::ApRecord ap(const std::string& ssid, const uint8_t lastByte, const Encryption enc) {
  wifiaudit::ApRecord r;
  r.ap.ssid = ssid;
  const uint8_t bssid[6] = {0xAA, 0xBB, 0xCC, 0x00, 0x00, lastByte};
  std::memcpy(r.ap.bssid, bssid, 6);
  r.ap.encryption = enc;
  return r;
}

}  // namespace

TEST(ThreatDetect, NoEvilTwinWhenEachSsidHasOneBssid) {
  const std::vector<wifiaudit::ApRecord> aps = {ap("Home", 0x01, Encryption::Wpa2),
                                                ap("Office", 0x02, Encryption::Wpa2)};
  EXPECT_TRUE(wifiaudit::findEvilTwins(aps).empty());
}

TEST(ThreatDetect, FlagsSsidSeenFromTwoBssids) {
  const std::vector<wifiaudit::ApRecord> aps = {ap("Home", 0x01, Encryption::Wpa2),
                                                ap("Home", 0x02, Encryption::Wpa2)};
  const auto alerts = wifiaudit::findEvilTwins(aps);
  ASSERT_EQ(alerts.size(), 1u);
  EXPECT_EQ(alerts[0].ssid, "Home");
  EXPECT_EQ(alerts[0].bssidCount, 2u);
  EXPECT_FALSE(alerts[0].encryptionMismatch);
}

TEST(ThreatDetect, EncryptionMismatchIsTheStrongerTell) {
  // Same SSID, but one copy is Open and the other WPA2 -- classic evil twin.
  const std::vector<wifiaudit::ApRecord> aps = {ap("Home", 0x01, Encryption::Wpa2),
                                                ap("Home", 0x02, Encryption::Open)};
  const auto alerts = wifiaudit::findEvilTwins(aps);
  ASSERT_EQ(alerts.size(), 1u);
  EXPECT_TRUE(alerts[0].encryptionMismatch);
}

TEST(ThreatDetect, DuplicateBssidForSameSsidIsNotTwoAps) {
  // The same AP heard repeatedly must not look like a twin of itself.
  const std::vector<wifiaudit::ApRecord> aps = {ap("Home", 0x01, Encryption::Wpa2),
                                                ap("Home", 0x01, Encryption::Wpa2)};
  EXPECT_TRUE(wifiaudit::findEvilTwins(aps).empty());
}

TEST(ThreatDetect, HiddenSsidsAreIgnored) {
  const std::vector<wifiaudit::ApRecord> aps = {ap("", 0x01, Encryption::Wpa2), ap("", 0x02, Encryption::Wpa2)};
  EXPECT_TRUE(wifiaudit::findEvilTwins(aps).empty());
}

TEST(ThreatDetect, DeauthCountCountsOnlyWithinWindow) {
  // Events at 1000, 5000, 9000, 9500 ms; window 5000 ending at now=10000 -> keep
  // everything >= 5000: that is 5000, 9000, 9500 = 3.
  const std::vector<uint32_t> times = {1000, 5000, 9000, 9500};
  EXPECT_EQ(wifiaudit::deauthCountInWindow(times, 10000, 5000), 3u);
}

TEST(ThreatDetect, DeauthCountExcludesFutureTimestamps) {
  const std::vector<uint32_t> times = {9000, 11000};  // 11000 is after now
  EXPECT_EQ(wifiaudit::deauthCountInWindow(times, 10000, 5000), 1u);
}

TEST(ThreatDetect, DeauthCountHandlesEarlyBootWindowUnderflow) {
  // now < windowMs must not underflow the cutoff; everything up to now counts.
  const std::vector<uint32_t> times = {100, 200, 300};
  EXPECT_EQ(wifiaudit::deauthCountInWindow(times, 500, 5000), 3u);
}

TEST(ThreatDetect, FloodActiveAtOrAboveThreshold) {
  std::vector<uint32_t> times;
  for (uint32_t i = 0; i < 30; ++i) times.push_back(1000 + i);  // 30 within window
  EXPECT_TRUE(wifiaudit::deauthFloodActive(times, 2000, 5000, 30));
  EXPECT_FALSE(wifiaudit::deauthFloodActive(times, 2000, 5000, 31));
}

TEST(ThreatDetect, OccasionalDeauthIsNotAFlood) {
  const std::vector<uint32_t> times = {1000, 4000, 8000};
  EXPECT_FALSE(wifiaudit::deauthFloodActive(times, 10000, 5000, 20));
}
