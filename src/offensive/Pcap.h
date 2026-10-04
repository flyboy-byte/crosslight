#pragma once

#include <cstddef>
#include <cstdint>

// Classic libpcap byte-format helpers for streaming captured 802.11 frames to an
// SD card, openable in Wireshark/tshark or crackable offline. Capturing is
// passive -- you record frames already in the air -- so this ships in every
// build. These two functions are pure byte serializers (little-endian, the
// a1b2c3d4 host-order magic) so the exact header layout is host-tested; the SD
// streaming on top lives in PcapSink.
namespace wifiaudit {

// LINKTYPE for raw 802.11 frames with no radiotap header, as promiscuous mode
// delivers them. (Radiotap captures would use 127 instead.)
constexpr uint32_t PCAP_LINKTYPE_IEEE802_11 = 105;
constexpr uint32_t PCAP_SNAPLEN = 2324;  // max 802.11 MSDU; frames are capped to this
constexpr size_t PCAP_GLOBAL_HEADER_LEN = 24;
constexpr size_t PCAP_RECORD_HEADER_LEN = 16;

// Serialize the 24-byte pcap global file header into `out` (>= 24 bytes).
void pcapWriteGlobalHeader(uint8_t out[PCAP_GLOBAL_HEADER_LEN], uint32_t linktype, uint32_t snaplen);

// Serialize a 16-byte per-packet record header into `out` (>= 16 bytes).
// `inclLen` is the number of frame bytes that follow; `origLen` is the frame's
// true length (>= inclLen if it was truncated to snaplen).
void pcapWriteRecordHeader(uint8_t out[PCAP_RECORD_HEADER_LEN], uint32_t tsSec, uint32_t tsUsec, uint32_t inclLen,
                           uint32_t origLen);

}  // namespace wifiaudit
