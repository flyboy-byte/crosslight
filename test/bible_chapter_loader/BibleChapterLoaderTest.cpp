#include "BibleChapterLoader.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

namespace {
std::string fixturePath(const char* name) { return std::string(BIBLE_FIXTURE_DIR) + "/" + name; }
}  // namespace

// Esther 8:9 is the longest verse in the whole KJV (530 UTF-8 bytes, verified
// against the real getBible KJV file -- see BibleChapterLoader.h). It's the
// one case that actually exercises StreamingJsonParser::TOKEN_BUF_SIZE's
// overflow path; a regression here means the buffer shrank back below the
// real worst case, silently dropping verse text again.
TEST(BibleChapterLoaderTest, EstherLongestVerseLoadsIntact) {
  std::vector<BibleVerse> verses;
  ASSERT_TRUE(BibleChapterLoader::loadChapter(fixturePath("esther8.json").c_str(), "Esther", 8, verses));

  auto it = std::find_if(verses.begin(), verses.end(), [](const BibleVerse& v) { return v.number == 9; });
  ASSERT_NE(it, verses.end()) << "verse 9 missing entirely -- token overflow dropped it";

  // 530 UTF-8 *bytes* (the curly apostrophe in "king's" is 3 bytes, 1
  // codepoint) -- not the 528 you'd get counting codepoints/Python len().
  EXPECT_EQ(it->text.size(), 530u);
  EXPECT_EQ(it->text.rfind("Then were the king", 0), 0u) << "verse start truncated/corrupted";
  ASSERT_GE(it->text.size(), 30u);
  EXPECT_EQ(it->text.substr(it->text.size() - 30), "d according to their language.")
      << "verse end truncated -- overflow dropped the tail";
}

TEST(BibleChapterLoaderTest, EstherChapterHasAllSeventeenVerses) {
  std::vector<BibleVerse> verses;
  ASSERT_TRUE(BibleChapterLoader::loadChapter(fixturePath("esther8.json").c_str(), "Esther", 8, verses));
  EXPECT_EQ(verses.size(), 17u);
}

TEST(BibleChapterLoaderTest, MissingFileFails) {
  std::vector<BibleVerse> verses;
  EXPECT_FALSE(BibleChapterLoader::loadChapter("/does/not/exist.json", "Esther", 8, verses));
}
