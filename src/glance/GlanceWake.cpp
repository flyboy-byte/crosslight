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
  // Two slim strips, time on top and glucose on the bottom, so the middle of the
  // sleep wallpaper stays clear whatever wallpaper is chosen.
  const int w = renderer.getScreenWidth();
  const int h = renderer.getScreenHeight();
  const int lineH = renderer.getLineHeight(UI_12_FONT_ID);
  const int stripH = lineH + 20;
  const int pad = 18;

  auto strip = [&](const int y) {
    renderer.fillRect(0, y, w, stripH, false);
  };
  const int textY = (stripH - lineH) / 2;

  // Header: time centred, battery at the right edge.
  strip(0);
  renderer.drawLine(0, stripH, w, stripH, 2, true);
  {
    const int tw = renderer.getTextWidth(UI_12_FONT_ID, d.time, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, (w - tw) / 2, textY, d.time, true, EpdFontFamily::BOLD);
    char batt[8];
    snprintf(batt, sizeof(batt), "%u%%", static_cast<unsigned>(d.batteryPct));
    const int bw = renderer.getTextWidth(UI_12_FONT_ID, batt);
    renderer.drawText(UI_12_FONT_ID, w - pad - bw, textY, batt);
  }

  // Footer: latest glucose, trend and age, centred.
  const int footY = h - stripH;
  strip(footY);
  renderer.drawLine(0, footY, w, footY, 2, true);
  char line[48];
  if (d.hasGlucose) {
    snprintf(line, sizeof(line), "%d mg/dL %s  -  %d min ago", d.value, d.trend, d.ageMin);
  } else {
    snprintf(line, sizeof(line), "no recent reading");
  }
  const int lw = renderer.getTextWidth(UI_12_FONT_ID, line, EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID, (w - lw) / 2, footY + textY + 2, line, true, EpdFontFamily::BOLD);
}

}  // namespace glance
