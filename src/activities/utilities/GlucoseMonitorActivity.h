#pragma once

#include <cstdint>
#include <string>

#include "activities/Activity.h"
#include "libre/LibreBridgeClient.h"

// Glucose Monitor: Logan's own Libre bridge (see
// ~/projects/extraction/docs/agent-api.md), on the e-ink display. Same
// foreground-only shape as Claude Panel -- Wi-Fi and polling only while this
// screen is open, torn down on exit. Unlike Claude Panel, auto-refresh
// defaults ON: there's no quota to protect here, and new readings only land
// about once a minute anyway (see kAutoIntervalMs), so a slower screen is
// never a reason to poll faster.
class GlucoseMonitorActivity final : public Activity {
 public:
  GlucoseMonitorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool preventAutoSleep() override { return autoRefresh; }

 private:
  void onWifiSelectionComplete(bool connected);
  void refresh();
  const char* trendArrow() const;
  std::string statusText() const;

  libre::Reading reading;
  bool haveReading = false;
  bool stale = false;
  bool autoRefresh = true;
  bool connected = false;
  bool stopped = false;  // set on 401 -- never retried, see agent-api.md
  std::string statusLine;
  uint32_t lastFetchMs = 0;
};
