#include "GlanceWake.h"

#include <GfxRenderer.h>

#include <cstdio>

#include "fontIds.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_attr.h>
#include <esp_sleep.h>
#endif

namespace glance {

namespace {
constexpr uint32_t kMagic = 0x474C4E43;  // "GLNC"
#if defined(ARDUINO_ARCH_ESP32)
RTC_NOINIT_ATTR Stats g_stats;
#else
Stats g_stats;
#endif
}  // namespace

Stats& stats() {
  if (g_stats.magic != kMagic) {
    g_stats = Stats{};
    g_stats.magic = kMagic;
    g_stats.value = 0xFFFF;
  }
  return g_stats;
}

void armFirst(const uint8_t batteryPct, const int64_t nowEpoch) {
  Stats& s = stats();
  s = Stats{};
  s.magic = kMagic;
  s.value = 0xFFFF;
  s.armedBattery = batteryPct;
  s.armedEpoch = nowEpoch;
  armNext();
}

void armNext() {
#if defined(ARDUINO_ARCH_ESP32)
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(kIntervalS) * 1000000ULL);
#endif
}

bool isGlanceWake() {
#if defined(ARDUINO_ARCH_ESP32)
  return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER && g_stats.magic == kMagic && g_stats.armedBattery != 0;
#else
  return false;
#endif
}

void drawOverlay(GfxRenderer& renderer, const FaceData& d) {
  const int w = renderer.getScreenWidth();
  const int h = renderer.getScreenHeight();
  const int boxW = w - 80;
  const int timeH = renderer.getLineHeight(NOTOSANS_14_FONT_ID);
  const int smallH = renderer.getLineHeight(UI_12_FONT_ID);
  const int boxH = timeH * 2 + smallH * 2 + 90;
  const int bx = (w - boxW) / 2;
  const int by = (h - boxH) / 2;

  renderer.fillRect(bx, by, boxW, boxH, false);
  renderer.drawRect(bx, by, boxW, boxH, 3, true);

  auto center = [&](int fontId, int y, const char* text, bool bold) {
    const EpdFontFamily::Style style = bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    const int tw = renderer.getTextWidth(fontId, text, style);
    renderer.drawText(fontId, bx + (boxW - tw) / 2, y, text, true, style);
  };

  int y = by + 22;
  center(NOTOSANS_14_FONT_ID, y, d.time, true);
  y += timeH + 6;

  char line[48];
  snprintf(line, sizeof(line), "%s  %u%%", d.date, static_cast<unsigned>(d.batteryPct));
  center(UI_12_FONT_ID, y, line, false);
  y += smallH + 18;
  renderer.drawLine(bx + 20, y, bx + boxW - 20, y, 2, true);
  y += 18;

  if (d.hasGlucose) {
    snprintf(line, sizeof(line), "%d %s", d.value, d.trend);
    center(NOTOSANS_14_FONT_ID, y, line, true);
    y += timeH + 6;
    snprintf(line, sizeof(line), "mg/dL  -  %d min ago", d.ageMin);
    center(UI_12_FONT_ID, y, line, false);
  } else {
    center(UI_12_FONT_ID, y, "no recent reading", false);
  }
}

}  // namespace glance
