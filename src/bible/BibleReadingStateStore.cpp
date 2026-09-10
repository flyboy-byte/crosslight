#include "BibleReadingStateStore.h"

void BibleReadingStateStore::toJson(JsonDocument& doc) const {
  doc["book"] = bookName;
  doc["chapter"] = chapter;
  doc["page"] = page;
}

bool BibleReadingStateStore::fromJson(JsonVariantConst doc) {
  bookName = doc["book"] | "";
  chapter = doc["chapter"] | 1;
  page = doc["page"] | 0;
  return true;
}

void BibleReadingStateStore::save(const std::string& book, int chapterNum, int pageNum) {
  bookName = book;
  chapter = chapterNum;
  page = pageNum;
  saveToFile();
}
