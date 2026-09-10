#pragma once

#include <string>
#include <vector>

#include "activities/UiListActivity.h"
#include "bible/BibleChapterLoader.h"

// Lists the books found in a getBible translation file (see
// BibleChapterLoader::loadBookIndex) and returns the chosen one as a
// BibleBookResult. Book count is small enough (~66-80) that, unlike the EPUB
// TOC picker, all rows are built once in onEnter -- no viewport windowing.
class BibleBookSelectionActivity final : public UiListActivity {
 public:
  BibleBookSelectionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string translationPath);

  void onEnter() override;

 private:
  int listCount() const override { return static_cast<int>(books.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  std::string translationPath;
  std::vector<BibleBookInfo> books;
  std::vector<std::string> labels;
  std::vector<freeink::ui::ListItem> rowItems;
  bool loaded = false;
};
