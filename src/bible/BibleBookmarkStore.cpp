#include "BibleBookmarkStore.h"

void BibleBookmarkStore::toJson(JsonDocument& doc) const {
  JsonArray arr = doc["bookmarks"].to<JsonArray>();
  for (const auto& bookmark : bookmarks) {
    JsonObject obj = arr.add<JsonObject>();
    obj["book"] = bookmark.book;
    obj["chapter"] = bookmark.chapter;
    obj["page"] = bookmark.page;
  }
}

bool BibleBookmarkStore::fromJson(JsonVariantConst doc) {
  bookmarks.clear();
  for (JsonVariantConst entry : doc["bookmarks"].as<JsonArrayConst>()) {
    if (bookmarks.size() >= MAX_BOOKMARKS) break;
    // const char*, not std::string -- see PersistableStore's note on the
    // per-TU serializer copy the std::string converter drags in.
    const char* book = entry["book"] | "";
    if (*book == '\0') continue;
    bookmarks.push_back(BibleBookmark{book, entry["chapter"] | 1, entry["page"] | 0});
  }
  return true;
}

int BibleBookmarkStore::indexOf(const std::string& book, const int chapter, const int page) const {
  for (size_t i = 0; i < bookmarks.size(); ++i) {
    if (bookmarks[i].book == book && bookmarks[i].chapter == chapter && bookmarks[i].page == page) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

bool BibleBookmarkStore::toggle(const std::string& book, const int chapter, const int page) {
  const int existing = indexOf(book, chapter, page);
  if (existing >= 0) {
    removeAt(existing);
    return false;
  }
  bookmarks.insert(bookmarks.begin(), BibleBookmark{book, chapter, page});
  if (bookmarks.size() > MAX_BOOKMARKS) bookmarks.resize(MAX_BOOKMARKS);
  saveToFile();
  return true;
}

void BibleBookmarkStore::removeAt(const int index) {
  if (index < 0 || index >= static_cast<int>(bookmarks.size())) return;
  bookmarks.erase(bookmarks.begin() + index);
  saveToFile();
}
