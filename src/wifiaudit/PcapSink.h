#pragma once

#include <HalStorage.h>

#include <cstddef>
#include <cstdint>

#include "Pcap.h"

// Streams captured 802.11 frames to a .pcap file on the SD card. Portable (uses
// HalStorage, so it also builds on the host). Open writes the global header;
// appendFrame writes one record. Frames longer than PCAP_SNAPLEN are truncated
// in the file but their true length is preserved in the record's origLen, the
// way tcpdump does it.
namespace wifiaudit {

class PcapSink {
 public:
  // Create/overwrite `path` and write the pcap global header. False if the file
  // could not be opened (e.g. no SD card).
  bool open(const char* path);
  // Write one frame record. Safe to call only while isOpen(). `origLen` is the
  // frame's true length; `bytes`/`len` is what was actually captured.
  bool appendFrame(const uint8_t* bytes, size_t len, uint32_t origLen, uint32_t tsSec, uint32_t tsUsec);
  void close();

  bool isOpen() const { return opened; }
  uint32_t frameCount() const { return frames; }
  uint32_t bytesWritten() const { return bytes; }

 private:
  HalFile file;
  bool opened = false;
  uint32_t frames = 0;
  uint32_t bytes = 0;
};

}  // namespace wifiaudit
