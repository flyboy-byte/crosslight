#include "UnitConverterActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// base = value*scale + offset; value = (base - offset)/scale. offset is 0 for
// the purely multiplicative units and nonzero only for temperature.
struct Unit {
  const char* name;
  double scale;
  double offset;
};
struct Category {
  const char* name;
  const Unit* units;
  int count;
};

// Length — base metre.
constexpr Unit kLength[] = {{"m", 1, 0},         {"km", 1000, 0},   {"cm", 0.01, 0},   {"mm", 0.001, 0},
                            {"mi", 1609.344, 0}, {"yd", 0.9144, 0}, {"ft", 0.3048, 0}, {"in", 0.0254, 0}};
// Mass — base kilogram.
constexpr Unit kMass[] = {{"kg", 1, 0}, {"g", 0.001, 0}, {"lb", 0.45359237, 0}, {"oz", 0.0283495231, 0}};
// Temperature — base kelvin. F: K = F*(5/9) + (273.15 - 32*5/9).
constexpr Unit kTemp[] = {{"C", 1, 273.15}, {"F", 0.5555555556, 255.3722222}, {"K", 1, 0}};
// Volume — base litre.
constexpr Unit kVolume[] = {
    {"L", 1, 0}, {"mL", 0.001, 0}, {"gal", 3.785411784, 0}, {"qt", 0.946352946, 0}, {"floz", 0.0295735296, 0}};
// Speed — base metre/second.
constexpr Unit kSpeed[] = {{"m/s", 1, 0}, {"km/h", 0.2777777778, 0}, {"mph", 0.44704, 0}, {"kn", 0.5144444444, 0}};

constexpr Category kCategories[] = {
    {"Length", kLength, sizeof(kLength) / sizeof(kLength[0])},
    {"Mass", kMass, sizeof(kMass) / sizeof(kMass[0])},
    {"Temp", kTemp, sizeof(kTemp) / sizeof(kTemp[0])},
    {"Volume", kVolume, sizeof(kVolume) / sizeof(kVolume[0])},
    {"Speed", kSpeed, sizeof(kSpeed) / sizeof(kSpeed[0])},
};
constexpr int kCategoryCount = sizeof(kCategories) / sizeof(kCategories[0]);

constexpr int MAX_ENTRY_DIGITS = 12;

std::string formatNumber(const double value) {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%.10g", value);
  return buffer;
}

// Wrap `v` into [0, n) so cycling with +1 / -1 never goes out of range.
int wrap(const int v, const int n) { return ((v % n) + n) % n; }
}  // namespace

UnitConverterActivity::UnitConverterActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("UnitConverter", renderer, mappedInput) {}

void UnitConverterActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

double UnitConverterActivity::result() const {
  const Category& cat = kCategories[category];
  const Unit& from = cat.units[fromUnit];
  const Unit& to = cat.units[toUnit];
  const double value = std::strtod(entry.c_str(), nullptr);
  const double base = value * from.scale + from.offset;
  return (base - to.offset) / to.scale;
}

// Three selector rows sit between the display band and the keypad.
Rect UnitConverterActivity::selectorRect(const int row) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int top = metrics.topPadding + metrics.headerHeight + renderer.getLineHeight(UI_12_FONT_ID) * 2;
  const int rowH = renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing;
  return Rect{0, top + row * rowH, renderer.getScreenWidth(), rowH};
}

Rect UnitConverterActivity::keypadRect(const int index) const {
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int gridTop = selectorRect(2).y + selectorRect(2).height + 4;
  const int gridHeight = safe.y + safe.height - gridTop;
  const int cellW = renderer.getScreenWidth() / KEYPAD_COLUMNS;
  const int cellH = gridHeight / KEYPAD_ROWS;
  const int row = index / KEYPAD_COLUMNS;
  const int column = index % KEYPAD_COLUMNS;
  // The last key (N0) spans the full bottom row.
  const int width = index == static_cast<int>(Key::N0) ? cellW * KEYPAD_COLUMNS : cellW;
  return Rect{column * cellW, gridTop + row * cellH, width, cellH};
}

int UnitConverterActivity::keypadCellAt(const int x, const int y) const {
  for (int i = 0; i <= static_cast<int>(Key::N0); ++i) {
    const Rect r = keypadRect(i);
    if (x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height) return i;
  }
  return -1;
}

void UnitConverterActivity::inputDigit(const char digit) {
  if (digit == '.') {
    if (entry.find('.') == std::string::npos) entry += '.';
    return;
  }
  if (static_cast<int>(entry.size()) >= MAX_ENTRY_DIGITS) return;
  if (entry == "0") entry.clear();
  entry += digit;
  if (entry.empty()) entry = "0";
}

void UnitConverterActivity::cycleCategory(const int delta) {
  category = wrap(category + delta, kCategoryCount);
  // New category -> reset to its first two units so from/to are always valid.
  fromUnit = 0;
  toUnit = kCategories[category].count > 1 ? 1 : 0;
}

void UnitConverterActivity::cycleFrom(const int delta) {
  fromUnit = wrap(fromUnit + delta, kCategories[category].count);
}

void UnitConverterActivity::cycleTo(const int delta) { toUnit = wrap(toUnit + delta, kCategories[category].count); }

void UnitConverterActivity::pressKey(const Key key) {
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
      static const char kDigit[] = {'7', '8', '9', '0', '4', '5', '6', '0', '1', '2', '3', '0', '0'};
      inputDigit(kDigit[static_cast<int>(key)]);
      break;
    }
    case Key::Dot:
      inputDigit('.');
      break;
    case Key::Back:
      if (!entry.empty()) entry.pop_back();
      if (entry.empty() || entry == "-") entry = "0";
      break;
    case Key::Clear:
      entry = "0";
      break;
  }
  requestUpdate();
}

void UnitConverterActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  int x = 0;
  int y = 0;
  if (!mappedInput.wasScreenTapped(x, y)) return;

  // Selector rows: left third steps back, right two-thirds steps forward.
  for (int row = 0; row < 3; ++row) {
    const Rect r = selectorRect(row);
    if (y < r.y || y >= r.y + r.height) continue;
    const int delta = x < renderer.getScreenWidth() / 3 ? -1 : 1;
    RenderLock lock(*this);
    if (row == 0)
      cycleCategory(delta);
    else if (row == 1)
      cycleFrom(delta);
    else
      cycleTo(delta);
    requestUpdate();
    return;
  }

  const int cell = keypadCellAt(x, y);
  if (cell >= 0) pressKey(static_cast<Key>(cell));
}

void UnitConverterActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const Category& cat = kCategories[category];

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_UNIT_CONVERTER));

  // Display band: "<entry> <from>" small, "= <result> <to>" bold beneath.
  const int bandTop = metrics.topPadding + metrics.headerHeight;
  const std::string in = entry + " " + cat.units[fromUnit].name;
  const std::string out = "= " + formatNumber(result()) + " " + cat.units[toUnit].name;
  const int inW = renderer.getTextWidth(UI_10_FONT_ID, in.c_str());
  renderer.drawText(UI_10_FONT_ID, pageWidth - metrics.contentSidePadding - inW, bandTop, in.c_str());
  const int outW = renderer.getTextWidth(UI_12_FONT_ID, out.c_str());
  renderer.drawText(UI_12_FONT_ID, pageWidth - metrics.contentSidePadding - outW,
                    bandTop + renderer.getLineHeight(UI_10_FONT_ID), out.c_str(), true, EpdFontFamily::BOLD);

  // Selector rows: "◂ Label ▸" style, label centered, chevrons at the edges.
  const char* labels[3] = {cat.name, cat.units[fromUnit].name, cat.units[toUnit].name};
  const char* prefix[3] = {"Category", "From", "To"};
  for (int row = 0; row < 3; ++row) {
    const Rect r = selectorRect(row);
    renderer.drawLine(0, r.y, pageWidth, r.y);
    const int ty = r.y + (r.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, ty, prefix[row]);
    std::string mid = std::string("◂  ") + labels[row] + "  ▸";
    const int midW = renderer.getTextWidth(UI_12_FONT_ID, mid.c_str());
    renderer.drawText(UI_12_FONT_ID, (pageWidth - midW) / 2, ty, mid.c_str());
  }

  // Keypad.
  static const char* const kLabels[] = {"7", "8", "9", "<", "4", "5", "6", "C", "1", "2", "3", ".", "0"};
  for (int i = 0; i <= static_cast<int>(Key::N0); ++i) {
    const Rect r = keypadRect(i);
    renderer.drawRect(r.x, r.y, r.width, r.height);
    const int lw = renderer.getTextWidth(UI_12_FONT_ID, kLabels[i]);
    renderer.drawText(UI_12_FONT_ID, r.x + (r.width - lw) / 2,
                      r.y + (r.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2, kLabels[i]);
  }

  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
