#pragma once

#include "activities/Activity.h"

// CrossLight's first native screen: a static placeholder Bible reader.
// Renders a hardcoded sample passage to exercise menu registration and a new
// activity end-to-end (build -> simulator -> screenshot). Translation loading,
// book/chapter navigation, and search come later; see PLAN.md.
class BibleActivity final : public Activity {
 public:
  explicit BibleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Bible", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void goBack() { finish(); }
};
