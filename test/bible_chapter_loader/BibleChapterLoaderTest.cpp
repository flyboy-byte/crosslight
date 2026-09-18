#include "BibleChapterLoader.h"

#include <gtest/gtest.h>

#include <unistd.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {
std::string fixturePath(const char* name) { return std::string(BIBLE_FIXTURE_DIR) + "/" + name; }

// The cache is written next to its source, so tests work on a temp copy.
class BibleCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dir = fs::temp_directory_path() / ("bible_cache_test_" + std::to_string(getpid()));
    fs::create_directories(dir);
    source = (dir / "esther8.json").string();
    fs::copy_file(fixturePath("esther8.json"), source, fs::copy_options::overwrite_existing);
  }
  void TearDown() override { fs::remove_all(dir); }

  fs::path dir;
  std::string source;
};
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

TEST_F(BibleCacheTest, CachedChapterMatchesJsonChapter) {
  std::vector<BibleBookInfo> built;
  ASSERT_TRUE(BibleChapterLoader::buildCache(source.c_str(), built));
  ASSERT_EQ(built.size(), 1u);
  EXPECT_EQ(built[0].name, "Esther");
  EXPECT_EQ(built[0].chapterCount, 1);
  EXPECT_EQ(built[0].firstChapterIndex, 0) << "cache was not written";

  std::vector<BibleBookInfo> cached;
  ASSERT_TRUE(BibleChapterLoader::loadCachedBookIndex(source.c_str(), cached));
  ASSERT_EQ(cached.size(), 1u);
  EXPECT_EQ(cached[0].name, "Esther");
  EXPECT_EQ(cached[0].firstChapterIndex, 0);

  // The fixture's only chapter is numbered 8, so this also covers the non-ordinal lookup.
  std::vector<BibleVerse> fromCache;
  std::vector<BibleVerse> fromJson;
  ASSERT_TRUE(BibleChapterLoader::loadCachedChapter(source.c_str(), cached[0], 8, fromCache));
  ASSERT_TRUE(BibleChapterLoader::loadChapter(source.c_str(), "Esther", 8, fromJson));
  ASSERT_EQ(fromCache.size(), fromJson.size());
  for (size_t i = 0; i < fromJson.size(); ++i) {
    EXPECT_EQ(fromCache[i].number, fromJson[i].number);
    EXPECT_EQ(fromCache[i].text, fromJson[i].text);
  }
  EXPECT_FALSE(BibleChapterLoader::loadCachedChapter(source.c_str(), cached[0], 1, fromCache));
}

TEST_F(BibleCacheTest, StaleCacheIsRejected) {
  std::vector<BibleBookInfo> books;
  ASSERT_TRUE(BibleChapterLoader::buildCache(source.c_str(), books));
  ASSERT_TRUE(BibleChapterLoader::loadCachedBookIndex(source.c_str(), books));

  std::ofstream(source, std::ios::app) << "\n";
  EXPECT_FALSE(BibleChapterLoader::loadCachedBookIndex(source.c_str(), books));
}

TEST_F(BibleCacheTest, MissingCacheIsNotAnError) {
  std::vector<BibleBookInfo> books;
  EXPECT_FALSE(BibleChapterLoader::loadCachedBookIndex(source.c_str(), books));
  EXPECT_TRUE(books.empty());
}

// Full-KJV check against the simulator's SD copy (gitignored, so skipped where absent):
// the first and last chapter of every book must match the JSON loader byte for byte.
TEST(BibleCacheFullKjvTest, FirstAndLastChapterOfEveryBookMatchJson) {
  const fs::path kjv = fs::path(REPO_ROOT_DIR) / "fs_/Bible/KJV/kjv.json";
  if (!fs::exists(kjv)) GTEST_SKIP() << "no local KJV at " << kjv;
  const fs::path dir = fs::temp_directory_path() / ("bible_kjv_cache_test_" + std::to_string(getpid()));
  fs::create_directories(dir);
  const std::string source = (dir / "kjv.json").string();
  fs::copy_file(kjv, source, fs::copy_options::overwrite_existing);

  std::vector<BibleBookInfo> books;
  ASSERT_TRUE(BibleChapterLoader::buildCache(source.c_str(), books));
  ASSERT_EQ(books.size(), 66u);
  int chapters = 0;
  for (const auto& book : books) chapters += book.chapterCount;
  EXPECT_EQ(chapters, 1189);

  for (const auto& book : books) {
    for (int chapter : {1, book.chapterCount}) {
      std::vector<BibleVerse> fromCache;
      std::vector<BibleVerse> fromJson;
      ASSERT_TRUE(BibleChapterLoader::loadCachedChapter(source.c_str(), book, chapter, fromCache))
          << book.name << " " << chapter;
      ASSERT_TRUE(BibleChapterLoader::loadChapter(source.c_str(), book.name.c_str(), chapter, fromJson));
      ASSERT_EQ(fromCache.size(), fromJson.size()) << book.name << " " << chapter;
      for (size_t i = 0; i < fromJson.size(); ++i) {
        ASSERT_EQ(fromCache[i].number, fromJson[i].number);
        ASSERT_EQ(fromCache[i].text, fromJson[i].text) << book.name << " " << chapter << ":" << fromJson[i].number;
      }
    }
  }
  fs::remove_all(dir);
}
