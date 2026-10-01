#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Passive Wi-Fi access-point parsing for the security toolkit. This header is
// the pure, host-tested half: it turns a raw 802.11 management frame into an
// AccessPoint (BSSID, SSID, channel, encryption). It never touches the radio --
// capture lives in ApScanner.{h,cpp}, guarded for the simulator -- so the
// fiddly bit-twiddling (the RSN/WPA information-element walk that decides
// WPA2 vs WPA3) stays dependency-free and testable off-device.
//
// Encryption is read the same way a scanner does: the Privacy bit in the
// capability field says "encrypted at all," and the RSN (802.11i) and WPA v1
// information elements say which scheme. SAE in the RSN AKM list means WPA3;
// PSK/802.1X without SAE means WPA2; both present is a WPA2/WPA3 transition AP.
namespace wifiaudit {

enum class Encryption : uint8_t {
  Open,      // no Privacy bit, no RSN/WPA IE
  Wep,       // Privacy bit set, but no RSN/WPA IE
  Wpa,       // WPA v1 vendor IE present (legacy)
  Wpa2,      // RSN IE, AKM is PSK/802.1X without SAE
  Wpa3,      // RSN IE, AKM includes SAE (or OWE)
  Wpa2Wpa3,  // RSN IE offers both PSK and SAE (transition mode)
};

// Short, stable label for display and logs ("Open", "WPA2", ...).
const char* encryptionLabel(Encryption enc);

// The management-frame subtypes this toolkit cares about. Deauth/Disassoc are
// what a deauth-flood attack sprays; Beacon/ProbeResponse advertise an AP.
enum class MgmtKind : uint8_t {
  Other,
  Beacon,
  ProbeResponse,
  Deauth,
  Disassoc,
};

// Classify a raw frame by its 802.11 management subtype, or Other if it is not
// a management frame (or is too short to read). Cheap: reads only the frame
// control byte. Host-tested.
MgmtKind managementKind(const uint8_t* buf, size_t len);

struct AccessPoint {
  uint8_t bssid[6] = {};
  std::string ssid;     // empty = hidden network
  uint8_t channel = 0;  // from DS Parameter Set IE; 0 if absent
  Encryption encryption = Encryption::Open;
};

// Parse an 802.11 beacon (subtype 0x80) or probe-response (0x50) into `out`.
// False if the frame is too short or is not one of those subtypes. `buf`/`len`
// are the frame as delivered by promiscuous mode: MAC header first, no
// radiotap. Every read is bounds-checked, so a truncated or malformed frame
// yields a best-effort AccessPoint (e.g. a cut-off RSN IE reads as the weaker
// scheme) rather than an over-read. The SSID bytes are copied verbatim and are
// not guaranteed valid UTF-8.
bool parseBeacon(const uint8_t* buf, size_t len, AccessPoint& out);

}  // namespace wifiaudit
