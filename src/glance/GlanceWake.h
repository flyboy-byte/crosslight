#pragma once

#include <cstdint>

class GfxRenderer;

// "Watch face on sleep": while the device sleeps it wakes on a timer every few
// minutes, draws the time and the latest glucose over the sleep screen, and goes
// back to sleep. A timer wake is a chip reset, so the cycle runs from setup()
// before any activity exists (see main.cpp glanceWakeCycle()).
//
// What is measured vs. unknown (PLAN.md "Glance-on-sleep"): deep-sleep current on
// this board is UNMEASURED. The counters below exist so the real cost can be read
// off the device instead of guessed.
namespace glance {

constexpr uint32_t kIntervalS = 300;      // 5 minutes
constexpr uint32_t kBleWindowMs = 8000;   // how long a wake listens for the companion
constexpr uint32_t kHalfRefreshEvery = 6;  // clear ghosting with a HALF refresh every Nth wake
constexpr uint8_t kMinBatteryPct = 8;      // below this the timer is not re-armed

// Survives chip resets (RTC_NOINIT), lost on power loss. Validated by `magic`.
struct Stats {
  uint32_t magic;
  uint32_t wakes;        // glance wakes since the device last went to sleep by hand
  uint32_t bleHits;      // wakes in which the companion delivered a reading
  uint32_t awakeMs;      // summed time spent awake in glance wakes (boot to re-sleep)
  uint32_t bleMs;        // summed BLE listening time
  int64_t armedEpoch;    // when sleep began (unix UTC), 0 if the clock was untrusted
  uint8_t armedBattery;  // battery % when sleep began
  // Latest reading the companion delivered while asleep.
  uint16_t value;        // mg/dL, 0xFFFF none
  uint8_t trend;         // wire trend 0..5
  uint8_t flags;         // wire flags
  int64_t rxEpoch;       // unix UTC when it arrived, 0 = none
};

Stats& stats();

// Called on the way into a hand-initiated deep sleep with the feature on:
// resets the counters and arms the first timer wake.
void armFirst(uint8_t batteryPct, int64_t nowEpoch);
// Re-arms the next timer wake (inside a glance wake).
void armNext();

// True when this boot was a glance timer wake.
bool isGlanceWake();

struct FaceData {
  bool haveTime = false;
  char time[8] = "--:--";
  char date[24] = "";
  uint8_t batteryPct = 0;
  // Glucose line; hasGlucose=false draws "no recent reading".
  bool hasGlucose = false;
  int value = 0;
  const char* trend = "";
  int ageMin = 0;
};

// Draws the face as a slim header (time, battery) and footer (glucose) over
// whatever is already in the frame buffer (the sleep screen), leaving the middle
// of the wallpaper clear. Does not refresh the display.
void drawOverlay(GfxRenderer& renderer, const FaceData& data);

}  // namespace glance
