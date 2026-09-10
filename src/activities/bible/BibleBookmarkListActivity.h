#pragma once

#include <string>
#include <vector>

#include "activities/UiListActivity.h"

// Lists saved Bible bookmarks (newest first, as BibleBookmarkStore keeps them)
// and returns the chosen one as a BibleBookmarkResult.
//
// Deletion is deliberately not here: it lives on the reader's menu as a toggle
// against the current location. That keeps this screen a pure picker and keeps
// deletion reachable without touch, which a long-press row action would not be.
class BibleBookmarkListActivity final : public UiListActivity {
 public:
  BibleBookmarkListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;

 private:
  int listCount() const override { return static_cast<int>(rowItems.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  std::vector<std::string> labels;
  std::vector<std::string> pageLabels;
  std::vector<freeink::ui::ListItem> rowItems;
};
