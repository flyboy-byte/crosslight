#pragma once

#include <string>
#include <vector>

// Streams a getBible-format (https://api.getbible.net/v2/<abbrev>.json)
// translation file from SD and extracts one chapter's verses, without
// loading the whole file into memory. Full-Bible getBible files run
// 5-10MB (KJV is ~8.9MB); StreamingJsonParser is SAX-based (small fixed
// read buffer, no DOM), which is what makes this feasible on-device.
//
// File shape (verified against a live fetch, 2026-09-09):
//   { ..., "books": [ { "nr", "name", "chapters": [ { "chapter", "name",
//     "verses": [ { "chapter", "verse", "name", "text" } ] } ] } ] }
//
// StreamingJsonParser's fixed token buffer (TOKEN_BUF_SIZE) drops (does not
// truncate) any string value longer than it. Verified against live KJV data:
// Esther 8:9 (530 UTF-8 bytes) is the single longest verse in the whole Bible, and
// TOKEN_BUF_SIZE was bumped past it for exactly that reason -- see
// BibleChapterLoaderTest's EstherLongestVerseLoadsIntact regression test.
struct BibleVerse {
  int number = 0;
  std::string text;
};

// One book's name and chapter count, for a book/chapter picker. No verse text.
struct BibleBookInfo {
  std::string name;
  int chapterCount = 0;
};

class BibleChapterLoader {
 public:
  // Loads `bookName`/`chapterNumber` (1-based) from a getBible-format JSON
  // file at `path` on SD (opened via HalStorage). Returns false if the file
  // can't be opened or the book/chapter isn't found; `outVerses` is
  // untouched on failure. `bookName` must match the file's book "name"
  // field exactly (e.g. "Genesis").
  static bool loadChapter(const char* path, const char* bookName, int chapterNumber,
                           std::vector<BibleVerse>& outVerses);

  // Scans `path` for its book list and each book's chapter count, without
  // materializing any verse text. Still a full-file streaming pass (the SAX
  // parser tokenizes every byte regardless), but memory stays flat since verse
  // text is never stored. Returns false if the file can't be opened or a JSON
  // error occurs; `outBooks` is untouched on failure.
  static bool loadBookIndex(const char* path, std::vector<BibleBookInfo>& outBooks);
};
