#pragma once

#include <I18n.h>

#include <memory>
#include <vector>

#include "components/themes/BaseTheme.h"

class Activity;
class GfxRenderer;
class MappedInputManager;

// The registry behind Home's "Utilities" entry.
//
// Home's menu is a hand-maintained enum <-> index mapping (HomeActivity's
// menuItemToIndex/indexToMenuItem plus the item and icon lists), so every app
// added there costs three edits to a file upstream touches often -- the
// 2026-09-16 merge already conflicted exactly there. Utilities therefore take
// ONE Home slot between them: a new tool is one entry in kUtilities, and Home
// never changes again.
//
// Each utility is constructed only when its row is opened and destroyed on
// exit, so a tool costs flash and nothing else until it is used. Tools that
// need the radio bring it up themselves and tear it down on exit, the way
// BibleDownloadActivity does -- nothing here runs in the background.
namespace utilities {

struct Utility {
  StrId title;
  UIIcon icon = Blocks;
  std::unique_ptr<Activity> (*create)(GfxRenderer&, MappedInputManager&) = nullptr;
  // Optional gate for a tool that needs hardware or files that may be absent.
  // Null means always listed.
  bool (*available)() = nullptr;
};

// Every registered utility, in menu order, including unavailable ones.
const std::vector<Utility>& all();

}  // namespace utilities
