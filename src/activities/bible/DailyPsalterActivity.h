#pragma once

#include <string>

#include "activities/Activity.h"
#include "components/themes/BaseTheme.h"

// Daily Psalter: the 1611 KJV front matter's "Table and Kalender... of
// Psalmes and Lessons... at Morning and Euening prayer" -- type a day of the
// month, read that day's Morning and Evening Psalm readings from the Prayer
// Book's fixed 30-day cycle.
//
// The table is pure fixed data (bible/BiblePsalterReadings.h, host-tested);
// this screen is input + layout, modeled on HistoricalCalendarActivity's
// keypad screen.
class DailyPsalterActivity final : public Activity {
 public:
  DailyPsalterActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

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
  int day() const;  // parsed entry, 0 if empty

  std::string entry;
};
