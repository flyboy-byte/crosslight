#include <gtest/gtest.h>

#include "bible/BiblePsalterReadings.h"

using bible_psalter::DayReadings;
using bible_psalter::readingsForDay;

// Pinned against the Church of England's own published Psalter page order and
// the independently well-documented fact that Psalm 119 splits across days
// 24-26 -- see BiblePsalterReadings.h's header comment for the sourcing.

TEST(BiblePsalterReadings, Day1StartsAtPsalm1) {
  DayReadings d;
  ASSERT_TRUE(readingsForDay(1, d));
  EXPECT_EQ(d.morning.label(), "Psalms 1-5");
  EXPECT_EQ(d.evening.label(), "Psalms 6-8");
}

TEST(BiblePsalterReadings, Day30EndsAtPsalm150) {
  DayReadings d;
  ASSERT_TRUE(readingsForDay(30, d));
  EXPECT_EQ(d.morning.label(), "Psalms 144-146");
  EXPECT_EQ(d.evening.label(), "Psalms 147-150");
}

// Single-psalm days render without a dash.
TEST(BiblePsalterReadings, SinglePsalmDayHasNoDash) {
  DayReadings d;
  ASSERT_TRUE(readingsForDay(13, d));
  EXPECT_EQ(d.morning.label(), "Psalm 68");
  EXPECT_EQ(d.evening.label(), "Psalms 69-70");
}

// Psalm 119 is split into five verse segments across days 24-26 -- too long
// for one sitting. This is the cross-check fact noted in the header comment.
TEST(BiblePsalterReadings, Psalm119SpansThreeDays) {
  DayReadings d24, d25, d26;
  ASSERT_TRUE(readingsForDay(24, d24));
  ASSERT_TRUE(readingsForDay(25, d25));
  ASSERT_TRUE(readingsForDay(26, d26));
  EXPECT_EQ(d24.morning.label(), "Psalms 116-118");
  EXPECT_EQ(d24.evening.label(), "Psalm 119:1-32");
  EXPECT_EQ(d25.morning.label(), "Psalm 119:33-72");
  EXPECT_EQ(d25.evening.label(), "Psalm 119:73-104");
  EXPECT_EQ(d26.morning.label(), "Psalm 119:105-144");
  EXPECT_EQ(d26.evening.label(), "Psalm 119:145-176");
}

// Day 31 repeats Day 30 -- the Prayer Book's own rule for 31-day months.
TEST(BiblePsalterReadings, Day31RepeatsDay30) {
  DayReadings d30, d31;
  ASSERT_TRUE(readingsForDay(30, d30));
  ASSERT_TRUE(readingsForDay(31, d31));
  EXPECT_EQ(d31.morning.label(), d30.morning.label());
  EXPECT_EQ(d31.evening.label(), d30.evening.label());
}

TEST(BiblePsalterReadings, RejectsOutOfRangeDay) {
  DayReadings d;
  EXPECT_FALSE(readingsForDay(0, d));
  EXPECT_FALSE(readingsForDay(32, d));
  EXPECT_FALSE(readingsForDay(-1, d));
}
