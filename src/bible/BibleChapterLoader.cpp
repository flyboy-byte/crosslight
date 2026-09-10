#include "BibleChapterLoader.h"

#include <HalStorage.h>
#include <Logging.h>
#include <StreamingJsonParser.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr size_t READ_CHUNK_SIZE = 512;
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

  char buf[READ_CHUNK_SIZE];
  int n;
  while (!ctx.done && (n = file.read(buf, sizeof(buf))) > 0) {
    parser.feed(buf, static_cast<size_t>(n));
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

  char buf[READ_CHUNK_SIZE];
  int n;
  while ((n = file.read(buf, sizeof(buf))) > 0) {
    parser.feed(buf, static_cast<size_t>(n));
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
