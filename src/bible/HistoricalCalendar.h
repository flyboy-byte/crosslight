#pragma once

#include <string>

// Historical Calendar: the computus apparatus an early-modern English Bible
// tabulated in its front matter -- Golden Number, Epact, Dominical (Sunday)
// Letter, and the date of Easter -- reconstructed for any year. This is the
// "An Almanacke for xxxix yeeres" table from the 1611 KJV generalized past its
// 1603-1641 range.
//
// Two calendars on purpose. In 1611 England still kept the Julian ("Old Style")
// calendar; Catholic Europe had switched to the Gregorian ("New Style") in 1582.
// The two give different Easters, and that ten-day gap is itself part of the
// history the tool teaches -- so both are computed and labelled.
//
// Pure arithmetic, dependency-free (needs only <string>), inline on purpose so
// it is host-testable without ArduinoJson/HalStorage -- every formula here is a
// place a subtle off-by-one would silently print a wrong date, so all of it is
// pinned by test/historical_calendar/HistoricalCalendarTest.cpp against
// known-answer values (modern Easters and the 1611 almanac row).
//
// NOTE on the 1611 row, recorded because a research handoff got it wrong: the
// Golden Number of 1611 is 16, not 13. (Y mod 19) + 1 = (1611 mod 19) + 1 =
// 15 + 1 = 16; 13 would be the year 1608. Verified here and in the tests.
namespace historical_calendar {

struct Date {
  int month = 0;  // 1 = January .. 12 = December
  int day = 0;
};

// Golden Number: the year's position in the 19-year Metonic lunar cycle, 1..19.
// The classical definition the almanac's "Golden Number" column used.
inline int goldenNumber(const int year) { return (year % 19) + 1; }

// Epact: the age of the ecclesiastical moon on 1 January, used to place the
// paschal full moon. The classical (Julian) epact is 11*(GoldenNumber-1) mod 30
// -- the value the Old-Style computus and the 1611 almanac carried.
inline int julianEpact(const int year) { return (11 * (goldenNumber(year) - 1)) % 30; }

// Easter in the Julian calendar (Meeus's Julian algorithm). The returned date is
// expressed in the Julian calendar itself -- i.e. the Old-Style date the 1611
// almanac would print. Add the Julian-Gregorian offset of the century (10 days
// in 1611) to get the civil Gregorian date of that same Sunday.
inline Date julianEaster(const int year) {
  const int a = year % 4;
  const int b = year % 7;
  const int c = year % 19;
  const int d = (19 * c + 15) % 30;
  const int e = (2 * a + 4 * b - d + 34) % 7;
  const int f = d + e + 114;  // 114 = 31*3 + 21: anchors the month/day split at March
  return {f / 31, (f % 31) + 1};
}

// Easter in the Gregorian calendar (the Anonymous Gregorian algorithm, a.k.a.
// Meeus/Jones/Butcher). Valid for every Gregorian year.
inline Date gregorianEaster(const int year) {
  const int a = year % 19;
  const int b = year / 100;
  const int c = year % 100;
  const int d = b / 4;
  const int e = b % 4;
  const int f = (b + 8) / 25;
  const int g = (b - f + 1) / 3;
  const int h = (19 * a + b - d - g + 15) % 30;
  const int i = c / 4;
  const int k = c % 4;
  const int l = (32 + 2 * e + 2 * i - h - k) % 7;
  const int m = (a + 11 * h + 22 * l) / 451;
  const int n = h + l - 7 * m + 114;
  return {n / 31, (n % 31) + 1};
}

// Gregorian Dominical (Sunday) Letter(s): the letter(s) marking which days are
// Sundays, where A = 1 January, B = 2 January, ... G = 7 January. A common year
// has one; a leap year has two (the first for Jan-Feb, the second for Mar-Dec,
// e.g. 2024 = "GF"), because the leap day shifts the weekday sequence.
inline std::string gregorianDominicalLetter(const int year) {
  const int idx = (year + year / 4 - year / 100 + year / 400) % 7;  // 0..6
  const int marDec = (7 - idx) % 7;                                 // 0 = A .. 6 = G
  const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
  const char second = static_cast<char>('A' + marDec);
  if (!leap) return std::string(1, second);
  const char first = static_cast<char>('A' + (marDec + 1) % 7);  // the letter one step ahead
  return std::string(1, first) + second;
}

// Full month name for display ("March"); empty for an out-of-range month.
inline const char* monthName(const int month) {
  static const char* const kNames[] = {"",     "January", "February",  "March",   "April",    "May",     "June",
                                       "July", "August",  "September", "October", "November", "December"};
  return (month >= 1 && month <= 12) ? kNames[month] : "";
}

// "March 24" style, calendar-agnostic (the caller labels Old/New Style).
inline std::string formatDate(const Date& date) {
  return std::string(monthName(date.month)) + " " + std::to_string(date.day);
}

}  // namespace historical_calendar
