#pragma once

#include <string>

#include "activities/Activity.h"

// Downloads one Bible translation from getBible over Wi-Fi into /Bible/<ABBR>/<abbr>.json
// and selects it. Follows OtaUpdateActivity's shape: Wi-Fi picker, blocking transfer with
// a progress screen (Back cancels), and a silent restart on exit to tear Wi-Fi down.
class BibleDownloadActivity final : public Activity {
 public:
  BibleDownloadActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string abbr);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;

 private:
  enum State { CONNECTING, DOWNLOADING, DONE, FAILED };

  void onWifiSelectionComplete(bool connected);
  void download();

  std::string abbr;
  State state = CONNECTING;
  size_t downloaded = 0;
  size_t total = 0;
  bool cancelRequested = false;
  const char* failure = nullptr;
};
