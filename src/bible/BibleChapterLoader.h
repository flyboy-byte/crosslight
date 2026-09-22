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
  // Position of this book's first chapter in the on-SD chapter cache, or -1 when the
  // list came from a plain JSON scan and chapters must be loaded with loadChapter().
  int firstChapterIndex = -1;
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

  // --- On-SD chapter cache -------------------------------------------------
  // Parsing the whole KJV takes ~7s on the X4 Pro, so the first open writes
  // `<path>.cache`: the book list plus every chapter's verses at a known offset.
  // Later opens and chapter jumps read a few KB from it instead of parsing JSON.
  // The cache records the source file's size and mtime and is rebuilt when either
  // changes.

  static std::string cachePathFor(const char* path);

  // Fast path: the book list from an existing, up-to-date cache. False if the cache
  // is missing, stale, or unreadable (no parsing is attempted).
  static bool loadCachedBookIndex(const char* path, std::vector<BibleBookInfo>& outBooks);

  // One full parse of `path` that writes the cache and fills `outBooks`. If the
  // parse succeeds but the cache can't be written, still returns true with books
  // whose firstChapterIndex is -1 (callers fall back to loadChapter()).
  static bool buildCache(const char* path, std::vector<BibleBookInfo>& outBooks);

  // Reads one chapter from the cache. `book` must come from loadCachedBookIndex()
  // or buildCache(); returns false if it has no cache position.
  static bool loadCachedChapter(const char* path, const BibleBookInfo& book, int chapterNumber,
                                std::vector<BibleVerse>& outVerses);

  // Case-insensitive (ASCII) substring search over the whole cached translation, in
  // canonical order. Collects at most `maxHits`; `truncated` reports whether more
  // matched. False only if the cache is missing or unreadable (build it first).
  struct SearchHit {
    std::string book;
    int chapter = 0;
    int verse = 0;
    std::string snippet;  // the verse, or a window around the match for long verses
  };
  static bool searchCache(const char* path, const std::string& query, size_t maxHits, std::vector<SearchHit>& outHits,
                          bool* truncated = nullptr);
};
