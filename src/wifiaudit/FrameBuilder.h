#pragma once

#include <cstddef>
#include <cstdint>

// Active 802.11 management-frame builders for the security toolkit: the inverse
// of WifiFrame.{h,cpp}'s parser. Each function SERIALIZES a raw management frame
// into a caller-provided buffer and returns the number of bytes written, or 0 if
// the buffer is too small. They are pure byte builders -- no radio, no heap -- so
// the exact wire layout is host-tested the same way Pcap.{h,cpp} is; the actual
// transmit is isolated in AttackTx and double-gated.
//
// These frames drive *active* audits (deauth/disassoc floods, rogue beacons)
// that a passive scan cannot do, because they put energy on the air. Building
// the bytes is harmless and always compiled; transmitting them is what the
// compile flag and the runtime ActiveAuditGate guard. Keeping the builders pure
// means we can prove the bytes are correct off-device, where the radio refuses
// some subtypes anyway (see AttackTx).
namespace wifiaudit {

// 802.11 reason codes (IEEE 802.11 Table 9-45) carried by deauth/disassoc
// frames. A deauth claiming the client sent a Class-3 frame while not associated
// uses CLASS3; a disassoc meaning "the station is leaving" uses DISASSOC_LEAVING.
// These are the values stock clients expect, so they act on the frame.
constexpr uint16_t REASON_CLASS3_FROM_NONASSOC = 7;
constexpr uint16_t REASON_DISASSOC_LEAVING = 8;

// Deauth and disassoc share one fixed size: the 24-byte MAC header plus a 2-byte
// reason code. Exposed so callers can stack-allocate an exact buffer.
constexpr size_t DEAUTH_FRAME_LEN = 26;
constexpr size_t DISASSOC_FRAME_LEN = 26;

// 802.11 caps an SSID at 32 bytes, and the SSID information element carries its
// length in a single byte -- buildBeacon refuses a longer SSID rather than
// emit a malformed element.
constexpr size_t MAX_SSID_LEN = 32;

// Serialize a deauthentication frame (management subtype 12) into `out`. addr1
// is the destination `client` -- pass broadcast FF:FF:FF:FF:FF:FF to kick every
// station off `bssid`; addr2 and addr3 are the `bssid` the frame is spoofed from,
// so the client believes the AP sent it. The reason code is written
// little-endian, as it travels on the wire. Returns 26, or 0 if `cap` < 26.
size_t buildDeauth(uint8_t* out, size_t cap, const uint8_t bssid[6], const uint8_t client[6], uint16_t reasonCode);

// Serialize a disassociation frame (management subtype 10). Byte-for-byte a
// deauth apart from the subtype in the frame-control field. Returns 26, or 0 if
// `cap` < 26.
size_t buildDisassoc(uint8_t* out, size_t cap, const uint8_t bssid[6], const uint8_t client[6], uint16_t reasonCode);

// Serialize a beacon frame (management subtype 8) advertising `bssid` as an AP
// named `ssid` (`ssidLen` bytes, up to MAX_SSID_LEN) on `channel`. addr1 is
// broadcast. The body is the minimum a client needs to list the network: the
// fixed timestamp/interval/capability fields, then the SSID, Supported Rates and
// DS Parameter Set information elements. Returns the total length (51 + ssidLen),
// or 0 if `cap` is too small, `ssid` is null, or `ssidLen` exceeds MAX_SSID_LEN.
size_t buildBeacon(uint8_t* out, size_t cap, const uint8_t bssid[6], const char* ssid, size_t ssidLen,
                   uint8_t channel);

}  // namespace wifiaudit
