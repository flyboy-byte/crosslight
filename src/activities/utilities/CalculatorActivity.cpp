#include "CalculatorActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cmath>
#include <cstdio>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Row-major labels for the 4x5 grid, matching CalculatorActivity::Key order.
const char* const KEY_LABELS[] = {"7",  "8", "9", "/", "4", "5", "6", "*", "1", "2",
                                  "3",  "-", ".", "0", "+/-", "+", "C", "<", "="};
constexpr int KEY_COUNT = sizeof(KEY_LABELS) / sizeof(KEY_LABELS[0]);

// Digits a double can carry before the printed form starts inventing
// precision. %.10g keeps 1/3 readable without a tail of noise.
constexpr int MAX_ENTRY_DIGITS = 12;

std::string formatNumber(const double value) {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%.10g", value);
  return buffer;
}
}  // namespace

CalculatorActivity::CalculatorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Calculator", renderer, mappedInput) {}

void CalculatorActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

// The grid fills the screen below the display band. The last row is three keys
// wide (C, backspace, =), with = taking the double-width slot.
Rect CalculatorActivity::keyRect(const int index) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int gridTop = safe.y + metrics.topPadding + metrics.headerHeight + renderer.getLineHeight(UI_12_FONT_ID) * 2;
  const int gridHeight = safe.y + safe.height - gridTop;
  const int cellW = renderer.getScreenWidth() / COLUMNS;
  const int cellH = gridHeight / ROWS;

  const int row = index / COLUMNS;
  const int column = index % COLUMNS;
  const int width = index == static_cast<int>(Key::Equals) ? cellW * 2 : cellW;
  return Rect{column * cellW, gridTop + row * cellH, width, cellH};
}

void CalculatorActivity::setEntry(const double value) {
  if (std::isnan(value) || std::isinf(value)) {
    error = true;
    entry = tr(STR_CALC_ERROR);
    return;
  }
  entry = formatNumber(value);
  entryIsResult = true;
}

void CalculatorActivity::inputDigit(const char digit) {
  if (error) {
    error = false;
    entry = "0";
    entryIsResult = true;
  }
  if (digit == '.') {
    if (entryIsResult) {
      entry = "0.";
      entryIsResult = false;
      return;
    }
    if (entry.find('.') == std::string::npos) entry += '.';
    return;
  }
  if (entryIsResult) {
    entry.clear();
    entryIsResult = false;
  }
  if (static_cast<int>(entry.size()) >= MAX_ENTRY_DIGITS) return;
  if (entry == "0") entry.clear();
  entry += digit;
  if (entry.empty()) entry = "0";
}

// Folds the pending operation into the accumulator. Called by an operator key
// and by =, which is what makes 2 + 3 x 4 evaluate left to right.
void CalculatorActivity::applyPending() {
  const double value = std::strtod(entry.c_str(), nullptr);
  if (!hasPending) {
    accumulator = value;
    return;
  }
  switch (pendingOp) {
    case Key::Add:
      accumulator += value;
      break;
    case Key::Sub:
      accumulator -= value;
      break;
    case Key::Mul:
      accumulator *= value;
      break;
    case Key::Div:
      // Division by zero would give inf; setEntry turns that into the error
      // display rather than printing "inf".
      accumulator /= value;
      break;
    default:
      accumulator = value;
      break;
  }
}

void CalculatorActivity::press(const Key key) {
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
    case Key::N9:
      inputDigit(KEY_LABELS[static_cast<int>(key)][0]);
      break;
    case Key::Dot:
      inputDigit('.');
      break;
    case Key::Sign:
      if (entry != "0" && !error) {
        if (entry[0] == '-') {
          entry.erase(0, 1);
        } else {
          entry.insert(entry.begin(), '-');
        }
      }
      break;
    case Key::Clear:
      entry = "0";
      accumulator = 0;
      hasPending = false;
      entryIsResult = true;
      error = false;
      break;
    case Key::Back:
      if (error || entryIsResult) break;
      entry.pop_back();
      if (entry.empty() || entry == "-") entry = "0";
      break;
    case Key::Add:
    case Key::Sub:
    case Key::Mul:
    case Key::Div:
      if (error) break;
      applyPending();
      setEntry(accumulator);
      pendingOp = key;
      hasPending = true;
      break;
    case Key::Equals:
      if (error) break;
      applyPending();
      setEntry(accumulator);
      hasPending = false;
      break;
  }
  requestUpdate();
}

void CalculatorActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  int x = 0;
  int y = 0;
  if (!mappedInput.wasScreenTapped(x, y)) return;
  for (int i = 0; i < KEY_COUNT; ++i) {
    const Rect rect = keyRect(i);
    if (x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height) {
      press(static_cast<Key>(i));
      return;
    }
  }
}

void CalculatorActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_CALCULATOR));

  // Display band: the current entry, right-aligned the way a calculator reads.
  const Rect firstKey = keyRect(0);
  const int entryWidth = renderer.getTextWidth(UI_12_FONT_ID, entry.c_str());
  renderer.drawText(UI_12_FONT_ID, pageWidth - metrics.contentSidePadding - entryWidth,
                    firstKey.y - renderer.getLineHeight(UI_12_FONT_ID), entry.c_str(), true, EpdFontFamily::BOLD);
  renderer.drawLine(0, firstKey.y - 2, pageWidth, firstKey.y - 2);

  for (int i = 0; i < KEY_COUNT; ++i) {
    const Rect rect = keyRect(i);
    renderer.drawRect(rect.x, rect.y, rect.width, rect.height);
    const int labelWidth = renderer.getTextWidth(UI_12_FONT_ID, KEY_LABELS[i]);
    renderer.drawText(UI_12_FONT_ID, rect.x + (rect.width - labelWidth) / 2,
                      rect.y + (rect.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2, KEY_LABELS[i]);
  }

  // A keypress should feel immediate, so redraw fast rather than clean; the
  // grid is line art, which ghosts little.
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
