#include "FlashlightActivity.h"

#include <GfxRenderer.h>
#include <HalFrontlight.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

FlashlightActivity::FlashlightActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Flashlight", renderer, mappedInput) {}

void FlashlightActivity::onEnter() {
  Activity::onEnter();
  hasHardware = Frontlight.present();
  if (hasHardware) {
    priorOn = Frontlight.isOn();
    priorBrightness = Frontlight.brightness();
    priorWarmth = Frontlight.warmth();
    setLit(true);  // open straight into light-on, the thing you came here for
  }
  requestUpdate();
}

void FlashlightActivity::onExit() {
  if (hasHardware) {
    // Put the frontlight back exactly as it was before the flashlight ran.
    Frontlight.setBrightness(priorBrightness);
    if (Frontlight.hasColorTemperature()) Frontlight.setWarmth(priorWarmth);
    Frontlight.setOn(priorOn);
  }
  Activity::onExit();
}

void FlashlightActivity::setLit(const bool on) {
  lit = on;
  if (on) Frontlight.setBrightness(100);  // full output; this is a flashlight
  Frontlight.setOn(on);
}

void FlashlightActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  int x = 0;
  int y = 0;
  if (hasHardware && mappedInput.wasScreenTapped(x, y)) {
    RenderLock lock(*this);
    setLit(!lit);
    requestUpdate();
  }
}

void FlashlightActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_FLASHLIGHT));

  if (!hasHardware) {
    const char* msg = tr(STR_FLASHLIGHT_NO_HW);
    const int w = renderer.getTextWidth(UI_12_FONT_ID, msg);
    renderer.drawText(UI_12_FONT_ID, (pageWidth - w) / 2, pageHeight / 2, msg);
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    return;
  }

  // Big centered state word, with the toggle hint beneath it.
  const char* state = lit ? "ON" : "OFF";
  const int stateW = renderer.getTextWidth(UI_12_FONT_ID, state);
  const int stateY = pageHeight / 2 - renderer.getLineHeight(UI_12_FONT_ID);
  renderer.drawText(UI_12_FONT_ID, (pageWidth - stateW) / 2, stateY, state, true, EpdFontFamily::BOLD);

  const char* hint = tr(STR_FLASHLIGHT_TAP);
  const int hintW = renderer.getTextWidth(UI_10_FONT_ID, hint);
  renderer.drawText(UI_10_FONT_ID, (pageWidth - hintW) / 2, stateY + renderer.getLineHeight(UI_12_FONT_ID) * 2, hint);

  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
