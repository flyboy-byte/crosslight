#include <gtest/gtest.h>

#include <array>

#include "libre/LibreBlePacket.h"

namespace {

std::array<uint8_t, libre::kBlePacketSize> packet(const uint16_t valueMgdl, const uint8_t trendWire,
                                                   const uint8_t flags, const uint32_t readingTime) {
  return {
      static_cast<uint8_t>(valueMgdl & 0xFF),
      static_cast<uint8_t>(valueMgdl >> 8),
      trendWire,
      flags,
      static_cast<uint8_t>(readingTime & 0xFF),
      static_cast<uint8_t>((readingTime >> 8) & 0xFF),
      static_cast<uint8_t>((readingTime >> 16) & 0xFF),
      static_cast<uint8_t>((readingTime >> 24) & 0xFF),
  };
}

}  // namespace

// Vector from extraction's handoff message (2026-10-13): reading 226 mg/dL,
// steady, 2026-10-13T03:10:23Z -> E2 00 03 00 1F A1 CD 6A. Independently
// recomputed (date -u -d ... +%s = 1791861023) rather than trusted as given.
TEST(DecodeBlePacket, MatchesExtractionTestVector) {
  const auto bytes = packet(226, /*steady*/ 3, /*flags*/ 0, /*readingTime*/ 1791861023u);
  ASSERT_EQ((bytes), (std::array<uint8_t, 8>{0xE2, 0x00, 0x03, 0x00, 0x1F, 0xA1, 0xCD, 0x6A}));

  libre::BlePacket out;
  ASSERT_TRUE(libre::decodeBlePacket(bytes.data(), bytes.size(), out));
  EXPECT_EQ(out.valueMgdl, 226);
  EXPECT_EQ(out.trend, libre::Trend::Steady);
  EXPECT_FALSE(out.high);
  EXPECT_FALSE(out.low);
  EXPECT_FALSE(out.unavailable);
  EXPECT_EQ(out.readingTimeUnix, 1791861023u);
}

TEST(DecodeBlePacket, RejectsNullptr) {
  libre::BlePacket out;
  EXPECT_FALSE(libre::decodeBlePacket(nullptr, 0, out));
  EXPECT_FALSE(libre::decodeBlePacket(nullptr, libre::kBlePacketSize, out));
}

TEST(DecodeBlePacket, RejectsWrongLength) {
  const auto bytes = packet(100, 3, 0, 1000);
  libre::BlePacket out;
  EXPECT_FALSE(libre::decodeBlePacket(bytes.data(), 7, out));
  EXPECT_FALSE(libre::decodeBlePacket(bytes.data(), 9, out));
  EXPECT_FALSE(libre::decodeBlePacket(bytes.data(), 0, out));
}

TEST(DecodeBlePacket, OutUntouchedOnRejection) {
  libre::BlePacket out;
  out.valueMgdl = 55;
  const auto bytes = packet(100, 3, 0, 1000);
  EXPECT_FALSE(libre::decodeBlePacket(bytes.data(), 7, out));
  EXPECT_EQ(out.valueMgdl, 55);  // untouched, not partially overwritten
}

TEST(DecodeBlePacket, TrendWireMapping) {
  const struct {
    uint8_t wire;
    libre::Trend expected;
  } cases[] = {
      {0, libre::Trend::Unknown},     {1, libre::Trend::FallingFast}, {2, libre::Trend::Falling},
      {3, libre::Trend::Steady},      {4, libre::Trend::Rising},      {5, libre::Trend::RisingFast},
      {6, libre::Trend::Unknown},     {255, libre::Trend::Unknown},
  };
  for (const auto& c : cases) {
    const auto bytes = packet(100, c.wire, 0, 1000);
    libre::BlePacket out;
    ASSERT_TRUE(libre::decodeBlePacket(bytes.data(), bytes.size(), out));
    EXPECT_EQ(out.trend, c.expected) << "wire=" << static_cast<int>(c.wire);
  }
}

TEST(DecodeBlePacket, FlagBitsAreIndependent) {
  {
    const auto bytes = packet(100, 3, 0x01, 1000);  // HIGH
    libre::BlePacket out;
    ASSERT_TRUE(libre::decodeBlePacket(bytes.data(), bytes.size(), out));
    EXPECT_TRUE(out.high);
    EXPECT_FALSE(out.low);
    EXPECT_FALSE(out.unavailable);
  }
  {
    const auto bytes = packet(100, 3, 0x02, 1000);  // LOW
    libre::BlePacket out;
    ASSERT_TRUE(libre::decodeBlePacket(bytes.data(), bytes.size(), out));
    EXPECT_FALSE(out.high);
    EXPECT_TRUE(out.low);
    EXPECT_FALSE(out.unavailable);
  }
  {
    const auto bytes = packet(100, 3, 0x04, 1000);  // UNAVAILABLE
    libre::BlePacket out;
    ASSERT_TRUE(libre::decodeBlePacket(bytes.data(), bytes.size(), out));
    EXPECT_FALSE(out.high);
    EXPECT_FALSE(out.low);
    EXPECT_TRUE(out.unavailable);
  }
}

TEST(DecodeBlePacket, NoValueSentinel) {
  const auto bytes = packet(libre::kBleNoValueMgdl, 0, 0x04, 1000);
  libre::BlePacket out;
  ASSERT_TRUE(libre::decodeBlePacket(bytes.data(), bytes.size(), out));
  EXPECT_EQ(out.valueMgdl, libre::kBleNoValueMgdl);
}

// ---- bleReadingFromPacket -------------------------------------------------
// Pure function of the packet now: status from flags/sentinel only. Freshness
// is judged by receipt time (millis uptime) in GlucoseMonitorActivity, not
// here, so there are no clock-based cases left to test at this layer.

TEST(BleReadingFromPacket, PlainReadingIsOk) {
  libre::BlePacket p;
  p.valueMgdl = 143;
  p.trend = libre::Trend::Falling;
  const libre::Reading r = libre::bleReadingFromPacket(p);
  EXPECT_EQ(r.status, libre::ReadingStatus::Ok);
  EXPECT_EQ(r.value, 143);
  EXPECT_EQ(r.trend, libre::Trend::Falling);
}

TEST(BleReadingFromPacket, UnavailableFlagWinsOverValue) {
  libre::BlePacket p;
  p.valueMgdl = 100;
  p.unavailable = true;
  const libre::Reading r = libre::bleReadingFromPacket(p);
  EXPECT_EQ(r.status, libre::ReadingStatus::Unavailable);
}

TEST(BleReadingFromPacket, NoValueSentinelIsUnavailableEvenWithoutFlag) {
  libre::BlePacket p;
  p.valueMgdl = libre::kBleNoValueMgdl;
  const libre::Reading r = libre::bleReadingFromPacket(p);
  EXPECT_EQ(r.status, libre::ReadingStatus::Unavailable);
}

TEST(BleReadingFromPacket, HighFlagSetsHighStatusAndKeepsNumber) {
  libre::BlePacket p;
  p.valueMgdl = 260;
  p.high = true;
  const libre::Reading r = libre::bleReadingFromPacket(p);
  EXPECT_EQ(r.status, libre::ReadingStatus::High);
  EXPECT_EQ(r.value, 260);
}

TEST(BleReadingFromPacket, LowFlagSetsLowStatus) {
  libre::BlePacket p;
  p.valueMgdl = 55;
  p.low = true;
  const libre::Reading r = libre::bleReadingFromPacket(p);
  EXPECT_EQ(r.status, libre::ReadingStatus::Low);
  EXPECT_EQ(r.value, 55);
}
