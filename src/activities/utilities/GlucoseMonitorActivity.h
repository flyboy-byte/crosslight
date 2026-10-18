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
// Two acquisition sources, switched with the Right button in-session (see
// crosslight/PLAN.md "Planned: Bluetooth glucose relay" and
// ~/projects/extraction/docs/reference/crosslight-ble-glucose-handoff.md):
// Wi-Fi/VPS (default, existing) and BLE from the companion app, which pushes
// the frozen 8-byte packet (extraction DECISIONS.md D-020) over one held,
// bonded GATT connection rather than per-reading reconnects. The choice is
// persisted (SETTINGS.glucoseSource) and also settable from a toggle row in
// UtilitiesActivity's list, so the source can be picked before opening this
// screen at all -- a plain Right-button tap is too easy to trigger by
// accident on a button also used for paging everywhere else.
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

  void onWifiSelectionComplete(bool connected);
  void refresh();
  void switchSource(Source next);
  void beginSource();
  void endSource();
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
};
