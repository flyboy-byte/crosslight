#include "HistoricalCalendarActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>

#include <cstdlib>
#include <ctime>

#include "MappedInputManager.h"
#include "bible/HistoricalCalendar.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace hc = historical_calendar;

namespace {
constexpr int MAX_YEAR_DIGITS = 4;

// Default the screen to the current year when the RTC knows it, so the common
// question ("when is Easter this year?") is answered on open; otherwise fall
// back to 1611, the almanac this feature reconstructs.
std::string defaultYear() {
  struct tm now{};
  if (halClock.isAvailable() && halClock.localTime(now)) {
    const int y = now.tm_year + 1900;
    if (y > 0 && y < 10000) return std::to_string(y);
  }
  return "1611";
}
}  // namespace

HistoricalCalendarActivity::HistoricalCalendarActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("HistoricalCalendar", renderer, mappedInput) {}

void HistoricalCalendarActivity::onEnter() {
  Activity::onEnter();
  entry = defaultYear();
  requestUpdate();
}

int HistoricalCalendarActivity::year() const { return entry.empty() ? 0 : std::atoi(entry.c_str()); }

int HistoricalCalendarActivity::keypadTop() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  // header + note line + the big year line + five result rows.
  const int afterHeader = metrics.topPadding + metrics.headerHeight;
  const int noteH = renderer.getLineHeight(UI_10_FONT_ID) + metrics.verticalSpacing;
  const int yearH = renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing;
  const int rowsH = 5 * (renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing / 2);
  return afterHeader + noteH + yearH + rowsH + metrics.verticalSpacing;
}

Rect HistoricalCalendarActivity::keypadRect(const int index) const {
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int top = keypadTop();
  const int gridHeight = safe.y + safe.height - top;
  const int cellW = renderer.getScreenWidth() / KEYPAD_COLUMNS;
  const int cellH = gridHeight / KEYPAD_ROWS;
  // N0 spans the full bottom row; the other keys fill the three rows above it.
  if (index == static_cast<int>(Key::N0)) {
    return Rect{0, top + (KEYPAD_ROWS - 1) * cellH, cellW * KEYPAD_COLUMNS, cellH};
  }
  const int row = index / KEYPAD_COLUMNS;
  const int column = index % KEYPAD_COLUMNS;
  return Rect{column * cellW, top + row * cellH, cellW, cellH};
}

int HistoricalCalendarActivity::keypadCellAt(const int x, const int y) const {
  for (int i = 0; i <= static_cast<int>(Key::N0); ++i) {
    const Rect r = keypadRect(i);
    if (x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height) return i;
  }
  return -1;
}

void HistoricalCalendarActivity::inputDigit(const char digit) {
  if (static_cast<int>(entry.size()) >= MAX_YEAR_DIGITS) return;
  if (entry == "0") entry.clear();
  entry += digit;
}

void HistoricalCalendarActivity::pressKey(const Key key) {
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

void HistoricalCalendarActivity::loop() {
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

void HistoricalCalendarActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pad = metrics.contentSidePadding;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_HISTORICAL_CALENDAR));

  int y = metrics.topPadding + metrics.headerHeight;

  // One-line framing note.
  renderer.drawText(UI_10_FONT_ID, pad, y, tr(STR_HC_NOTE));
  y += renderer.getLineHeight(UI_10_FONT_ID) + metrics.verticalSpacing;

  // The year under edit, centered and bold.
  const std::string shown = entry.empty() ? "----" : entry;
  const int yw = renderer.getTextWidth(UI_12_FONT_ID, shown.c_str());
  renderer.drawText(UI_12_FONT_ID, (pageWidth - yw) / 2, y, shown.c_str(), true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing;

  // Computus results: label left, value right-aligned.
  const int yr = year();
  const int rowH = renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing / 2;
  auto drawRow = [&](const char* label, const std::string& value) {
    renderer.drawText(UI_10_FONT_ID, pad, y, label);
    const int vw = renderer.getTextWidth(UI_12_FONT_ID, value.c_str());
    renderer.drawText(UI_12_FONT_ID, pageWidth - pad - vw, y, value.c_str());
    y += rowH;
  };

  if (yr > 0) {
    drawRow(tr(STR_HC_GOLDEN_NUMBER), std::to_string(hc::goldenNumber(yr)));
    drawRow(tr(STR_HC_EPACT), std::to_string(hc::julianEpact(yr)));
    drawRow(tr(STR_HC_DOMINICAL_LETTER), hc::gregorianDominicalLetter(yr));
    drawRow(tr(STR_HC_EASTER_OLD_STYLE), hc::formatDate(hc::julianEaster(yr)));
    drawRow(tr(STR_HC_EASTER_NEW_STYLE), hc::formatDate(hc::gregorianEaster(yr)));
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
