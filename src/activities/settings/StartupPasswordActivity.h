#pragma once

#include <string>
#include <vector>

#include "activities/UiListActivity.h"

// Settings > System > Startup Password: set or change the passphrase, switch
// the lock on and off, and choose whether waking asks for it too.
//
// Changing or removing the passphrase asks for the current one first, so
// someone who picks up an unlocked device cannot quietly turn the lock off.
class StartupPasswordActivity final : public UiListActivity {
 public:
  StartupPasswordActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;

 private:
  enum Row : int16_t { SetPassphrase, LockOnWake, Remove };

  int listCount() const override { return static_cast<int>(rowItems.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void buildRows();

  // Prompts for the current passphrase (when one is set) and runs `next` only
  // if it matches.
  void withCurrentPassphrase(std::function<void()> next);
  void promptNewPassphrase();

  std::vector<freeink::ui::ListItem> rowItems;
};
