#pragma once

#include <vector>

#include "activities/UiListActivity.h"

// Home's "Utilities" screen: one row per registered utility (see
// utilities::all()). Opening a row constructs that tool; nothing is built or
// started until then.
class UtilitiesActivity final : public UiListActivity {
 public:
  UtilitiesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;

 private:
  int listCount() const override { return static_cast<int>(rowItems.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  // Registry indices of the listed rows, so an unavailable tool being skipped
  // can't shift what a row opens.
  std::vector<int> rowToUtility;
  std::vector<freeink::ui::ListItem> rowItems;
};
