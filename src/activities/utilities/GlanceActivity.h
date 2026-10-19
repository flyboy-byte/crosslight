#pragma once

#include "activities/Activity.h"

// Glance face -- the "e-reader pocket watch" resting view (PLAN.md "Glance mode
// -- product intent"). v1 is a render-only, foreground Utility: open it, see
// the time and your glucose at a glance, press any button to drop back into the
// reader. It is deliberately a plain screen now, NOT the deep-sleep resting
// state yet -- that (timed wake, fast wake path, BLE window) comes after the
// layout is agreed and deep-sleep current is measured.
//
// Glucose is shown as a representative sample in this preview: wiring the live
// source (BLE/Wi-Fi) into the face is a later step, once the layout is settled.
class GlanceActivity final : public Activity {
 public:
  GlanceActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&& lock) override;
};
