#pragma once

#include <string>

#include "activities/Activity.h"
#include "offensive/CaptureScanner.h"

// The screen behind the "Wi-Fi Capture" utility: a passive, receive-only
// promiscuous capture that streams every 802.11 frame it hears to a .pcap file
// on the SD card (see CaptureScanner). Open the file later in Wireshark, or run
// the captured EAPOL/PMKID frames through the hashcat path. Back stops the
// capture, closes the file, and leaves.
//
// Captures land in /wifiaudit/ on the card (the directory must exist, like
// /flock/ for the camera scanner). The simulator has no radio, so this is
// verified on device; the sim only confirms the shell and the unavailable state.
class WifiCaptureActivity final : public Activity {
 public:
  WifiCaptureActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool preventAutoSleep() override { return true; }

 private:
  enum class State { Unavailable, Capturing };

  wifiaudit::CaptureScanner scanner;
  State state = State::Capturing;
  std::string path;
  uint32_t startedMs = 0;
  uint32_t lastHopMs = 0;
  uint32_t lastPaintMs = 0;
};
