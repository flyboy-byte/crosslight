#include "BibleChapterLoader.h"

#include <HalStorage.h>
#include <Logging.h>
#include <StreamingJsonParser.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>

namespace {

// Heap, not stack: 512-byte reads made opening the Bible a ~10s, ~17k-read pass on
// the X4 Pro's SD (measured 2026-09-18).
constexpr size_t READ_CHUNK_SIZE = 16 * 1024;
constexpr size_t BOOK_NAME_BUF_SIZE = 64;

// Semantic container kind at the current nesting level. Tracked as a small
// manual stack (max depth ~7 for this fixed schema) since StreamingJsonParser
// only reports generic object/array start/end, not what they mean.
enum class Frame : uint8_t {
  ROOT,
  BOOKS_ARRAY,
  BOOK_OBJECT,
  CHAPTERS_ARRAY,
  CHAPTER_OBJECT,
  VERSES_ARRAY,
  VERSE_OBJECT,
  OTHER,  // anything not on the path we care about (skipped, not descended into specially)
};

enum class Key : uint8_t {
  NONE,
  BOOKS,
  NAME,
  CHAPTERS,
  CHAPTER_NUM,
  VERSES,
  VERSE_NUM,
  TEXT,
  OTHER,
};

constexpr uint8_t MAX_FRAME_DEPTH = 16;

struct ParseContext {
  const char* targetBook;
  int targetChapter;
  std::vector<BibleVerse>* outVerses;

  Frame frameStack[MAX_FRAME_DEPTH];
  uint8_t frameDepth = 0;
  Key lastKey = Key::NONE;

  bool inTargetBook = false;
  bool inTargetChapter = false;
  bool done = false;  // set once the target chapter's verses array closes

  char currentBookName[BOOK_NAME_BUF_SIZE] = {0};
  int currentVerseNumber = 0;

  Frame top() const { return frameDepth > 0 ? frameStack[frameDepth - 1] : Frame::ROOT; }
  void push(Frame f) {
    if (frameDepth < MAX_FRAME_DEPTH) frameStack[frameDepth++] = f;
  }
  void pop() {
    if (frameDepth > 0) --frameDepth;
  }
};

Key keyFromString(const char* key, size_t len) {
  auto is = [&](const char* s) { return len == strlen(s) && memcmp(key, s, len) == 0; };
  if (is("books")) return Key::BOOKS;
  if (is("name")) return Key::NAME;
  if (is("chapters")) return Key::CHAPTERS;
  if (is("chapter")) return Key::CHAPTER_NUM;
  if (is("verses")) return Key::VERSES;
  if (is("verse")) return Key::VERSE_NUM;
  if (is("text")) return Key::TEXT;
  return Key::OTHER;
}

void onKey(void* ctxPtr, const char* key, size_t len) {
  auto* ctx = static_cast<ParseContext*>(ctxPtr);
  ctx->lastKey = keyFromString(key, len);
}

void onObjectStart(void* ctxPtr) {
  auto* ctx = static_cast<ParseContext*>(ctxPtr);
  const Frame parent = ctx->top();
  if (ctx->frameDepth == 0) {
    ctx->push(Frame::ROOT);
  } else if (parent == Frame::BOOKS_ARRAY) {
    ctx->push(Frame::BOOK_OBJECT);
    ctx->currentBookName[0] = '\0';
    ctx->inTargetBook = false;
  } else if (parent == Frame::CHAPTERS_ARRAY) {
    ctx->push(Frame::CHAPTER_OBJECT);
    ctx->inTargetChapter = false;
  } else if (parent == Frame::VERSES_ARRAY) {
    ctx->push(Frame::VERSE_OBJECT);
    ctx->currentVerseNumber = 0;
  } else {
    ctx->push(Frame::OTHER);
  }
}

void onObjectEnd(void* ctxPtr) {
  auto* ctx = static_cast<ParseContext*>(ctxPtr);
  const Frame closing = ctx->top();
  ctx->pop();
  if (closing == Frame::BOOK_OBJECT) {
    ctx->inTargetBook = false;
  } else if (closing == Frame::CHAPTER_OBJECT) {
    ctx->inTargetChapter = false;
  }
}

void onArrayStart(void* ctxPtr) {
  auto* ctx = static_cast<ParseContext*>(ctxPtr);
  const Frame parent = ctx->top();
  const Key key = ctx->lastKey;
  if (parent == Frame::ROOT && key == Key::BOOKS) {
    ctx->push(Frame::BOOKS_ARRAY);
  } else if (parent == Frame::BOOK_OBJECT && key == Key::CHAPTERS) {
    ctx->push(Frame::CHAPTERS_ARRAY);
  } else if (parent == Frame::CHAPTER_OBJECT && key == Key::VERSES) {
    ctx->push(Frame::VERSES_ARRAY);
  } else {
    ctx->push(Frame::OTHER);
  }
}

void onArrayEnd(void* ctxPtr) {
  auto* ctx = static_cast<ParseContext*>(ctxPtr);
  const Frame closing = ctx->top();
  ctx->pop();
  if (closing == Frame::VERSES_ARRAY && ctx->inTargetBook && ctx->inTargetChapter) {
    // We just finished the verses array of the chapter we wanted -- nothing
    // more to collect. The caller stops feeding further file bytes once it
    // sees `done`.
    ctx->done = true;
  }
}

void onString(void* ctxPtr, const char* value, size_t len) {
  auto* ctx = static_cast<ParseContext*>(ctxPtr);
  if (ctx->top() == Frame::BOOK_OBJECT && ctx->lastKey == Key::NAME) {
    const size_t n = len < BOOK_NAME_BUF_SIZE - 1 ? len : BOOK_NAME_BUF_SIZE - 1;
    memcpy(ctx->currentBookName, value, n);
    ctx->currentBookName[n] = '\0';
    ctx->inTargetBook = (strcmp(ctx->currentBookName, ctx->targetBook) == 0);
  } else if (ctx->top() == Frame::VERSE_OBJECT && ctx->lastKey == Key::TEXT && ctx->inTargetBook &&
             ctx->inTargetChapter) {
    ctx->outVerses->push_back(BibleVerse{ctx->currentVerseNumber, std::string(value, len)});
  }
}

void onNumber(void* ctxPtr, const char* value, size_t len) {
  auto* ctx = static_cast<ParseContext*>(ctxPtr);
  if (ctx->top() == Frame::CHAPTER_OBJECT && ctx->lastKey == Key::CHAPTER_NUM) {
    char buf[16];
    const size_t n = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
    memcpy(buf, value, n);
    buf[n] = '\0';
    const int chapterNum = atoi(buf);
    ctx->inTargetChapter = ctx->inTargetBook && (chapterNum == ctx->targetChapter);
  } else if (ctx->top() == Frame::VERSE_OBJECT && ctx->lastKey == Key::VERSE_NUM) {
    char buf[16];
    const size_t n = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
    memcpy(buf, value, n);
    buf[n] = '\0';
    ctx->currentVerseNumber = atoi(buf);
  }
}

// --- book index scan (name + chapter count only, no verse text) -----------

struct IndexParseContext {
  std::vector<BibleBookInfo>* outBooks;

  Frame frameStack[MAX_FRAME_DEPTH];
  uint8_t frameDepth = 0;
  Key lastKey = Key::NONE;

  std::string currentBookName;
  int currentChapterCount = 0;

  Frame top() const { return frameDepth > 0 ? frameStack[frameDepth - 1] : Frame::ROOT; }
  void push(Frame f) {
    if (frameDepth < MAX_FRAME_DEPTH) frameStack[frameDepth++] = f;
  }
  void pop() {
    if (frameDepth > 0) --frameDepth;
  }
};

void onIndexKey(void* ctxPtr, const char* key, size_t len) {
  auto* ctx = static_cast<IndexParseContext*>(ctxPtr);
  ctx->lastKey = keyFromString(key, len);
}

void onIndexObjectStart(void* ctxPtr) {
  auto* ctx = static_cast<IndexParseContext*>(ctxPtr);
  const Frame parent = ctx->top();
  if (ctx->frameDepth == 0) {
    ctx->push(Frame::ROOT);
  } else if (parent == Frame::BOOKS_ARRAY) {
    ctx->push(Frame::BOOK_OBJECT);
    ctx->currentBookName.clear();
    ctx->currentChapterCount = 0;
  } else if (parent == Frame::CHAPTERS_ARRAY) {
    ctx->push(Frame::CHAPTER_OBJECT);
    ++ctx->currentChapterCount;
  } else if (parent == Frame::VERSES_ARRAY) {
    ctx->push(Frame::VERSE_OBJECT);
  } else {
    ctx->push(Frame::OTHER);
  }
}

void onIndexObjectEnd(void* ctxPtr) {
  auto* ctx = static_cast<IndexParseContext*>(ctxPtr);
  const Frame closing = ctx->top();
  ctx->pop();
  if (closing == Frame::BOOK_OBJECT) {
    ctx->outBooks->push_back(BibleBookInfo{ctx->currentBookName, ctx->currentChapterCount});
  }
}

void onIndexArrayStart(void* ctxPtr) {
  auto* ctx = static_cast<IndexParseContext*>(ctxPtr);
  const Frame parent = ctx->top();
  const Key key = ctx->lastKey;
  if (parent == Frame::ROOT && key == Key::BOOKS) {
    ctx->push(Frame::BOOKS_ARRAY);
  } else if (parent == Frame::BOOK_OBJECT && key == Key::CHAPTERS) {
    ctx->push(Frame::CHAPTERS_ARRAY);
  } else if (parent == Frame::CHAPTER_OBJECT && key == Key::VERSES) {
    ctx->push(Frame::VERSES_ARRAY);
  } else {
    ctx->push(Frame::OTHER);
  }
}

void onIndexArrayEnd(void* ctxPtr) {
  auto* ctx = static_cast<IndexParseContext*>(ctxPtr);
  ctx->pop();
}

void onIndexString(void* ctxPtr, const char* value, size_t len) {
  auto* ctx = static_cast<IndexParseContext*>(ctxPtr);
  if (ctx->top() == Frame::BOOK_OBJECT && ctx->lastKey == Key::NAME) {
    ctx->currentBookName.assign(value, len);
  }
}

// --- chapter cache ---------------------------------------------------------
// Little-endian, written raw (ESP32 and every host we build on are LE):
//   CacheHeader | chapter blobs | book table | chapter table
// Book table entry: u8 nameLen, name, u16 chapterCount, u16 firstChapterIndex.
// Chapter blob: repeated { u16 verseNumber, u16 textLen, text bytes }.

constexpr char CACHE_MAGIC[4] = {'C', 'L', 'B', 'C'};
constexpr uint16_t CACHE_VERSION = 1;

#pragma pack(push, 1)
struct CacheHeader {
  char magic[4];
  uint16_t version;
  uint16_t bookCount;
  uint64_t sourceSize;
  uint32_t sourceMtime;
  uint32_t booksOffset;
  uint32_t chaptersOffset;
  uint16_t chapterCount;
  uint16_t reserved;
};
struct ChapterEntry {
  uint32_t offset;
  uint32_t length;
  uint16_t number;
};
#pragma pack(pop)
static_assert(sizeof(CacheHeader) == 32, "cache header layout");
static_assert(sizeof(ChapterEntry) == 10, "cache chapter entry layout");

bool sourceStamp(const char* path, uint64_t& size, uint32_t& mtime) {
  HalFile file;
  if (!Storage.openFileForRead("BIBLE", path, file)) return false;
  size = file.fileSize64();
  mtime = file.modificationTime();
  return true;
}

bool readHeader(HalFile& file, CacheHeader& header) {
  return file.read(&header, sizeof(header)) == static_cast<int>(sizeof(header)) &&
         memcmp(header.magic, CACHE_MAGIC, sizeof(CACHE_MAGIC)) == 0 && header.version == CACHE_VERSION;
}

struct BuildContext {
  HalFile* out;
  std::vector<BibleBookInfo>* outBooks;
  std::vector<ChapterEntry> chapters;
  bool writeFailed = false;
  uint32_t dataPos = sizeof(CacheHeader);

  Frame frameStack[MAX_FRAME_DEPTH];
  uint8_t frameDepth = 0;
  Key lastKey = Key::NONE;

  std::string bookName;
  int bookChapterCount = 0;
  int bookFirstChapter = 0;
  int chapterNumber = 0;
  int verseNumber = 0;
  std::string chapterBlob;

  Frame top() const { return frameDepth > 0 ? frameStack[frameDepth - 1] : Frame::ROOT; }
  void push(Frame f) {
    if (frameDepth < MAX_FRAME_DEPTH) frameStack[frameDepth++] = f;
  }
  void pop() {
    if (frameDepth > 0) --frameDepth;
  }
};

void appendU16(std::string& s, uint16_t v) {
  s.push_back(static_cast<char>(v & 0xFF));
  s.push_back(static_cast<char>(v >> 8));
}

int parseInt(const char* value, size_t len) {
  char buf[16];
  const size_t n = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
  memcpy(buf, value, n);
  buf[n] = '\0';
  return atoi(buf);
}

void onBuildKey(void* ctxPtr, const char* key, size_t len) {
  static_cast<BuildContext*>(ctxPtr)->lastKey = keyFromString(key, len);
}

void onBuildObjectStart(void* ctxPtr) {
  auto* ctx = static_cast<BuildContext*>(ctxPtr);
  const Frame parent = ctx->top();
  if (ctx->frameDepth == 0) {
    ctx->push(Frame::ROOT);
  } else if (parent == Frame::BOOKS_ARRAY) {
    ctx->push(Frame::BOOK_OBJECT);
    ctx->bookName.clear();
    ctx->bookChapterCount = 0;
    ctx->bookFirstChapter = static_cast<int>(ctx->chapters.size());
  } else if (parent == Frame::CHAPTERS_ARRAY) {
    ctx->push(Frame::CHAPTER_OBJECT);
    ctx->chapterBlob.clear();
    ctx->chapterNumber = ctx->bookChapterCount + 1;  // ordinal unless the JSON says otherwise
    ++ctx->bookChapterCount;
  } else if (parent == Frame::VERSES_ARRAY) {
    ctx->push(Frame::VERSE_OBJECT);
    ctx->verseNumber = 0;
  } else {
    ctx->push(Frame::OTHER);
  }
}

void onBuildObjectEnd(void* ctxPtr) {
  auto* ctx = static_cast<BuildContext*>(ctxPtr);
  const Frame closing = ctx->top();
  ctx->pop();
  if (closing == Frame::CHAPTER_OBJECT) {
    const auto len = static_cast<uint32_t>(ctx->chapterBlob.size());
    if (!ctx->writeFailed && len > 0 && ctx->out->write(ctx->chapterBlob.data(), len) != len) {
      ctx->writeFailed = true;
    }
    ctx->chapters.push_back(ChapterEntry{ctx->dataPos, len, static_cast<uint16_t>(ctx->chapterNumber)});
    ctx->dataPos += len;
  } else if (closing == Frame::BOOK_OBJECT) {
    ctx->outBooks->push_back(BibleBookInfo{ctx->bookName, ctx->bookChapterCount, ctx->bookFirstChapter});
  }
}

void onBuildArrayStart(void* ctxPtr) {
  auto* ctx = static_cast<BuildContext*>(ctxPtr);
  const Frame parent = ctx->top();
  const Key key = ctx->lastKey;
  if (parent == Frame::ROOT && key == Key::BOOKS) {
    ctx->push(Frame::BOOKS_ARRAY);
  } else if (parent == Frame::BOOK_OBJECT && key == Key::CHAPTERS) {
    ctx->push(Frame::CHAPTERS_ARRAY);
  } else if (parent == Frame::CHAPTER_OBJECT && key == Key::VERSES) {
    ctx->push(Frame::VERSES_ARRAY);
  } else {
    ctx->push(Frame::OTHER);
  }
}

void onBuildArrayEnd(void* ctxPtr) { static_cast<BuildContext*>(ctxPtr)->pop(); }

void onBuildString(void* ctxPtr, const char* value, size_t len) {
  auto* ctx = static_cast<BuildContext*>(ctxPtr);
  if (ctx->top() == Frame::BOOK_OBJECT && ctx->lastKey == Key::NAME) {
    ctx->bookName.assign(value, len);
  } else if (ctx->top() == Frame::VERSE_OBJECT && ctx->lastKey == Key::TEXT) {
    const auto n = static_cast<uint16_t>(len < 0xFFFF ? len : 0xFFFF);
    appendU16(ctx->chapterBlob, static_cast<uint16_t>(ctx->verseNumber));
    appendU16(ctx->chapterBlob, n);
    ctx->chapterBlob.append(value, n);
  }
}

void onBuildNumber(void* ctxPtr, const char* value, size_t len) {
  auto* ctx = static_cast<BuildContext*>(ctxPtr);
  if (ctx->top() == Frame::CHAPTER_OBJECT && ctx->lastKey == Key::CHAPTER_NUM) {
    ctx->chapterNumber = parseInt(value, len);
  } else if (ctx->top() == Frame::VERSE_OBJECT && ctx->lastKey == Key::VERSE_NUM) {
    ctx->verseNumber = parseInt(value, len);
  }
}

bool readChapterEntry(HalFile& file, const CacheHeader& header, int index, ChapterEntry& entry) {
  if (index < 0 || index >= header.chapterCount) return false;
  return file.seek(header.chaptersOffset + static_cast<size_t>(index) * sizeof(ChapterEntry)) &&
         file.read(&entry, sizeof(entry)) == static_cast<int>(sizeof(entry));
}

}  // namespace

bool BibleChapterLoader::loadChapter(const char* path, const char* bookName, int chapterNumber,
                                     std::vector<BibleVerse>& outVerses) {
  HalFile file;
  if (!Storage.openFileForRead("BIBLE", path, file)) {
    LOG_ERR("BIBLE", "Could not open translation file: %s", path);
    return false;
  }

  outVerses.clear();  // callers reuse the vector across chapter switches

  ParseContext ctx;
  ctx.targetBook = bookName;
  ctx.targetChapter = chapterNumber;
  ctx.outVerses = &outVerses;
  outVerses.reserve(40);  // most chapters are well under this; Psalm 119 (176) is the outlier

  const JsonCallbacks callbacks{&ctx,  onKey,        onString,     onNumber,   nullptr /*onBool*/,
                                nullptr /*onNull*/, onObjectStart, onObjectEnd, onArrayStart, onArrayEnd};
  StreamingJsonParser parser(callbacks);

  std::unique_ptr<char[]> buf(new (std::nothrow) char[READ_CHUNK_SIZE]);
  if (!buf) {
    LOG_ERR("BIBLE", "OOM: read buffer (%u bytes)", static_cast<unsigned>(READ_CHUNK_SIZE));
    return false;
  }
  int n;
  while (!ctx.done && (n = file.read(buf.get(), READ_CHUNK_SIZE)) > 0) {
    parser.feed(buf.get(), static_cast<size_t>(n));
    if (parser.hasError()) {
      LOG_ERR("BIBLE", "JSON parse error in %s", path);
      return false;
    }
  }

  if (outVerses.empty()) {
    LOG_ERR("BIBLE", "Book/chapter not found: %s %d (in %s)", bookName, chapterNumber, path);
    return false;
  }
  return true;
}

bool BibleChapterLoader::loadBookIndex(const char* path, std::vector<BibleBookInfo>& outBooks) {
  HalFile file;
  if (!Storage.openFileForRead("BIBLE", path, file)) {
    LOG_ERR("BIBLE", "Could not open translation file: %s", path);
    return false;
  }

  IndexParseContext ctx;
  ctx.outBooks = &outBooks;
  outBooks.reserve(66);  // most translations follow the 66-book Protestant canon

  const JsonCallbacks callbacks{&ctx, onIndexKey,          onIndexString,       nullptr /*onNumber*/,
                                nullptr /*onBool*/, nullptr /*onNull*/, onIndexObjectStart, onIndexObjectEnd,
                                onIndexArrayStart,   onIndexArrayEnd};
  StreamingJsonParser parser(callbacks);

  std::unique_ptr<char[]> buf(new (std::nothrow) char[READ_CHUNK_SIZE]);
  if (!buf) {
    LOG_ERR("BIBLE", "OOM: read buffer (%u bytes)", static_cast<unsigned>(READ_CHUNK_SIZE));
    return false;
  }
  int n;
  while ((n = file.read(buf.get(), READ_CHUNK_SIZE)) > 0) {
    parser.feed(buf.get(), static_cast<size_t>(n));
    if (parser.hasError()) {
      LOG_ERR("BIBLE", "JSON parse error in %s", path);
      return false;
    }
  }

  if (outBooks.empty()) {
    LOG_ERR("BIBLE", "No books found in %s", path);
    return false;
  }
  return true;
}

std::string BibleChapterLoader::cachePathFor(const char* path) { return std::string(path) + ".cache"; }

bool BibleChapterLoader::loadCachedBookIndex(const char* path, std::vector<BibleBookInfo>& outBooks) {
  uint64_t sourceSize = 0;
  uint32_t sourceMtime = 0;
  if (!sourceStamp(path, sourceSize, sourceMtime)) return false;

  const std::string cachePath = cachePathFor(path);
  if (!Storage.exists(cachePath.c_str())) return false;  // first open: expected, not an error
  HalFile file;
  if (!Storage.openFileForRead("BIBLE", cachePath, file)) return false;
  CacheHeader header;
  if (!readHeader(file, header) || header.sourceSize != sourceSize || header.sourceMtime != sourceMtime ||
      header.chaptersOffset < header.booksOffset) {
    LOG_INF("BIBLE", "Chapter cache missing or stale for %s", path);
    return false;
  }

  const size_t tableLen = header.chaptersOffset - header.booksOffset;
  std::unique_ptr<uint8_t[]> table(new (std::nothrow) uint8_t[tableLen]);
  if (!table || !file.seek(header.booksOffset) || file.read(table.get(), tableLen) != static_cast<int>(tableLen)) {
    return false;
  }

  std::vector<BibleBookInfo> books;
  books.reserve(header.bookCount);
  size_t pos = 0;
  for (uint16_t i = 0; i < header.bookCount; ++i) {
    if (pos + 1 > tableLen) return false;
    const uint8_t nameLen = table[pos++];
    if (pos + nameLen + 4 > tableLen) return false;
    BibleBookInfo book;
    book.name.assign(reinterpret_cast<const char*>(&table[pos]), nameLen);
    pos += nameLen;
    book.chapterCount = table[pos] | (table[pos + 1] << 8);
    book.firstChapterIndex = table[pos + 2] | (table[pos + 3] << 8);
    pos += 4;
    books.push_back(std::move(book));
  }
  if (books.empty()) return false;
  outBooks = std::move(books);
  return true;
}

bool BibleChapterLoader::buildCache(const char* path, std::vector<BibleBookInfo>& outBooks) {
  uint64_t sourceSize = 0;
  uint32_t sourceMtime = 0;
  HalFile source;
  if (!Storage.openFileForRead("BIBLE", path, source)) {
    LOG_ERR("BIBLE", "Could not open translation file: %s", path);
    return false;
  }
  sourceSize = source.fileSize64();
  sourceMtime = source.modificationTime();

  const std::string cachePath = cachePathFor(path);
  const std::string tmpPath = cachePath + ".tmp";
  Storage.remove(tmpPath.c_str());
  HalFile out;
  const bool canWrite = Storage.openFileForWrite("BIBLE", tmpPath, out);
  if (!canWrite) LOG_ERR("BIBLE", "Could not create %s; loading without a cache", tmpPath.c_str());

  CacheHeader header{};
  const bool headerReserved = canWrite && out.write(&header, sizeof(header)) == sizeof(header);

  std::vector<BibleBookInfo> books;
  books.reserve(66);
  BuildContext ctx;
  ctx.out = &out;
  ctx.outBooks = &books;
  ctx.writeFailed = !headerReserved;
  ctx.chapters.reserve(1189);  // KJV; other Protestant-canon translations match
  ctx.chapterBlob.reserve(16 * 1024);

  const JsonCallbacks callbacks{&ctx, onBuildKey,          onBuildString,       onBuildNumber,     nullptr /*onBool*/,
                                nullptr /*onNull*/, onBuildObjectStart, onBuildObjectEnd, onBuildArrayStart,
                                onBuildArrayEnd};
  StreamingJsonParser parser(callbacks);

  std::unique_ptr<char[]> buf(new (std::nothrow) char[READ_CHUNK_SIZE]);
  if (!buf) {
    LOG_ERR("BIBLE", "OOM: read buffer (%u bytes)", static_cast<unsigned>(READ_CHUNK_SIZE));
    return false;
  }
  int n;
  while ((n = source.read(buf.get(), READ_CHUNK_SIZE)) > 0) {
    parser.feed(buf.get(), static_cast<size_t>(n));
    if (parser.hasError()) {
      LOG_ERR("BIBLE", "JSON parse error in %s", path);
      return false;
    }
  }
  if (books.empty()) {
    LOG_ERR("BIBLE", "No books found in %s", path);
    return false;
  }

  bool cached = !ctx.writeFailed && ctx.chapters.size() <= 0xFFFF;
  if (cached) {
    std::string table;
    for (const auto& book : books) {
      const auto nameLen = static_cast<uint8_t>(book.name.size() < 255 ? book.name.size() : 255);
      table.push_back(static_cast<char>(nameLen));
      table.append(book.name, 0, nameLen);
      appendU16(table, static_cast<uint16_t>(book.chapterCount));
      appendU16(table, static_cast<uint16_t>(book.firstChapterIndex));
    }
    memcpy(header.magic, CACHE_MAGIC, sizeof(CACHE_MAGIC));
    header.version = CACHE_VERSION;
    header.bookCount = static_cast<uint16_t>(books.size());
    header.sourceSize = sourceSize;
    header.sourceMtime = sourceMtime;
    header.booksOffset = ctx.dataPos;
    header.chaptersOffset = ctx.dataPos + static_cast<uint32_t>(table.size());
    header.chapterCount = static_cast<uint16_t>(ctx.chapters.size());
    const size_t chaptersLen = ctx.chapters.size() * sizeof(ChapterEntry);
    cached = out.write(table.data(), table.size()) == table.size() &&
             out.write(ctx.chapters.data(), chaptersLen) == chaptersLen && out.seek(0) &&
             out.write(&header, sizeof(header)) == sizeof(header);
  }
  if (canWrite) cached = out.close() && cached;

  if (cached) {
    Storage.remove(cachePath.c_str());
    cached = Storage.rename(tmpPath.c_str(), cachePath.c_str());
  }
  if (!cached) {
    LOG_ERR("BIBLE", "Chapter cache not written; chapters will load from JSON");
    Storage.remove(tmpPath.c_str());
    for (auto& book : books) book.firstChapterIndex = -1;
  }
  outBooks = std::move(books);
  return true;
}

bool BibleChapterLoader::loadCachedChapter(const char* path, const BibleBookInfo& book, int chapterNumber,
                                           std::vector<BibleVerse>& outVerses) {
  if (book.firstChapterIndex < 0 || chapterNumber < 1) return false;

  HalFile file;
  if (!Storage.openFileForRead("BIBLE", cachePathFor(path), file)) return false;
  CacheHeader header;
  if (!readHeader(file, header)) return false;

  // Chapters are almost always numbered 1..n in order; confirm, else search the book.
  ChapterEntry entry{};
  bool found = readChapterEntry(file, header, book.firstChapterIndex + chapterNumber - 1, entry) &&
               entry.number == chapterNumber;
  for (int i = 0; !found && i < book.chapterCount; ++i) {
    found = readChapterEntry(file, header, book.firstChapterIndex + i, entry) && entry.number == chapterNumber;
  }
  if (!found) return false;

  std::unique_ptr<uint8_t[]> blob(new (std::nothrow) uint8_t[entry.length > 0 ? entry.length : 1]);
  if (!blob || !file.seek(entry.offset) ||
      file.read(blob.get(), entry.length) != static_cast<int>(entry.length)) {
    return false;
  }

  std::vector<BibleVerse> verses;
  verses.reserve(40);
  size_t pos = 0;
  while (pos + 4 <= entry.length) {
    const int number = blob[pos] | (blob[pos + 1] << 8);
    const size_t len = blob[pos + 2] | (blob[pos + 3] << 8);
    pos += 4;
    if (pos + len > entry.length) return false;
    verses.push_back(BibleVerse{number, std::string(reinterpret_cast<const char*>(&blob[pos]), len)});
    pos += len;
  }
  if (verses.empty()) return false;
  outVerses = std::move(verses);
  return true;
}
