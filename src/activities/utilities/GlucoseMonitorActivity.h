#pragma once

#include <cstdint>
#include <string>

#include "activities/Activity.h"
#include "libre/LibreBleServer.h"
#include "libre/LibreBridgeClient.h"

// Glucose Monitor: Logan's own Libre bridge (see
// ~/projects/extraction/docs/agent-api.md), on the e-ink display. Same
// foreground-only shape as Claude Panel -- radios and polling only while
// this screen is open, torn down on exit. Unlike Claude Panel, auto-refresh
// defaults ON: there's no quota to protect here, and new readings only land
// about once a minute anyway (see kAutoIntervalMs), so a slower screen is
// never a reason to poll faster.
//
// Lives on the Home menu (not under Utilities). Two acquisition sources,
// picked with an on-screen Wi-Fi / Bluetooth switch (touch targets, never a
// physical button -- a plain Right-button tap was too easy to trigger by
// accident on a button also used for paging everywhere else): Wi-Fi/VPS
// polling, and BLE pushed by the companion app (the firmware owns that
// contract; see crosslight/PLAN.md "BLE rework"). The choice is persisted in
// SETTINGS.glucoseSource. A Read button fetches now on Wi-Fi; on BLE the phone
// pushes, so Read just re-judges freshness.
class GlucoseMonitorActivity final : public Activity {
 public:
  GlucoseMonitorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool preventAutoSleep() override { return autoRefresh; }

 private:
  enum class Source { Wifi, Ble };

  // Touch targets, shared by render() and loop() so a drawn button and its hit
  // area can never drift apart.
  struct Box {
    int x = 0, y = 0, w = 0, h = 0;
    bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
  };
  struct Layout {
    Box wifi;      // left half of the source switch
    Box ble;       // right half of the source switch
    Box mode;      // Wi-Fi only: "Auto 60s" / "Manual" line (tap toggles); empty on BLE
    Box read;      // Read button
    int infoY = 0;    // text baseline of the line under the switch (mode on Wi-Fi, forget hint on BLE)
    int bodyTop = 0;  // first y below the controls
  };
  Layout computeLayout() const;
  void readNow();

  void onWifiSelectionComplete(bool connected);
  void refresh();
  void switchSource(Source next);
  void beginSource();
  void endSource(bool returnToMonitor = false);
  void pollBle();
  void forgetBlePeer();
  const char* trendArrow() const;
  std::string statusText() const;

  Source source = Source::Wifi;
  libre::LibreBleServer bleServer;
  libre::BlePacket lastBlePacket;
  libre::Reading reading;
  bool haveReading = false;
  bool stale = false;
  bool autoRefresh = true;
  bool connected = false;
  bool stopped = false;  // set on 401 -- never retried, see agent-api.md
  std::string statusLine;
  uint32_t lastFetchMs = 0;
  uint32_t lastBleWriteMs = 0;  // millis() when the last BLE packet arrived (receipt-time freshness)
  bool forceBleRecheck = false;  // Read pressed on BLE: re-judge freshness now instead of at the next 10 s tick
};
