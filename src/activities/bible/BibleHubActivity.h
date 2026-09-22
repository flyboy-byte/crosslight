#pragma once

#include <string>
#include <vector>

#include "activities/UiListActivity.h"

// The Bible's landing screen off Home: Continue Reading (pre-selected, so resuming
// stays one press), Select Book, Go to Verse, Search, Bookmarks, Translations.
// Reader rows replace this screen with the reader plus an initial action; the
// reader's Back comes back here.
class BibleHubActivity final : public UiListActivity {
 public:
  BibleHubActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;

 private:
  enum Row : int16_t { Continue, SelectBook, GoToVerse, Search, Bookmarks, Translations };

  int listCount() const override { return static_cast<int>(rowItems.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void buildRows();

  std::string continueSubtitle;
  std::string bookmarkCount;
  std::string translationLabel;
  std::vector<freeink::ui::ListItem> rowItems;
};
