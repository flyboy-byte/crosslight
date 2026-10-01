#pragma once

#include <string>
#include <utility>
#include <vector>

#include "BibleChapterLoader.h"  // BibleVerse

// Pure helpers for the cross-translation compare screen: given the same chapter
// already loaded from several translations, pull one verse out of each and
// report the navigable verse range. No SD, no rendering -- just selection logic,
// so it is host-tested (test/bible_compare/) the way the number parser and the
// computus engine are. The activity does the SD loading and the drawing.
namespace bible_compare {

// One translation's take on a single verse.
struct VerseRow {
  std::string label;  // translation short label, e.g. "KJV"
  std::string text;   // the verse text, or empty when this translation lacks it
  bool present = false;
};

// A translation's loaded chapter: its label plus that chapter's verses.
using LoadedChapter = std::pair<std::string, std::vector<BibleVerse>>;

// Highest verse number present across all loaded chapters (translations can
// disagree on a chapter's verse count; nav should reach the longest). 0 if none.
inline int maxVerse(const std::vector<LoadedChapter>& chapters) {
  int hi = 0;
  for (const auto& ch : chapters) {
    for (const auto& v : ch.second) {
      if (v.number > hi) hi = v.number;
    }
  }
  return hi;
}

// One row per translation for `verseNumber`, in the order given. A translation
// that doesn't have that verse still gets a row (present=false), so the UI can
// show "(not in this translation)" rather than silently dropping a column.
inline std::vector<VerseRow> rowsForVerse(const std::vector<LoadedChapter>& chapters, const int verseNumber) {
  std::vector<VerseRow> rows;
  rows.reserve(chapters.size());
  for (const auto& ch : chapters) {
    VerseRow row;
    row.label = ch.first;
    for (const auto& v : ch.second) {
      if (v.number == verseNumber) {
        row.text = v.text;
        row.present = true;
        break;
      }
    }
    rows.push_back(std::move(row));
  }
  return rows;
}

}  // namespace bible_compare
