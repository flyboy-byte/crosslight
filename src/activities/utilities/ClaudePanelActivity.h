#pragma once

#include <cstdint>
#include <string>

#include "activities/Activity.h"
#include "claude/ClaudeClient.h"

// Claude Panel: subscription usage (5h + weekly) on the e-ink display.
// Foreground-only: Wi-Fi and polling live only while this screen is open.
// Confirm (or a tap) refreshes; Left (or the mode chip) toggles manual vs 30 s
// auto-refresh. Defaults to manual because every refresh is a real request
// against the quota. Plan: docs/crosslight/claude-panel.md.
class ClaudePanelActivity final : public Activity {
 public:
  ClaudePanelActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool preventAutoSleep() override { return autoRefresh; }

 private:
  void onWifiSelectionComplete(bool connected);
  void refresh();
  void drawWindow(int y, const char* title, const claude::Window& w, int pad, int width) const;
  std::string resetText(const claude::Window& w) const;

  claude::Usage usage;
  bool haveUsage = false;
  bool stale = false;
  bool autoRefresh = false;
  bool connected = false;
  std::string statusLine;
  uint32_t lastFetchMs = 0;
};
