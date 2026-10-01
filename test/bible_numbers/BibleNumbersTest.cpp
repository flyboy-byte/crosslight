#include <gtest/gtest.h>

#include "bible/BibleNumbers.h"

using bible_numbers::Classification;
using bible_numbers::classificationFromString;

// classificationFromString is the one place a typo in a data file could
// silently mis-bucket a claim, so it is pinned here: every spelling the data
// files actually use, plus the fallback behavior for anything unrecognized.

TEST(BibleNumbersClassification, RecognizesFact) { EXPECT_EQ(classificationFromString("FACT"), Classification::Fact); }

TEST(BibleNumbersClassification, RecognizesPatternCaseInsensitive) {
  EXPECT_EQ(classificationFromString("pattern"), Classification::Pattern);
  EXPECT_EQ(classificationFromString("Pattern"), Classification::Pattern);
}

TEST(BibleNumbersClassification, RecognizesDebate) {
  EXPECT_EQ(classificationFromString("DEBATE"), Classification::Debate);
  EXPECT_EQ(classificationFromString("scholarly_debate"), Classification::Debate);
}

TEST(BibleNumbersClassification, RecognizesTraditionSpellings) {
  EXPECT_EQ(classificationFromString("TRADITION"), Classification::Tradition);
  EXPECT_EQ(classificationFromString("traditional_interpretation"), Classification::Tradition);
  EXPECT_EQ(classificationFromString("interpretation"), Classification::Tradition);
}

// The whole point of the classifier: an unrecognized or missing class string
// must land in the most cautious bucket, never silently default to Fact.
TEST(BibleNumbersClassification, UnknownStringFallsBackToSpeculation) {
  EXPECT_EQ(classificationFromString("nonsense"), Classification::Speculation);
  EXPECT_EQ(classificationFromString(""), Classification::Speculation);
}

TEST(BibleNumbersRef, SingleVerseReferenceHasNoRange) {
  bible_numbers::Ref ref{"Genesis", 2, 3, 3};
  EXPECT_EQ(ref.reference(), "Genesis 2:3");
}

TEST(BibleNumbersRef, RangeReferenceShowsDash) {
  bible_numbers::Ref ref{"Exodus", 24, 18, 18};
  bible_numbers::Ref range{"Revelation", 21, 12, 14};
  EXPECT_EQ(ref.reference(), "Exodus 24:18");
  EXPECT_EQ(range.reference(), "Revelation 21:12-14");
}
