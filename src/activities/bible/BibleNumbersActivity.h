#pragma once

#include <string>
#include <vector>

#include "activities/UiListActivity.h"
#include "bible/BibleNumbers.h"

// Entry list for "Bible Numbers": one row per number file on SD, numeric order
// (7, 12, 40, 666 ...), opening BibleNumberDetailActivity on selection.
//
// Flat by design -- unlike Memory Work's course/lesson split, a number file is
// already the addressable unit, so there is no grouping level above it.
class BibleNumbersActivity final : public UiListActivity {
 public:
  BibleNumbersActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string translationPath);

  void onEnter() override;

 private:
  int listCount() const override { return static_cast<int>(rowItems.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  std::string translationPath;
  std::vector<std::string> studyPaths;
  std::vector<std::string> labels;
  std::vector<freeink::ui::ListItem> rowItems;
};
