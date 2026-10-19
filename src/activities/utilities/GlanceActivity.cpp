#include "GlanceActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalPowerManager.h>
#include <I18n.h>

#include <cstdio>
#include <ctime>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

GlanceActivity::GlanceActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Glance", renderer, mappedInput) {}

void GlanceActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void GlanceActivity::loop() {
  // Any deliberate press is the escape hatch back to the reader (principle 4).
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    finish();
    return;
  }
  int x = 0;
  int y = 0;
  if (mappedInput.wasScreenTapped(x, y)) finish();
}

void GlanceActivity::render(RenderLock&&) {
  const int w = renderer.getScreenWidth();
  const int h = renderer.getScreenHeight();
  renderer.clearScreen();

  auto centerText = [&](int fontId, int y, const char* text, bool bold) {
    const EpdFontFamily::Style style = bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    const int tw = renderer.getTextWidth(fontId, text, style);
    renderer.drawText(fontId, (w - tw) / 2, y, text, true, style);
  };

  // ---- Top strip: date (left) and battery (right) ----
  struct tm now;
  const bool haveTime = halClock.localTime(now);
  char dateBuf[32] = "";
  char timeBuf[8] = "--:--";
  if (haveTime) {
    strftime(dateBuf, sizeof(dateBuf), "%a %b %-d", &now);
    strftime(timeBuf, sizeof(timeBuf), "%H:%M", &now);
  }
  const int pad = 18;
  renderer.drawText(UI_12_FONT_ID, pad, 20, dateBuf);
  char battBuf[8];
  snprintf(battBuf, sizeof(battBuf), "%u%%", powerManager.getBatteryPercentage());
  const int battW = renderer.getTextWidth(UI_12_FONT_ID, battBuf);
  renderer.drawText(UI_12_FONT_ID, w - pad - battW, 20, battBuf);

  // ---- Hero: time, as large as the guaranteed fonts allow ----
  int y = 150;
  centerText(NOTOSANS_14_FONT_ID, y, timeBuf, /*bold=*/true);
  y += renderer.getLineHeight(NOTOSANS_14_FONT_ID) + 60;

  // ---- Divider ----
  renderer.drawLine(pad, y, w - pad, y, 2, true);
  y += 50;

  // ---- Glucose block (representative sample in this preview) ----
  const int glucoseValue = 112;        // sample
  const char* glucoseUnit = "mg/dL";   // sample
  const char* trend = "^";             // sample: rising
  const char* ageLine = "2 min ago";   // sample: receipt age

  char valBuf[8];
  snprintf(valBuf, sizeof(valBuf), "%d", glucoseValue);
  // Number + unit + trend on one baseline, centered as a group.
  const int numW = renderer.getTextWidth(NOTOSANS_14_FONT_ID, valBuf, EpdFontFamily::BOLD);
  const int unitW = renderer.getTextWidth(UI_12_FONT_ID, glucoseUnit);
  const int trendW = renderer.getTextWidth(NOTOSANS_14_FONT_ID, trend, EpdFontFamily::BOLD);
  const int gap = 12;
  const int groupW = numW + gap + unitW + gap + trendW;
  int gx = (w - groupW) / 2;
  const int numH = renderer.getLineHeight(NOTOSANS_14_FONT_ID);
  renderer.drawText(NOTOSANS_14_FONT_ID, gx, y, valBuf, true, EpdFontFamily::BOLD);
  gx += numW + gap;
  renderer.drawText(UI_12_FONT_ID, gx, y + (numH - renderer.getLineHeight(UI_12_FONT_ID)) - 4, glucoseUnit);
  gx += unitW + gap;
  renderer.drawText(NOTOSANS_14_FONT_ID, gx, y, trend, true, EpdFontFamily::BOLD);
  y += numH + 10;
  centerText(UI_10_FONT_ID, y, ageLine, false);

  // ---- Bottom hint ----
  centerText(SMALL_FONT_ID, h - 44, tr(STR_GLANCE_HINT), false);

  renderer.displayBuffer(HalDisplay::FULL_REFRESH);
}
