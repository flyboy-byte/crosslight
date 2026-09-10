#pragma once

#include <string>
#include <vector>

#include "activities/UiListActivity.h"

// Lists chapters 1..chapterCount for a book already chosen via
// BibleBookSelectionActivity and returns the pick as a BibleChapterResult.
// Purely numeric (no SD access needed) -- the caller already has the count
// from the book index scan.
class BibleChapterSelectionActivity final : public UiListActivity {
 public:
  BibleChapterSelectionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string bookName,
                                 int chapterCount, int currentChapter);

  void onEnter() override;

 private:
  int listCount() const override { return chapterCount; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  std::string bookName;
  int chapterCount;
  int currentChapter;
  std::vector<std::string> labels;
  std::vector<freeink::ui::ListItem> rowItems;
};
