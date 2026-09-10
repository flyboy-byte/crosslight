#include "BibleReference.h"

#include <gtest/gtest.h>

namespace {

// A slice of the real canonical order, enough to exercise prefix ambiguity
// ("jo" spans Joshua/Judges-adjacent names), the numbered books, and real
// chapter counts for range rejection.
const std::vector<BibleBookInfo>& testBooks() {
  static const std::vector<BibleBookInfo> books = {
      {"Genesis", 50},   {"Exodus", 40},    {"Joshua", 24},        {"Judges", 21},
      {"Psalms", 150},   {"Matthew", 28},   {"John", 21},          {"1 Corinthians", 16},
      {"1 John", 5},     {"2 John", 1},     {"Revelation", 22},
  };
  return books;
}

BibleReferenceQuery parseOk(const std::string& input) {
  BibleReferenceQuery out;
  EXPECT_TRUE(parseBibleReference(input, testBooks(), out)) << "failed to parse: " << input;
  return out;
}

}  // namespace

TEST(BibleReferenceTest, BookChapterAndVerse) {
  const auto ref = parseOk("John 3:16");
  EXPECT_EQ(ref.book, "John");
  EXPECT_EQ(ref.chapter, 3);
  EXPECT_EQ(ref.verse, 16);
}

TEST(BibleReferenceTest, ChapterDefaultsToOneWhenOmitted) {
  const auto ref = parseOk("Revelation");
  EXPECT_EQ(ref.book, "Revelation");
  EXPECT_EQ(ref.chapter, 1);
  EXPECT_EQ(ref.verse, 0);
}

TEST(BibleReferenceTest, VerseOptional) {
  const auto ref = parseOk("Psalms 23");
  EXPECT_EQ(ref.book, "Psalms");
  EXPECT_EQ(ref.chapter, 23);
  EXPECT_EQ(ref.verse, 0);
}

// Punctuation and spacing are dropped entirely, so all of these are the same
// query. This is the property the whole normalize() step exists for.
//
// Note "1 Joh." not "1 Jn." -- matching is prefix-based, so a contracted
// abbreviation that drops interior letters is *not* supported. The first draft
// of this test asserted "1 Jn." should work, contradicting the rule stated in
// BibleReference.h, and failed. See RejectsUnparseableInput for the pinned case.
TEST(BibleReferenceTest, SpacingAndPunctuationIrrelevant) {
  for (const char* input : {"1 John 2:3", "1john2:3", "1 Joh. 2:3", "  1  JOHN  2:3  "}) {
    const auto ref = parseOk(input);
    EXPECT_EQ(ref.book, "1 John") << input;
    EXPECT_EQ(ref.chapter, 2) << input;
    EXPECT_EQ(ref.verse, 3) << input;
  }
}

// Book names never end in a digit, which is what lets a numbered book sit next
// to a chapter number without a separator.
TEST(BibleReferenceTest, NumberedBookDistinguishedFromChapter) {
  const auto one = parseOk("1cor13");
  EXPECT_EQ(one.book, "1 Corinthians");
  EXPECT_EQ(one.chapter, 13);

  const auto two = parseOk("2john1");
  EXPECT_EQ(two.book, "2 John");
  EXPECT_EQ(two.chapter, 1);
}

TEST(BibleReferenceTest, PrefixAbbreviations) {
  EXPECT_EQ(parseOk("gen 1").book, "Genesis");
  EXPECT_EQ(parseOk("ps 23").book, "Psalms");
  EXPECT_EQ(parseOk("matt 5").book, "Matthew");
  EXPECT_EQ(parseOk("rev 22").book, "Revelation");
}

// Documented behavior, not an accident: ambiguity resolves to canonical order,
// so "jo" is Joshua even though a reader may have meant John.
TEST(BibleReferenceTest, AmbiguousPrefixTakesFirstCanonicalMatch) {
  EXPECT_EQ(parseOk("jo").book, "Joshua");
  EXPECT_EQ(parseOk("john").book, "John");
}

// An exact name wins over a prefix hit that appears earlier in canonical order.
TEST(BibleReferenceTest, ExactMatchBeatsEarlierPrefixMatch) {
  const auto ref = parseOk("judges 5");
  EXPECT_EQ(ref.book, "Judges");
  EXPECT_EQ(ref.chapter, 5);
}

TEST(BibleReferenceTest, RejectsChapterPastBookLength) {
  BibleReferenceQuery out;
  EXPECT_FALSE(parseBibleReference("2 John 4", testBooks(), out));   // only 1 chapter
  EXPECT_FALSE(parseBibleReference("Genesis 51", testBooks(), out));  // only 50
  EXPECT_FALSE(parseBibleReference("John 0", testBooks(), out));
}

TEST(BibleReferenceTest, RejectsUnparseableInput) {
  BibleReferenceQuery out;
  EXPECT_FALSE(parseBibleReference("", testBooks(), out));
  EXPECT_FALSE(parseBibleReference("   ", testBooks(), out));
  EXPECT_FALSE(parseBibleReference("42", testBooks(), out));            // no book
  EXPECT_FALSE(parseBibleReference("Habakkuk 1", testBooks(), out));    // not in this list
  EXPECT_FALSE(parseBibleReference("John 3:16:2", testBooks(), out));   // two colons
  EXPECT_FALSE(parseBibleReference("John :16", testBooks(), out));      // no chapter
}

// Pinned so the prefix-only rule is a decision, not an accident: contracted
// abbreviations that skip interior letters are not supported. If someone later
// adds an abbreviation table, this test should be updated deliberately.
TEST(BibleReferenceTest, ContractedAbbreviationsAreNotSupported) {
  BibleReferenceQuery out;
  EXPECT_FALSE(parseBibleReference("Jn 3:16", testBooks(), out));
  EXPECT_FALSE(parseBibleReference("1 Jn 2", testBooks(), out));
}
