#pragma once

#include <string>
#include <vector>

#include "BibleChapterLoader.h"

struct BibleReferenceQuery {
  std::string book;  // canonical book name, exactly as it appears in the translation file
  int chapter = 1;
  int verse = 0;  // 0 = not specified; caller decides what that means
};

// Parses a free-text verse reference ("John 3:16", "1 john 2", "ps23", "rev")
// against the canonical book list loaded from the translation file.
//
// Normalization drops everything but [a-z0-9:], so spacing and punctuation are
// irrelevant: "1 Jn." and "1john" normalize alike. Book names never end in a
// digit, which is what makes a trailing digit run unambiguously the chapter
// even for the numbered books ("1john2" -> 1 John, chapter 2).
//
// Book matching is exact-first, then *prefix* — so "gen", "ps", "matt", "1 cor"
// and "rev" all work, but non-prefix abbreviations ("jn", "mk", "lk") do not.
// An ambiguous prefix resolves to the first match in canonical order ("jo" ->
// Joshua, not John); deterministic and cheap, and typing more characters always
// disambiguates.
//
// Returns false when no book matches, or when the chapter is outside that
// book's real chapter count. The verse is *not* validated here (the book index
// carries chapter counts, not verse counts) — the caller resolves it against
// the loaded chapter.
bool parseBibleReference(const std::string& input, const std::vector<BibleBookInfo>& books,
                         BibleReferenceQuery& out);
