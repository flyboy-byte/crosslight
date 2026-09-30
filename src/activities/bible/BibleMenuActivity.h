#pragma once

#include <string>
#include <vector>

#include "activities/UiListActivity.h"

// Action menu for the Bible reader, opened with Confirm.
//
// Exists because the reader's four buttons are all spoken for (Back / Select /
// previous page / next page), so bookmarks had nowhere to bind. Confirm used to
// open the book picker directly; it now opens this, and "Select Book" is the
// first row so that path stays one extra press away.
//
// Returns the chosen action as a MenuResult (no Bible-specific result type
// needed); the reader maps it back through BibleMenuActivity::Action.
class BibleMenuActivity final : public UiListActivity {
 public:
  enum Action : int {
    GoToBook = 0,
    OpenBookmarks = 1,
    ToggleBookmark = 2,
  };

  BibleMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool currentLocationBookmarked,
                    int bookmarkCount);

  void onEnter() override;

 private:
  int listCount() const override { return static_cast<int>(rowItems.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  bool currentLocationBookmarked;
  int bookmarkCount;
  std::vector<std::string> labels;
  std::vector<freeink::ui::ListItem> rowItems;
};
