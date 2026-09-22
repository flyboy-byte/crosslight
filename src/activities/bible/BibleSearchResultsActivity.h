#pragma once

#include <string>
#include <vector>

#include "activities/UiListActivity.h"
#include "bible/BibleChapterLoader.h"

// Results of a Bible full-text search: one row per matching verse (reference plus a
// snippet). Returns the chosen verse as a BibleVerseResult.
class BibleSearchResultsActivity final : public UiListActivity {
 public:
  BibleSearchResultsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string query,
                             std::vector<BibleChapterLoader::SearchHit> hits, bool truncated);

  void onEnter() override;

 private:
  int listCount() const override { return static_cast<int>(rowItems.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override { return title.c_str(); }

  std::string title;
  std::vector<BibleChapterLoader::SearchHit> hits;
  bool truncated;
  std::vector<std::string> labels;
  std::vector<freeink::ui::ListItem> rowItems;
};
