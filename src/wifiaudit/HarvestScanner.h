#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <HalStorage.h>

// Passive PMKID harvester. Channel-hops in promiscuous mode, watches for the
// EAPOL message-1 frames some APs emit (which can carry an RSN PMKID), maps each
// BSSID to the SSID from its beacons, and appends a hashcat 22000 "WPA*01" line
// per unique PMKID to a file on SD. All receive-only -- capturing a PMKID is
// passive; cracking it is a separate offline step on a real computer -- so this
// ships in every build.
//
// Full 4-way-handshake assembly (WPA*02) is left to the offline path: run the
// Wi-Fi Capture tool's .pcap through hcxpcapngtool. This tool does the clientless
// PMKID catch live, with a counter.
//
// Same capture shape as ApScanner (queue + drain); the per-frame parsing reuses
// the host-tested WifiFrame + Eapol logic. ESP32-guarded: begin() returns false
// on the simulator.
namespace wifiaudit {

class HarvestScanner {
 public:
  // Enter promiscuous mode and open `outPath` (a .22000 file). False if the
  // radio is unavailable or the file can't be created.
  bool begin(const char* outPath);
  void end();
  bool running() const { return active; }

  void hopChannel();
  uint8_t currentChannel() const { return channel; }

  // Process queued frames: learn SSIDs, catch PMKIDs, write new lines. True if a
  // new PMKID was written.
  bool drain();

  uint32_t framesSeen() const { return seen; }
  uint32_t pmkidCount() const { return static_cast<uint32_t>(seenPmkids.size()); }

 private:
  struct SsidEntry {
    uint8_t bssid[6];
    std::string ssid;
  };
  void noteBeacon(const uint8_t bssid[6], const std::string& ssid);
  const std::string* lookupSsid(const uint8_t bssid[6]) const;
  bool pmkidIsNew(const uint8_t pmkid[16]);

  HalFile out;
  bool active = false;
  uint8_t channel = 1;
  uint32_t seen = 0;
  std::vector<SsidEntry> ssids;
  std::vector<std::array<uint8_t, 16>> seenPmkids;
};

}  // namespace wifiaudit
