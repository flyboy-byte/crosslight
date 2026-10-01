#pragma once

#include <cstdint>
#include <vector>

#include "WifiFrame.h"

// Passive Wi-Fi management-frame monitor. Puts the radio in promiscuous
// (monitor) mode, channel-hops across 2.4GHz, parses beacons/probe-responses
// into a list of access points with their strongest observed signal, and
// timestamps deauth/disassoc frames so a flood can be detected (see
// ThreatDetect). It only ever receives -- it never associates, transmits,
// probes, or deauths -- so it records what is already in the air. Passive: it
// ships in every build, no gating.
//
// Shaped after FlockScanner: the promiscuous callback (WiFi task) copies the
// raw frame into a fixed, no-allocation queue; drain() on the UI task runs the
// host-tested parseBeacon()/managementKind() and aggregates. Monitor mode
// monopolizes the radio, so this runs on its own screen and cannot coexist with
// any other Wi-Fi use.
namespace wifiaudit {

struct ApRecord {
  AccessPoint ap;
  int8_t rssi = 0;  // strongest RSSI seen for this BSSID (closer = higher)
  uint32_t count = 0;
  uint32_t firstSeenMs = 0;
  uint32_t lastSeenMs = 0;
};

class ApScanner {
 public:
  // Enters promiscuous mode. False if the radio is unavailable (e.g. the
  // desktop simulator), so the caller can show that rather than a dead scan.
  bool begin();
  // Exits promiscuous mode and releases the radio. Safe if begin() failed.
  void end();
  bool running() const { return active; }

  // Advance to the next 2.4GHz channel. Called on a timer by the UI so the scan
  // sweeps the band; the radio hears only its current channel at any instant.
  void hopChannel();
  uint8_t currentChannel() const { return channel; }

  // Drain queued frames into the AP set. True if anything changed (new AP or an
  // updated field) since the last drain.
  bool drain();

  const std::vector<ApRecord>& accessPoints() const { return found; }
  uint32_t framesSeen() const { return frames; }

  // Timestamps (millis) of recently heard deauth/disassoc frames, oldest first,
  // capped to the most recent MAX_DEAUTH_EVENTS. Feed to ThreatDetect to judge a
  // flood. Total count seen is deauthsSeen().
  const std::vector<uint32_t>& deauthEventsMs() const { return deauthEvents; }
  uint32_t deauthsSeen() const { return deauths; }

 private:
  void record(const AccessPoint& ap, int8_t rssi);
  void recordDeauth(uint32_t nowMs);

  std::vector<ApRecord> found;
  std::vector<uint32_t> deauthEvents;
  bool active = false;
  uint8_t channel = 1;
  uint32_t frames = 0;
  uint32_t deauths = 0;
};

}  // namespace wifiaudit
