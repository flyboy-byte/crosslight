#include <gtest/gtest.h>

#include "bible/HistoricalCalendar.h"

// The computus is pure arithmetic where a subtle off-by-one would silently print
// a wrong Easter, so every formula is pinned to known-answer values: modern
// Easters anyone can cross-check, Orthodox (Julian) Easters via the +13-day
// offset, and the 1611 almanac row itself.

using historical_calendar::Date;
using historical_calendar::goldenNumber;
using historical_calendar::gregorianDominicalLetter;
using historical_calendar::gregorianEaster;
using historical_calendar::julianEaster;
using historical_calendar::julianEpact;

namespace {
::testing::AssertionResult DateIs(const Date& d, int month, int day) {
  if (d.month == month && d.day == day) return ::testing::AssertionSuccess();
  return ::testing::AssertionFailure() << "got " << d.month << "/" << d.day << ", expected " << month << "/" << day;
}
}  // namespace

// Golden Number = (year mod 19) + 1. The 1611 value is the one a research handoff
// got wrong (it wrote 13); the correct value is 16, and 13 belongs to 1608.
TEST(HistoricalCalendarGoldenNumber, KnownValues) {
  EXPECT_EQ(goldenNumber(1611), 16);
  EXPECT_EQ(goldenNumber(1608), 13);
  EXPECT_EQ(goldenNumber(2000), 6);
  EXPECT_EQ(goldenNumber(2024), 11);
}

TEST(HistoricalCalendarGoldenNumber, WrapsAtCycleBoundary) {
  EXPECT_EQ(goldenNumber(1596), 1);  // 1596 = 19*84, start of a Metonic cycle
  EXPECT_EQ(goldenNumber(1595), 19);
}

// Gregorian Easter against dates anyone can verify from a calendar.
TEST(HistoricalCalendarGregorianEaster, ModernKnownDates) {
  EXPECT_TRUE(DateIs(gregorianEaster(2024), 3, 31));  // 31 March 2024
  EXPECT_TRUE(DateIs(gregorianEaster(2023), 4, 9));   // 9 April 2023
  EXPECT_TRUE(DateIs(gregorianEaster(2000), 4, 23));  // 23 April 2000
}

// Julian Easter is returned in the Julian calendar. Adding the modern 13-day
// offset must land on the Gregorian-calendar date of Orthodox Easter.
TEST(HistoricalCalendarJulianEaster, MatchesOrthodoxViaOffset) {
  const Date e2024 = julianEaster(2024);
  EXPECT_TRUE(DateIs(e2024, 4, 22));  // 22 April (Julian) + 13 = 5 May (Gregorian), Orthodox Easter 2024
  const Date e2023 = julianEaster(2023);
  EXPECT_TRUE(DateIs(e2023, 4, 3));  // 3 April (Julian) + 13 = 16 April (Gregorian), Orthodox Easter 2023
}

// The 1611 almanac row, the historical anchor. England was Old Style (Julian);
// the two Easters are exactly 10 days apart, which is the 1611 Julian-Gregorian
// offset -- a cross-check that both algorithms agree.
TEST(HistoricalCalendar1611, AlmanacRow) {
  EXPECT_EQ(goldenNumber(1611), 16);
  EXPECT_EQ(julianEpact(1611), 15);                  // 11*(16-1) mod 30 = 165 mod 30 = 15
  EXPECT_TRUE(DateIs(julianEaster(1611), 3, 24));    // 24 March, Old Style
  EXPECT_TRUE(DateIs(gregorianEaster(1611), 4, 3));  // 3 April, New Style = 24 March + 10
}

// Dominical letter: one letter for common years, two for leap years (Jan-Feb
// then Mar-Dec). Pinned to published values.
TEST(HistoricalCalendarDominicalLetter, CommonYears) {
  EXPECT_EQ(gregorianDominicalLetter(2023), "A");
  EXPECT_EQ(gregorianDominicalLetter(2022), "B");
  EXPECT_EQ(gregorianDominicalLetter(2021), "C");
}

TEST(HistoricalCalendarDominicalLetter, LeapYearsHaveTwo) {
  EXPECT_EQ(gregorianDominicalLetter(2024), "GF");
  EXPECT_EQ(gregorianDominicalLetter(2020), "ED");
  EXPECT_EQ(gregorianDominicalLetter(2000), "BA");
}
