#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "FlockSignature.h"

// Passive Wi-Fi surveillance-device scanner. Puts the radio in promiscuous
// (monitor) mode, channel-hops, and matches management frames against loaded
// signatures. It only receives -- it never associates, transmits, deauths, or
// touches any network. Monitor mode monopolizes the radio, so this runs on its
// own screen and cannot coexist with reading or any other Wi-Fi use.
//
// The promiscuous callback (WiFi task) does the minimum: copy the raw frame and
// RSSI into a fixed queue, no allocation. drain() runs on the UI task and does
// the real work with the host-tested parseManagementFrame()/match(), so nothing
// untested runs in the radio callback.
namespace flock {

struct Detection {
  std::string name;   // signature label
  std::string ssid;   // SSID seen (may be empty)
  uint8_t mac[6] = {};
  int8_t rssi = 0;    // strongest RSSI seen for this device (closer = higher)
  uint8_t channel = 0;
  uint32_t count = 0;  // frames matched from this device
  uint32_t firstSeenMs = 0;
  uint32_t lastSeenMs = 0;
};

class FlockScanner {
 public:
  // Enters promiscuous mode with `signatures`. False if the radio is
  // unavailable (e.g. the desktop simulator has none), in which case the caller
  // should show that rather than a dead scan.
  bool begin(std::vector<Signature> signatures);
  // Exits promiscuous mode and releases the radio. Safe to call if begin()
  // failed or was never called.
  void end();
  bool running() const { return active; }

  // Move to the next 2.4GHz channel. Called on a timer by the UI so the scan
  // sweeps the band; the radio hears only its current channel at any instant.
  void hopChannel();
  uint8_t currentChannel() const { return channel; }

  // Drain queued frames, update the detection set, and return true if anything
  // changed (new device or updated RSSI/count) since the last drain.
  bool drain();

  const std::vector<Detection>& detections() const { return found; }
  uint32_t framesSeen() const { return frames; }

 private:
  void record(const Observation& obs, int8_t rssi, int matchedIndex);

  std::vector<Signature> signatures;
  std::vector<Detection> found;
  bool active = false;
  uint8_t channel = 1;
  uint32_t frames = 0;
};

}  // namespace flock
