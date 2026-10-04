#pragma once

#include <cstdint>
#include <string>

#include "activities/Activity.h"
#include "components/themes/BaseTheme.h"

class DNSServer;
class WebServer;

// The screen behind the "Evil Twin" utility: clones a chosen SSID as an open
// access point and serves a captive-portal landing page to anything that
// joins -- for testing your own devices' and your own detector's reaction to
// an evil-twin AP (this pairs directly with the already-shipped
// WifiThreatActivity evil-twin DETECTION tool: run one CrossLight device as
// this, another as the detector, and confirm it fires).
//
// Deliberately NOT a credential-harvesting login form: the landing page is a
// plain, clearly-labelled test notice. This is an ACTIVE tool (it broadcasts
// and accepts associations), so it is gated the same way every active
// wifiaudit tool is -- AttackTx::buildSupportsActiveAudit() and the per-boot
// ActiveAuditGate (confirmed automatically on start, no modal, but the
// on-screen "your own gear only" warning stays up the whole time the screen
// is open).
//
// Reuses the same DNSServer-wildcard-redirect + WebServer mechanism
// CrossPointWebServerActivity already uses for the file-transfer hotspot --
// the radio mode (AP + web server) is genuinely stubbed in the simulator
// (unlike raw-frame TX), so the shell states verify there; the actual
// portal/association behavior is still device-only to confirm.
class EvilTwinActivity final : public Activity {
 public:
  EvilTwinActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool preventAutoSleep() override { return true; }

 private:
  enum class State { Disabled, Idle, Running };

  Rect ssidRowRect() const;
  void startPortal();
  void stopPortal();
  void openSsidEntry();

  State state = State::Disabled;
  std::string ssid;
  bool lastStartFailed = false;
  uint32_t startedMs = 0;
  uint32_t lastPaintMs = 0;

  DNSServer* dnsServer = nullptr;
  WebServer* webServer = nullptr;
};
