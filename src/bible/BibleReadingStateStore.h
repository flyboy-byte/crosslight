#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>

// Persists where the user left off reading the Bible (book/chapter/page), so
// reopening the app resumes there instead of always starting at Genesis 1.
// Kept as its own tiny store rather than added to CrossPointState -- see
// PLAN.md's coupling stance: the Bible reader deliberately avoids touching
// shared app state (openEpubPath/RecentBooksStore/EndOfBookOptions all assume
// file-book semantics that don't apply here).
class BibleReadingStateStore : public PersistableStore<BibleReadingStateStore> {
  BibleReadingStateStore() = default;
  ~BibleReadingStateStore() = default;

  friend class PersistableStore<BibleReadingStateStore>;

 public:
  std::string bookName;
  int chapter = 1;
  int page = 0;
  // getBible abbreviation of the selected translation ("kjv"); empty = default.
  // See BibleTranslations::current().
  std::string translation;

  static const char* getFilePath() { return "/.crosspoint/bible_state.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  // Empty bookName means "no saved position yet" (first run, or file absent).
  bool hasSavedPosition() const { return !bookName.empty(); }

  void save(const std::string& book, int chapterNum, int pageNum);
};

#define BIBLE_READING_STATE BibleReadingStateStore::getInstance()
