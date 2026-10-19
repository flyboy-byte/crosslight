#include <gtest/gtest.h>

#include <string>

#include "libre/LibreHistory.h"

namespace {

const char kBody[] =
    "{\"readings\":["
    "{\"value\":110,\"unit\":\"mg/dL\",\"trend\":\"steady\",\"status\":\"OK\",\"source_time\":\"2026-10-13T03:00:00.000Z\","
    "\"observed_time\":\"2026-10-13T03:00:05.000Z\",\"age_s\":600,\"method\":\"LIBRELINKUP_FOLLOWER\"},"
    "{\"value\":null,\"unit\":\"mg/dL\",\"trend\":null,\"status\":\"HIGH\",\"source_time\":\"x\",\"age_s\":540,\"method\":\"m\"},"
    "{\"value\":226,\"unit\":\"mg/dL\",\"trend\":\"rising\",\"status\":\"OK\",\"source_time\":\"x\",\"age_s\":10,\"method\":\"m\"}"
    "]}";

}  // namespace

TEST(LibreHistory, WholeBodyKeepsNumericReadingsOldestFirst) {
  std::vector<libre::HistoryPoint> pts;
  libre::HistoryScanner s(pts);
  s.feed(kBody, sizeof(kBody) - 1);
  ASSERT_EQ(pts.size(), 2u);
  EXPECT_EQ(pts[0].value, 110);
  EXPECT_EQ(pts[0].ageS, 600);
  EXPECT_EQ(pts[1].value, 226);
  EXPECT_EQ(pts[1].ageS, 10);
}

TEST(LibreHistory, SameResultForEverySplitSize) {
  for (size_t chunk = 1; chunk <= 40; ++chunk) {
    std::vector<libre::HistoryPoint> pts;
    libre::HistoryScanner s(pts);
    const std::string body(kBody);
    for (size_t i = 0; i < body.size(); i += chunk) {
      const std::string piece = body.substr(i, chunk);
      s.feed(piece.data(), piece.size());
    }
    ASSERT_EQ(pts.size(), 2u) << "chunk=" << chunk;
    EXPECT_EQ(pts[0].value, 110) << "chunk=" << chunk;
    EXPECT_EQ(pts[1].value, 226) << "chunk=" << chunk;
    EXPECT_EQ(pts[1].ageS, 10) << "chunk=" << chunk;
  }
}

TEST(LibreHistory, EmptyAndGarbageGiveNoPoints) {
  std::vector<libre::HistoryPoint> pts;
  libre::HistoryScanner s(pts);
  s.feed("{\"readings\":[]}", 15);
  s.feed("garbage", 7);
  EXPECT_TRUE(pts.empty());
}

TEST(LibreHistory, CapsAtMaxPoints) {
  std::vector<libre::HistoryPoint> pts;
  libre::HistoryScanner s(pts, 3);
  const std::string one = "{\"value\":100,\"age_s\":1},";
  for (int i = 0; i < 10; ++i) s.feed(one.data(), one.size());
  EXPECT_EQ(pts.size(), 3u);
}
