#pragma once

#include <cstdint>

#include "PcapSink.h"

// Passive full-frame capture: promiscuous mode with no subtype filter, streaming
// every 802.11 frame it hears to a .pcap on the SD card (see PcapSink). Receive
// only -- it never transmits -- so it ships in every build. Open in Wireshark,
// or feed the captured EAPOL/PMKID frames to the hashcat path (see Eapol).
//
// Same shape as ApScanner: the promiscuous callback (WiFi task) copies each raw
// frame into a fixed, no-allocation queue; drain() on the UI task writes queued
// frames to SD. Monitor mode monopolizes the radio, so this runs on its own
// screen. ESP32-guarded: begin() returns false on the simulator (no radio).
namespace wifiaudit {

class CaptureScanner {
 public:
  // Enter promiscuous mode and open `pcapPath` for writing. False if the radio
  // is unavailable or the file can't be created.
  bool begin(const char* pcapPath);
  // Leave promiscuous mode, release the radio, and close the pcap file.
  void end();
  bool running() const { return active; }

  void hopChannel();
  uint8_t currentChannel() const { return channel; }

  // Write queued frames to SD. Returns true if at least one frame was written.
  bool drain();

  uint32_t framesSeen() const { return seen; }
  uint32_t framesWritten() const { return sink.frameCount(); }
  uint32_t bytesWritten() const { return sink.bytesWritten(); }

 private:
  PcapSink sink;
  bool active = false;
  uint8_t channel = 1;
  uint32_t seen = 0;
};

}  // namespace wifiaudit
