#pragma once

#include <functional>
#include <string>

#include "activities/Activity.h"

// The passphrase prompt shown before the UI is reachable at boot, and on wake
// when that is switched on.
//
// It stands in front of the normal boot routing rather than on top of it: main
// hands over what it would otherwise have done, and this runs that only once
// the passphrase checks out. Layering it over an already-routed screen would
// paint the book or Home underneath first, which on e-ink is a visible flash of
// the thing being hidden.
//
// Back does not leave. There is no route past this screen except the
// passphrase (or reflashing the device, which was always true -- see
// DeviceLock's header on what this does and does not protect).
class LockScreenActivity final : public Activity {
 public:
  LockScreenActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::function<void()> onUnlocked);

  void onEnter() override;
  void render(RenderLock&& lock) override;
  bool preventAutoSleep() override { return false; }

 private:
  void prompt();

  std::function<void()> onUnlocked;
  bool wrong = false;
};
