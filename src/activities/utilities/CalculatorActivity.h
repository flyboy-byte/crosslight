#pragma once

#include <string>

#include "activities/Activity.h"
#include "components/themes/BaseTheme.h"

// Four-function calculator: the first utility tile, and the proof that the
// registry seam works end to end.
//
// Immediate-execution (the way a pocket calculator behaves, not RPN): an
// operator applies whatever is pending, so 2 + 3 x 4 reads left to right and
// gives 20. Deliberately small -- no memory keys, no history, no scientific
// functions.
class CalculatorActivity final : public Activity {
 public:
  CalculatorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&& lock) override;

 private:
  // Keys in row-major order, matching the drawn 4x5 grid.
  enum class Key { N7, N8, N9, Div, N4, N5, N6, Mul, N1, N2, N3, Sub, Dot, N0, Sign, Add, Clear, Back, Equals };
  static constexpr int COLUMNS = 4;
  static constexpr int ROWS = 5;

  // Screen rect of `key`'s button, for both drawing and hit-testing.
  Rect keyRect(int index) const;
  void press(Key key);
  void inputDigit(char digit);
  void applyPending();
  void setEntry(double value);

  std::string entry = "0";
  // The left-hand side of a pending operation, e.g. the 2 in "2 +".
  double accumulator = 0;
  Key pendingOp = Key::Equals;
  bool hasPending = false;
  // True until the next digit, which then replaces the display rather than
  // appending to it (after = or an operator).
  bool entryIsResult = true;
  bool error = false;
};
