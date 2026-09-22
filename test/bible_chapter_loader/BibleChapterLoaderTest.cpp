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

TEST_F(BibleCacheTest, SearchIsCaseInsensitiveAndCapped) {
  std::vector<BibleBookInfo> books;
  ASSERT_TRUE(BibleChapterLoader::buildCache(source.c_str(), books));

  std::vector<BibleChapterLoader::SearchHit> lower;
  std::vector<BibleChapterLoader::SearchHit> upper;
  ASSERT_TRUE(BibleChapterLoader::searchCache(source.c_str(), "king", 100, lower));
  ASSERT_TRUE(BibleChapterLoader::searchCache(source.c_str(), "  KING ", 100, upper));
  ASSERT_FALSE(lower.empty());
  ASSERT_EQ(lower.size(), upper.size());
  EXPECT_EQ(lower[0].book, "Esther");
  EXPECT_EQ(lower[0].chapter, 8);

  bool truncated = false;
  std::vector<BibleChapterLoader::SearchHit> capped;
  ASSERT_TRUE(BibleChapterLoader::searchCache(source.c_str(), "king", 2, capped, &truncated));
  EXPECT_EQ(capped.size(), 2u);
  EXPECT_TRUE(truncated);

  std::vector<BibleChapterLoader::SearchHit> none;
  ASSERT_TRUE(BibleChapterLoader::searchCache(source.c_str(), "zzzqqq", 100, none));
  EXPECT_TRUE(none.empty());
}

TEST_F(BibleCacheTest, LongVerseSnippetIsWindowedAroundMatch) {
  std::vector<BibleBookInfo> books;
  ASSERT_TRUE(BibleChapterLoader::buildCache(source.c_str(), books));
  // Esther 8:9 is 530 bytes; "their language" is its ending.
  std::vector<BibleChapterLoader::SearchHit> hits;
  ASSERT_TRUE(BibleChapterLoader::searchCache(source.c_str(), "according to their language", 10, hits));
  ASSERT_EQ(hits.size(), 1u);
  EXPECT_EQ(hits[0].verse, 9);
  EXPECT_LT(hits[0].snippet.size(), 130u);
  EXPECT_EQ(hits[0].snippet.rfind("...", 0), 0u) << "window should start mid-verse";
  EXPECT_NE(hits[0].snippet.find("their language"), std::string::npos);
}

TEST(BibleSearchFullKjvTest, FindsKnownVerses) {
  const fs::path kjv = fs::path(REPO_ROOT_DIR) / "fs_/Bible/KJV/kjv.json";
  if (!fs::exists(kjv)) GTEST_SKIP() << "no local KJV at " << kjv;
  const fs::path dir = fs::temp_directory_path() / ("bible_kjv_search_test_" + std::to_string(getpid()));
  fs::create_directories(dir);
  const std::string source = (dir / "kjv.json").string();
  fs::copy_file(kjv, source, fs::copy_options::overwrite_existing);
  std::vector<BibleBookInfo> books;
  ASSERT_TRUE(BibleChapterLoader::buildCache(source.c_str(), books));

  std::vector<BibleChapterLoader::SearchHit> hits;
  ASSERT_TRUE(BibleChapterLoader::searchCache(source.c_str(), "jesus wept", 10, hits));
  ASSERT_EQ(hits.size(), 1u);
  EXPECT_EQ(hits[0].book, "John");
  EXPECT_EQ(hits[0].chapter, 11);
  EXPECT_EQ(hits[0].verse, 35);

  ASSERT_TRUE(BibleChapterLoader::searchCache(source.c_str(), "in the beginning", 10, hits));
  ASSERT_GE(hits.size(), 2u);
  EXPECT_EQ(hits[0].book, "Genesis");
  EXPECT_EQ(hits[0].verse, 1);
  fs::remove_all(dir);
}

TEST_F(BibleCacheTest, VerseWhitespaceIsNormalizedInCacheAndJsonPaths) {
  // WEB-style text: paragraph indent, double spaces, trailing space.
  const std::string messy = (dir / "messy.json").string();
  std::ofstream(messy) << R"({"books":[{"name":"Genesis","chapters":[{"chapter":1,"verses":[)"
                       << R"({"verse":1,"text":"In the beginning."},)"
                       << R"({"verse":3,"text":"  God said,  “Let there be light,” and there was light. "}]}]}]})";
  const std::string expected = "God said, “Let there be light,” and there was light.";

  std::vector<BibleVerse> fromJson;
  ASSERT_TRUE(BibleChapterLoader::loadChapter(messy.c_str(), "Genesis", 1, fromJson));
  ASSERT_EQ(fromJson.size(), 2u);
  EXPECT_EQ(fromJson[1].text, expected);

  std::vector<BibleBookInfo> books;
  ASSERT_TRUE(BibleChapterLoader::buildCache(messy.c_str(), books));
  std::vector<BibleVerse> fromCache;
  ASSERT_TRUE(BibleChapterLoader::loadCachedChapter(messy.c_str(), books[0], 1, fromCache));
  ASSERT_EQ(fromCache.size(), 2u);
  EXPECT_EQ(fromCache[1].text, expected);

  std::vector<BibleChapterLoader::SearchHit> hits;
  ASSERT_TRUE(BibleChapterLoader::searchCache(messy.c_str(), "god said, “let there", 5, hits));
  EXPECT_EQ(hits.size(), 1u);
}
