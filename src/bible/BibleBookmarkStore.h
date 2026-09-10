#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>
#include <vector>

struct BibleBookmark {
  std::string book;
  int chapter = 1;
  int page = 0;
};

// Saved Bible locations, kept in its own store for the same reason
// BibleReadingStateStore is (see that header): the Bible reader deliberately
// stays off the shared file-book bookmark plumbing, which keys bookmarks by
// EPUB spine index / xpath and has no meaning for a getBible JSON file.
//
// Order is newest-first, not canonical Bible order -- the store only knows book
// names, not their canonical index (that lives in the loaded book index), and
// "what I bookmarked most recently" is the more useful list anyway.
class BibleBookmarkStore : public PersistableStore<BibleBookmarkStore> {
  BibleBookmarkStore() = default;
  ~BibleBookmarkStore() = default;

  friend class PersistableStore<BibleBookmarkStore>;

 public:
  // Bounds the JSON document so a runaway list can't outgrow what ArduinoJson
  // can hold on-device. Oldest entries fall off the end once the cap is hit.
  static constexpr size_t MAX_BOOKMARKS = 100;

  static const char* getFilePath() { return "/.crosspoint/bible_bookmarks.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  const std::vector<BibleBookmark>& all() const { return bookmarks; }

  // -1 when the location isn't bookmarked.
  int indexOf(const std::string& book, int chapter, int page) const;

  // Adds the location if absent, removes it if present. Returns true when the
  // location is bookmarked afterward. Saves either way.
  bool toggle(const std::string& book, int chapter, int page);

  void removeAt(int index);

 private:
  std::vector<BibleBookmark> bookmarks;
};

#define BIBLE_BOOKMARKS BibleBookmarkStore::getInstance()
