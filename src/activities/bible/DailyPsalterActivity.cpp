#include "DailyPsalterActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>

#include <cstdlib>
#include <ctime>

#include "MappedInputManager.h"
#include "bible/BiblePsalterReadings.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace dp = bible_psalter;

namespace {
constexpr int MAX_DAY_DIGITS = 2;

// Default to today's day-of-month when the RTC knows it, so opening the
// screen answers "what do I read today" immediately; otherwise day 1.
std::string defaultDay() {
  struct tm now{};
  if (halClock.isAvailable() && halClock.localTime(now)) {
    const int d = now.tm_mday;
    if (d >= 1 && d <= 31) return std::to_string(d);
  }
  return "1";
}
}  // namespace

DailyPsalterActivity::DailyPsalterActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("DailyPsalter", renderer, mappedInput) {}

void DailyPsalterActivity::onEnter() {
  Activity::onEnter();
  entry = defaultDay();
  requestUpdate();
}

int DailyPsalterActivity::day() const { return entry.empty() ? 0 : std::atoi(entry.c_str()); }

int DailyPsalterActivity::keypadTop() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int afterHeader = metrics.topPadding + metrics.headerHeight;
  const int noteH = renderer.getLineHeight(UI_10_FONT_ID) + metrics.verticalSpacing;
  const int dayH = renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing;
  // Two reading rows (label + value, each two lines) plus a footnote line.
  const int rowH = 2 * renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing;
  const int footnoteH = renderer.getLineHeight(UI_10_FONT_ID) + metrics.verticalSpacing;
  return afterHeader + noteH + dayH + 2 * rowH + footnoteH + metrics.verticalSpacing;
}

Rect DailyPsalterActivity::keypadRect(const int index) const {
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int top = keypadTop();
  const int gridHeight = safe.y + safe.height - top;
  const int cellW = renderer.getScreenWidth() / KEYPAD_COLUMNS;
  const int cellH = gridHeight / KEYPAD_ROWS;
  if (index == static_cast<int>(Key::N0)) {
    return Rect{0, top + (KEYPAD_ROWS - 1) * cellH, cellW * KEYPAD_COLUMNS, cellH};
  }
  const int row = index / KEYPAD_COLUMNS;
  const int column = index % KEYPAD_COLUMNS;
  return Rect{column * cellW, top + row * cellH, cellW, cellH};
}

int DailyPsalterActivity::keypadCellAt(const int x, const int y) const {
  for (int i = 0; i <= static_cast<int>(Key::N0); ++i) {
    const Rect r = keypadRect(i);
    if (x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height) return i;
  }
  return -1;
}

void DailyPsalterActivity::inputDigit(const char digit) {
  if (static_cast<int>(entry.size()) >= MAX_DAY_DIGITS) return;
  if (entry == "0") entry.clear();
  entry += digit;
}

void DailyPsalterActivity::pressKey(const Key key) {
  RenderLock lock(*this);
  switch (key) {
    case Key::N0:
    case Key::N1:
    case Key::N2:
    case Key::N3:
    case Key::N4:
    case Key::N5:
    case Key::N6:
    case Key::N7:
    case Key::N8:
    case Key::N9: {
      static const char kDigit[] = {'7', '8', '9', '0', '4', '5', '6', '0', '1', '2', '3', '0'};
      inputDigit(kDigit[static_cast<int>(key)]);
      break;
    }
    case Key::Back:
      if (!entry.empty()) entry.pop_back();
      break;
    case Key::Clear:
      entry.clear();
      break;
  }
  requestUpdate();
}

void DailyPsalterActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  int x = 0;
  int y = 0;
  if (!mappedInput.wasScreenTapped(x, y)) return;
  const int cell = keypadCellAt(x, y);
  if (cell >= 0) pressKey(static_cast<Key>(cell));
}

void DailyPsalterActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pad = metrics.contentSidePadding;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_DAILY_PSALTER));

  int y = metrics.topPadding + metrics.headerHeight;

  renderer.drawText(UI_10_FONT_ID, pad, y, tr(STR_DP_NOTE));
  y += renderer.getLineHeight(UI_10_FONT_ID) + metrics.verticalSpacing;

  const int dayNum = day();
  const std::string shown = entry.empty() ? "--" : entry;
  const std::string dayLine = std::string(tr(STR_DP_DAY)) + " " + shown;
  const int dw = renderer.getTextWidth(UI_12_FONT_ID, dayLine.c_str());
  renderer.drawText(UI_12_FONT_ID, (pageWidth - dw) / 2, y, dayLine.c_str(), true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing;

  dp::DayReadings readings;
  const bool valid = dayNum > 0 && dp::readingsForDay(dayNum, readings);

  auto drawReading = [&](const char* label, const dp::Reading& reading) {
    renderer.drawText(UI_10_FONT_ID, pad, y, label);
    y += renderer.getLineHeight(UI_10_FONT_ID);
    const std::string value = reading.label();
    renderer.drawText(UI_12_FONT_ID, pad, y, value.c_str(), true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing;
  };

  if (valid) {
    drawReading(tr(STR_DP_MORNING), readings.morning);
    drawReading(tr(STR_DP_EVENING), readings.evening);
  } else {
    y += 2 * (renderer.getLineHeight(UI_10_FONT_ID) + renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing);
  }

  if (dayNum == 31) {
    renderer.drawText(UI_10_FONT_ID, pad, y, tr(STR_DP_DAY31_NOTE));
  }

  // Keypad: 7 8 9 <-, 4 5 6 C, 1 2 3, and 0 spanning the bottom row.
  static const char* const kLabels[] = {"7", "8", "9", "<", "4", "5", "6", "C", "1", "2", "3", "0"};
  for (int i = 0; i <= static_cast<int>(Key::N0); ++i) {
    const Rect r = keypadRect(i);
    renderer.drawRect(r.x, r.y, r.width, r.height);
    const int lw = renderer.getTextWidth(UI_12_FONT_ID, kLabels[i]);
    renderer.drawText(UI_12_FONT_ID, r.x + (r.width - lw) / 2,
                      r.y + (r.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2, kLabels[i]);
  }

  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
