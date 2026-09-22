#pragma once

#include <string>
#include <vector>

#include "activities/UiListActivity.h"

// Installed translations (tap to select) followed by the English presets not yet on
// the card (tap to download over Wi-Fi, after a confirm that shows the license).
class BibleTranslationsActivity final : public UiListActivity {
 public:
  BibleTranslationsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;

 private:
  int listCount() const override { return static_cast<int>(rowItems.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void buildRows();
  void confirmDownload(const std::string& abbr);

  std::string currentAbbr;
  std::vector<std::string> abbrs;   // per row
  std::vector<std::string> labels;  // per row, backing ListItem strings
  std::vector<freeink::ui::ListItem> rowItems;
  size_t installedCount = 0;
};
