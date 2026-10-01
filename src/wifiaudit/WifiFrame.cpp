#include "WifiFrame.h"

#include <cstring>

namespace wifiaudit {

namespace {
// 802.11 frame control, first two bytes of the header.
constexpr uint8_t FC_TYPE_MASK = 0x0C;        // bits 2-3 = type
constexpr uint8_t FC_TYPE_MGMT = 0x00;        // management
constexpr uint8_t FC_SUBTYPE_MASK = 0xF0;     // bits 4-7 = subtype
constexpr uint8_t SUBTYPE_PROBE_RESP = 0x50;  // 5
constexpr uint8_t SUBTYPE_BEACON = 0x80;      // 8
constexpr uint8_t SUBTYPE_DEAUTH = 0xC0;      // 12
constexpr uint8_t SUBTYPE_DISASSOC = 0xA0;    // 10

constexpr size_t MAC_HEADER_LEN = 24;               // frame control..addr3 + seq ctrl
constexpr size_t ADDR2_OFFSET = 10;                 // transmitter address = AP BSSID here
constexpr size_t FIXED_BODY_LEN = 12;               // timestamp(8) + interval(2) + capability(2)
constexpr size_t CAP_OFFSET = MAC_HEADER_LEN + 10;  // capability info within the fixed body
constexpr uint16_t CAP_PRIVACY = 0x0010;            // "encryption required" bit

constexpr uint8_t TAG_SSID = 0x00;
constexpr uint8_t TAG_DS_PARAM = 0x03;  // current channel, 1 byte
constexpr uint8_t TAG_RSN = 0x30;       // 48: 802.11i RSN element -> WPA2/WPA3
constexpr uint8_t TAG_VENDOR = 0xDD;    // 221: vendor specific (WPA v1 lives here)
constexpr size_t MAX_SSID_LEN = 32;

// AKM suite selectors live under OUI 00:0F:AC in the RSN element. Only the
// suite types we need to tell WPA2 from WPA3 apart are named.
constexpr uint8_t AKM_TYPE_SAE = 8;   // WPA3-Personal
constexpr uint8_t AKM_TYPE_OWE = 18;  // Opportunistic Wireless Encryption (WPA3-era)

// The WPA v1 vendor IE: OUI 00:50:F2, vendor type 1.
constexpr uint8_t WPA_OUI_0 = 0x00;
constexpr uint8_t WPA_OUI_1 = 0x50;
constexpr uint8_t WPA_OUI_2 = 0xF2;
constexpr uint8_t WPA_VENDOR_TYPE = 0x01;

// Parse the AKM suite list of an RSN element body to learn whether it offers
// SAE (WPA3) and/or a classic WPA2 AKM (anything that is not SAE/OWE). `rsn`
// points just past the RSN element's length byte; `len` is its declared length.
// Any field that runs past `len` aborts the walk -- a truncated RSN IE just
// looks like "RSN present, scheme unknown", never an over-read.
void parseRsn(const uint8_t* rsn, size_t len, bool& hasSae, bool& hasWpa2Akm) {
  size_t i = 0;
  if (i + 2 > len) return;  // version
  i += 2;
  if (i + 4 > len) return;  // group cipher suite
  i += 4;
  if (i + 2 > len) return;  // pairwise cipher suite count
  const uint16_t pairwiseCount = static_cast<uint16_t>(rsn[i] | (rsn[i + 1] << 8));
  i += 2;
  if (i + static_cast<size_t>(pairwiseCount) * 4 > len) return;
  i += static_cast<size_t>(pairwiseCount) * 4;
  if (i + 2 > len) return;  // AKM suite count
  const uint16_t akmCount = static_cast<uint16_t>(rsn[i] | (rsn[i + 1] << 8));
  i += 2;
  for (uint16_t a = 0; a < akmCount; ++a) {
    if (i + 4 > len) return;  // each AKM suite is OUI(3) + type(1)
    const uint8_t type = rsn[i + 3];
    if (type == AKM_TYPE_SAE || type == AKM_TYPE_OWE) {
      hasSae = true;
    } else {
      hasWpa2Akm = true;  // PSK, 802.1X, FT variants, etc.
    }
    i += 4;
  }
}
}  // namespace

const char* encryptionLabel(const Encryption enc) {
  switch (enc) {
    case Encryption::Open:
      return "Open";
    case Encryption::Wep:
      return "WEP";
    case Encryption::Wpa:
      return "WPA";
    case Encryption::Wpa2:
      return "WPA2";
    case Encryption::Wpa3:
      return "WPA3";
    case Encryption::Wpa2Wpa3:
      return "WPA2/WPA3";
  }
  return "?";
}

MgmtKind managementKind(const uint8_t* buf, const size_t len) {
  if (!buf || len < 1) return MgmtKind::Other;
  const uint8_t fc = buf[0];
  if ((fc & FC_TYPE_MASK) != FC_TYPE_MGMT) return MgmtKind::Other;
  switch (fc & FC_SUBTYPE_MASK) {
    case SUBTYPE_BEACON:
      return MgmtKind::Beacon;
    case SUBTYPE_PROBE_RESP:
      return MgmtKind::ProbeResponse;
    case SUBTYPE_DEAUTH:
      return MgmtKind::Deauth;
    case SUBTYPE_DISASSOC:
      return MgmtKind::Disassoc;
    default:
      return MgmtKind::Other;
  }
}

bool parseBeacon(const uint8_t* buf, const size_t len, AccessPoint& out) {
  if (!buf || len < MAC_HEADER_LEN + FIXED_BODY_LEN) return false;
  const uint8_t fc = buf[0];
  if ((fc & FC_TYPE_MASK) != FC_TYPE_MGMT) return false;
  const uint8_t subtype = fc & FC_SUBTYPE_MASK;
  if (subtype != SUBTYPE_BEACON && subtype != SUBTYPE_PROBE_RESP) return false;

  std::memcpy(out.bssid, buf + ADDR2_OFFSET, 6);
  out.ssid.clear();
  out.channel = 0;

  const uint16_t cap = static_cast<uint16_t>(buf[CAP_OFFSET] | (buf[CAP_OFFSET + 1] << 8));
  const bool privacy = (cap & CAP_PRIVACY) != 0;

  bool hasRsn = false;
  bool rsnSae = false;
  bool rsnWpa2 = false;
  bool hasWpa = false;

  const uint8_t* body = buf + MAC_HEADER_LEN + FIXED_BODY_LEN;
  const size_t bodyLen = len - (MAC_HEADER_LEN + FIXED_BODY_LEN);
  size_t i = 0;
  while (i + 2 <= bodyLen) {
    const uint8_t tag = body[i];
    const uint8_t tagLen = body[i + 1];
    if (i + 2 + tagLen > bodyLen) break;  // truncated element
    const uint8_t* val = body + i + 2;
    switch (tag) {
      case TAG_SSID: {
        const size_t n = tagLen > MAX_SSID_LEN ? MAX_SSID_LEN : tagLen;
        out.ssid.assign(reinterpret_cast<const char*>(val), n);
        break;
      }
      case TAG_DS_PARAM:
        if (tagLen >= 1) out.channel = val[0];
        break;
      case TAG_RSN:
        hasRsn = true;
        parseRsn(val, tagLen, rsnSae, rsnWpa2);
        break;
      case TAG_VENDOR:
        if (tagLen >= 4 && val[0] == WPA_OUI_0 && val[1] == WPA_OUI_1 && val[2] == WPA_OUI_2 &&
            val[3] == WPA_VENDOR_TYPE) {
          hasWpa = true;
        }
        break;
      default:
        break;
    }
    i += 2 + tagLen;
  }

  if (hasRsn) {
    if (rsnSae && rsnWpa2) {
      out.encryption = Encryption::Wpa2Wpa3;
    } else if (rsnSae) {
      out.encryption = Encryption::Wpa3;
    } else {
      out.encryption = Encryption::Wpa2;
    }
  } else if (hasWpa) {
    out.encryption = Encryption::Wpa;
  } else if (privacy) {
    out.encryption = Encryption::Wep;
  } else {
    out.encryption = Encryption::Open;
  }
  return true;
}

}  // namespace wifiaudit
