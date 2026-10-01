#include <gtest/gtest.h>

#include "bible/BibleCompare.h"

using bible_compare::LoadedChapter;
using bible_compare::maxVerse;
using bible_compare::rowsForVerse;

namespace {
LoadedChapter chapter(const std::string& label, const std::vector<BibleVerse>& verses) { return {label, verses}; }
}  // namespace

TEST(BibleCompare, MaxVerseAcrossTranslations) {
  std::vector<LoadedChapter> chapters = {
      chapter("KJV", {{1, "a"}, {2, "b"}, {3, "c"}}),
      chapter("WEB", {{1, "a"}, {2, "b"}}),  // shorter
  };
  EXPECT_EQ(maxVerse(chapters), 3);
}

TEST(BibleCompare, MaxVerseEmptyIsZero) {
  std::vector<LoadedChapter> chapters;
  EXPECT_EQ(maxVerse(chapters), 0);
}

TEST(BibleCompare, RowsForVersePullsEachTranslation) {
  std::vector<LoadedChapter> chapters = {
      chapter("KJV", {{1, "In the beginning"}}),
      chapter("WEB", {{1, "In the beginning God"}}),
  };
  const auto rows = rowsForVerse(chapters, 1);
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_EQ(rows[0].label, "KJV");
  EXPECT_TRUE(rows[0].present);
  EXPECT_EQ(rows[0].text, "In the beginning");
  EXPECT_EQ(rows[1].label, "WEB");
  EXPECT_EQ(rows[1].text, "In the beginning God");
}

// A translation missing the verse still produces a row, marked not-present, so
// the UI shows a placeholder instead of silently dropping the column.
TEST(BibleCompare, MissingVerseStillGetsRow) {
  std::vector<LoadedChapter> chapters = {
      chapter("KJV", {{1, "a"}, {2, "b"}}),
      chapter("WEB", {{1, "a"}}),  // no verse 2
  };
  const auto rows = rowsForVerse(chapters, 2);
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_TRUE(rows[0].present);
  EXPECT_EQ(rows[0].text, "b");
  EXPECT_FALSE(rows[1].present);
  EXPECT_TRUE(rows[1].text.empty());
}

TEST(BibleCompare, OrderIsPreserved) {
  std::vector<LoadedChapter> chapters = {
      chapter("ASV", {{5, "x"}}),
      chapter("KJV", {{5, "y"}}),
      chapter("YLT", {{5, "z"}}),
  };
  const auto rows = rowsForVerse(chapters, 5);
  ASSERT_EQ(rows.size(), 3u);
  EXPECT_EQ(rows[0].label, "ASV");
  EXPECT_EQ(rows[1].label, "KJV");
  EXPECT_EQ(rows[2].label, "YLT");
}
