#pragma once

#include <cstdint>

#include "activities/Activity.h"

// Turns the X4 Pro's frontlight into a flashlight: enter → full brightness on,
// tap anywhere to toggle, Back restores whatever the frontlight was before.
// The e-ink panel is reflective (a white screen emits nothing), so the
// frontlight itself is the light source here, not the display.
class FlashlightActivity final : public Activity {
 public:
  FlashlightActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  // Hold off auto-sleep while lit, or the light would die on its own.
  bool preventAutoSleep() override { return lit; }

 private:
  void setLit(bool on);

  bool hasHardware = false;
  bool lit = false;

  // Frontlight state captured on entry, restored on exit.
  bool priorOn = false;
  uint8_t priorBrightness = 0;
  uint8_t priorWarmth = 0;
};
