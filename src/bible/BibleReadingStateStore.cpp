#include "BibleReadingStateStore.h"

void BibleReadingStateStore::toJson(JsonDocument& doc) const {
  doc["book"] = bookName;
  doc["chapter"] = chapter;
  doc["page"] = page;
  doc["translation"] = translation;
}

bool BibleReadingStateStore::fromJson(JsonVariantConst doc) {
  bookName = doc["book"] | "";
  chapter = doc["chapter"] | 1;
  page = doc["page"] | 0;
  translation = doc["translation"] | "";
  return true;
}

void BibleReadingStateStore::save(const std::string& book, int chapterNum, int pageNum) {
  bookName = book;
  chapter = chapterNum;
  page = pageNum;
  saveToFile();
}
