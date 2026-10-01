#pragma once

#include <string>

#include "activities/Activity.h"
#include "components/themes/BaseTheme.h"

// Tap-driven unit converter: pick a category and a from/to unit, type a value,
// read the result live. Immediate-execution like the calculator -- no modes.
//
// Every unit converts through its category's base via an affine map
// (base = value*scale + offset), so temperature (C/F/K, which have an offset)
// uses the same plain data table as the purely multiplicative units.
class UnitConverterActivity final : public Activity {
 public:
  UnitConverterActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&& lock) override;

 private:
  enum class Key { N7, N8, N9, Back, N4, N5, N6, Clear, N1, N2, N3, Dot, N0 };
  static constexpr int KEYPAD_COLUMNS = 4;
  static constexpr int KEYPAD_ROWS = 4;

  Rect keypadRect(int index) const;
  int keypadCellAt(int x, int y) const;  // -1 if none
  // y-bands of the three selector rows (category / from / to).
  Rect selectorRect(int row) const;

  void inputDigit(char digit);
  void pressKey(Key key);
  void cycleCategory(int delta);
  void cycleFrom(int delta);
  void cycleTo(int delta);
  double result() const;

  int category = 0;
  int fromUnit = 0;
  int toUnit = 1;
  std::string entry = "1";
};
