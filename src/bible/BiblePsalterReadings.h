#pragma once

#include <string>

// The Prayer Book's "Table for the Order of the Psalms" -- the 30-day Morning/
// Evening Psalter reading cycle the 1611 KJV's front matter printed ("The
// Table and Kalender... of Psalmes and Lessons... at Morning and Euening
// prayer"), unchanged in substance from 1549 through the 1662 Book of Common
// Prayer still in print today. Read through once a month: Day 1 starts at
// Psalm 1, Day 30 ends at Psalm 150.
//
// This is a FIXED historical table, not an open-ended catalog like Bible
// Numbers or Memory Work -- there is exactly one canonical version of it, so
// it is transcribed here as data rather than shipped as an SD-data file (no
// manual-copy step to forget, nothing to drop a new file into).
//
// Sourced and cross-checked 2026-10-01 against the Church of England's own
// published Psalter page order (churchofengland.org/.../book-common-prayer/
// psalter -- the table's 60 entries, read in order, are exactly this 30-day
// Morning+Evening cycle) and the well-documented fact that Psalm 119 is split
// across 5 segments spanning days 24-26 (it is too long to read in one
// sitting) -- an independent check that landed exactly where this table says
// it should. [[verify-dont-assume]]: do not alter this table from memory;
// re-derive from a primary BCP source if it's ever in question.
namespace bible_psalter {

// One reading: either a run of whole Psalms (fromVerse/toVerse both 0), or --
// for the three Psalm 119 segments only -- a verse range within Psalm 119.
struct Reading {
  int fromPsalm = 0;
  int toPsalm = 0;
  int fromVerse = 0;
  int toVerse = 0;

  // "Psalm 18" / "Psalms 116-118" / "Psalm 119:105-144".
  std::string label() const {
    if (fromVerse > 0) {
      return "Psalm " + std::to_string(fromPsalm) + ":" + std::to_string(fromVerse) + "-" + std::to_string(toVerse);
    }
    if (fromPsalm == toPsalm) return "Psalm " + std::to_string(fromPsalm);
    return "Psalms " + std::to_string(fromPsalm) + "-" + std::to_string(toPsalm);
  }
};

struct DayReadings {
  Reading morning;
  Reading evening;
};

namespace detail {
// clang-format off
constexpr DayReadings kTable[30] = {
    /* Day  1 */ {{1, 5}, {6, 8}},
    /* Day  2 */ {{9, 11}, {12, 14}},
    /* Day  3 */ {{15, 17}, {18, 18}},
    /* Day  4 */ {{19, 21}, {22, 23}},
    /* Day  5 */ {{24, 26}, {27, 29}},
    /* Day  6 */ {{30, 31}, {32, 34}},
    /* Day  7 */ {{35, 36}, {37, 37}},
    /* Day  8 */ {{38, 40}, {41, 43}},
    /* Day  9 */ {{44, 46}, {47, 49}},
    /* Day 10 */ {{50, 52}, {53, 55}},
    /* Day 11 */ {{56, 58}, {59, 61}},
    /* Day 12 */ {{62, 64}, {65, 67}},
    /* Day 13 */ {{68, 68}, {69, 70}},
    /* Day 14 */ {{71, 72}, {73, 74}},
    /* Day 15 */ {{75, 77}, {78, 78}},
    /* Day 16 */ {{79, 81}, {82, 85}},
    /* Day 17 */ {{86, 88}, {89, 89}},
    /* Day 18 */ {{90, 92}, {93, 94}},
    /* Day 19 */ {{95, 97}, {98, 101}},
    /* Day 20 */ {{102, 103}, {104, 104}},
    /* Day 21 */ {{105, 105}, {106, 106}},
    /* Day 22 */ {{107, 107}, {108, 109}},
    /* Day 23 */ {{110, 113}, {114, 115}},
    /* Day 24 */ {{116, 118}, {119, 119, 1, 32}},
    /* Day 25 */ {{119, 119, 33, 72}, {119, 119, 73, 104}},
    /* Day 26 */ {{119, 119, 105, 144}, {119, 119, 145, 176}},
    /* Day 27 */ {{120, 125}, {126, 131}},
    /* Day 28 */ {{132, 135}, {136, 138}},
    /* Day 29 */ {{139, 141}, {142, 143}},
    /* Day 30 */ {{144, 146}, {147, 150}},
};
// clang-format on
}  // namespace detail

// Readings for day 1-31 of the cycle. Day 31 repeats Day 30's readings -- the
// Prayer Book's own rule for the seven 31-day months, so the cycle restarts
// fresh on the 1st of the next month either way. False (out param untouched)
// for day < 1 or day > 31.
inline bool readingsForDay(const int dayOfMonth, DayReadings& out) {
  if (dayOfMonth < 1 || dayOfMonth > 31) return false;
  const int idx = (dayOfMonth > 30 ? 30 : dayOfMonth) - 1;
  out = detail::kTable[idx];
  return true;
}

}  // namespace bible_psalter
