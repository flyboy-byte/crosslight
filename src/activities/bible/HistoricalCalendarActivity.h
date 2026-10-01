#pragma once

#include <string>

#include "activities/Activity.h"
#include "components/themes/BaseTheme.h"

// Historical Calendar: the 1611 KJV's front-matter almanac ("An Almanacke for
// xxxix yeeres") generalized to any year. Type a year on the keypad and read the
// computus row the almanac would have tabulated -- Golden Number, Epact, Sunday
// (Dominical) Letter, and the date of Easter in both the Old Style (Julian, the
// calendar 1611 England kept) and New Style (Gregorian) systems.
//
// The math lives in bible/HistoricalCalendar.h (pure, host-tested); this screen
// is just input + layout. Modeled on UnitConverterActivity's keypad screen.
class HistoricalCalendarActivity final : public Activity {
 public:
  HistoricalCalendarActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&& lock) override;

 private:
  enum class Key { N7, N8, N9, Back, N4, N5, N6, Clear, N1, N2, N3, N0 };
  static constexpr int KEYPAD_COLUMNS = 4;
  static constexpr int KEYPAD_ROWS = 4;

  Rect keypadRect(int index) const;
  int keypadCellAt(int x, int y) const;  // -1 if none
  int keypadTop() const;

  void inputDigit(char digit);
  void pressKey(Key key);
  int year() const;  // parsed entry, 0 if empty

  std::string entry;
};
