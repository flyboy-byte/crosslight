#pragma once

#include <string>
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
  // rowToUtility sentinel for the synthetic Glucose Source toggle row
  // (see onEnter()) -- not a registry index, so activateIndex() must not
  // treat it as one.
  static constexpr int kGlucoseSourceToggleRow = -1;

  int listCount() const override { return static_cast<int>(rowItems.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  // Registry indices of the listed rows, so an unavailable tool being skipped
  // can't shift what a row opens. kGlucoseSourceToggleRow marks the one row
  // that isn't a utility at all.
  std::vector<int> rowToUtility;
  std::vector<freeink::ui::ListItem> rowItems;
  // Backing storage for the toggle row's subtitle -- ListItem::subtitle is a
  // raw pointer, so this has to outlive the row and get updated in place
  // rather than rebuilding rowItems on every toggle.
  std::string glucoseSourceSubtitle;
};
