#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "bible/BibleChapterLoader.h"

// CrossLight's first native screen: a Bible reader. Reads a getBible-format
// translation file straight off the SD card (no download required -- see
// PLAN.md's SD-first decision) and renders one chapter. Hardcoded to
// Genesis 1 of /Bible/KJV/kjv.json for now; book/chapter navigation and
// translation selection come later.
class BibleActivity final : public Activity {
 public:
  explicit BibleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Bible", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void goBack() { finish(); }

  std::vector<BibleVerse> verses;
  bool loaded = false;
};
