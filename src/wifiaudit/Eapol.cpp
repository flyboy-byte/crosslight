#include "Eapol.h"

#include <cstring>

namespace wifiaudit {

namespace {
// 802.1X / EAPOL-Key field offsets, measured from the EAPOL-Key body start (the
// byte after the 4-byte 802.1X header).
constexpr size_t X1_HEADER_LEN = 4;      // version, type, length(2)
constexpr uint8_t X1_TYPE_EAPOL_KEY = 3;
constexpr size_t EK_KEYINFO = 1;
constexpr size_t EK_NONCE = 13;
constexpr size_t EK_MIC = 77;
constexpr size_t EK_KEYDATALEN = 93;
constexpr size_t EK_KEYDATA = 95;
constexpr size_t EK_BODY_MIN = EK_KEYDATA;  // through the key-data length field

// Key Info bits (the field is big-endian on the wire).
constexpr uint16_t KI_MIC = 0x0100;
constexpr uint16_t KI_ACK = 0x0080;
constexpr uint16_t KI_SECURE = 0x0200;

// RSN PMKID KDE: 0xDD vendor element, OUI 00:0F:AC, data type 0x04.
constexpr uint8_t KDE_VENDOR = 0xDD;
constexpr uint8_t KDE_OUI_0 = 0x00;
constexpr uint8_t KDE_OUI_1 = 0x0F;
constexpr uint8_t KDE_OUI_2 = 0xAC;
constexpr uint8_t KDE_TYPE_PMKID = 0x04;

uint16_t be16(const uint8_t* p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }

const char* HEX = "0123456789abcdef";

void appendHex(std::string& s, const uint8_t* bytes, const size_t len) {
  for (size_t i = 0; i < len; ++i) {
    s.push_back(HEX[bytes[i] >> 4]);
    s.push_back(HEX[bytes[i] & 0x0F]);
  }
}
}  // namespace

bool parseEapolKey(const uint8_t* buf, const size_t len, EapolKey& out) {
  if (!buf || len < X1_HEADER_LEN + EK_BODY_MIN) return false;
  if (buf[1] != X1_TYPE_EAPOL_KEY) return false;
  const uint8_t* body = buf + X1_HEADER_LEN;
  const size_t bodyLen = len - X1_HEADER_LEN;
  if (bodyLen < EK_BODY_MIN) return false;

  out.descriptorType = body[0];
  out.keyInfo = be16(body + EK_KEYINFO);
  std::memcpy(out.nonce, body + EK_NONCE, 32);
  std::memcpy(out.mic, body + EK_MIC, 16);
  out.hasMic = (out.keyInfo & KI_MIC) != 0;

  const uint16_t keyDataLen = be16(body + EK_KEYDATALEN);
  if (EK_KEYDATA + keyDataLen > bodyLen) return false;  // declared key data overruns
  out.keyData = keyDataLen ? body + EK_KEYDATA : nullptr;
  out.keyDataLen = keyDataLen;
  return true;
}

EapolMessage classifyEapol(const uint16_t keyInfo) {
  const bool mic = (keyInfo & KI_MIC) != 0;
  const bool ack = (keyInfo & KI_ACK) != 0;
  const bool secure = (keyInfo & KI_SECURE) != 0;
  if (ack && !mic) return EapolMessage::M1;
  if (ack && mic) return EapolMessage::M3;
  if (!ack && mic && !secure) return EapolMessage::M2;
  if (!ack && mic && secure) return EapolMessage::M4;
  return EapolMessage::Unknown;
}

bool extractPmkid(const EapolKey& key, uint8_t pmkidOut[16]) {
  if (!key.keyData || key.keyDataLen < 2) return false;
  size_t i = 0;
  while (i + 2 <= key.keyDataLen) {
    const uint8_t id = key.keyData[i];
    const uint8_t elen = key.keyData[i + 1];
    if (i + 2 + elen > key.keyDataLen) break;
    if (id == KDE_VENDOR && elen >= 4 + 16) {
      const uint8_t* v = key.keyData + i + 2;
      if (v[0] == KDE_OUI_0 && v[1] == KDE_OUI_1 && v[2] == KDE_OUI_2 && v[3] == KDE_TYPE_PMKID) {
        const uint8_t* pmkid = v + 4;
        bool allZero = true;
        for (int b = 0; b < 16; ++b) {
          if (pmkid[b] != 0) {
            allZero = false;
            break;
          }
        }
        if (allZero) return false;  // AP offered no PMKID
        std::memcpy(pmkidOut, pmkid, 16);
        return true;
      }
    }
    i += 2 + elen;
  }
  return false;
}

bool findEapolInDataFrame(const uint8_t* frame, const size_t len, EapolLocation& out) {
  if (!frame || len < 24) return false;
  const uint8_t fc0 = frame[0];
  const uint8_t fc1 = frame[1];
  if ((fc0 & 0x0C) != 0x08) return false;  // not a data frame
  if (fc1 & 0x40) return false;            // Protected: encrypted body, LLC not in clear

  const bool toDs = (fc1 & 0x01) != 0;
  const bool fromDs = (fc1 & 0x02) != 0;
  const bool qos = (fc0 & 0xF0) >= 0x80;  // QoS-data subtypes have the 0x08 subtype bit set

  size_t hdr = 24;
  if (toDs && fromDs) hdr += 6;  // addr4 present (WDS)
  if (qos) hdr += 2;             // QoS control
  if (len < hdr + 8 + X1_HEADER_LEN) return false;

  // LLC/SNAP: AA AA 03 00 00 00, then 2-byte EtherType. 0x888E = EAPOL.
  const uint8_t* llc = frame + hdr;
  if (!(llc[0] == 0xAA && llc[1] == 0xAA && llc[2] == 0x03 && llc[3] == 0x00 && llc[4] == 0x00 && llc[5] == 0x00))
    return false;
  if (be16(llc + 6) != 0x888E) return false;

  const uint8_t* a1 = frame + 4;
  const uint8_t* a2 = frame + 10;
  const uint8_t* a3 = frame + 16;
  if (toDs && !fromDs) {  // STA -> AP: a1 = BSSID, a2 = STA
    std::memcpy(out.macAp, a1, 6);
    std::memcpy(out.macSta, a2, 6);
  } else if (!toDs && fromDs) {  // AP -> STA: a2 = BSSID, a1 = STA
    std::memcpy(out.macAp, a2, 6);
    std::memcpy(out.macSta, a1, 6);
  } else {  // IBSS/WDS: fall back to a3 as BSSID
    std::memcpy(out.macAp, a3, 6);
    std::memcpy(out.macSta, a2, 6);
  }

  out.eapol = frame + hdr + 8;
  out.eapolLen = len - hdr - 8;
  return true;
}

std::string formatPmkid22000(const uint8_t pmkid[16], const uint8_t macAp[6], const uint8_t macSta[6],
                             const uint8_t* essid, const size_t essidLen) {
  std::string s = "WPA*01*";
  appendHex(s, pmkid, 16);
  s.push_back('*');
  appendHex(s, macAp, 6);
  s.push_back('*');
  appendHex(s, macSta, 6);
  s.push_back('*');
  appendHex(s, essid, essidLen);
  s += "***";  // empty anonce, eapol, messagepair
  return s;
}

std::string formatEapol22000(const uint8_t mic[16], const uint8_t macAp[6], const uint8_t macSta[6],
                             const uint8_t* essid, const size_t essidLen, const uint8_t anonce[32],
                             const uint8_t* eapol, const size_t eapolLen, const uint8_t messagePair) {
  std::string s = "WPA*02*";
  appendHex(s, mic, 16);
  s.push_back('*');
  appendHex(s, macAp, 6);
  s.push_back('*');
  appendHex(s, macSta, 6);
  s.push_back('*');
  appendHex(s, essid, essidLen);
  s.push_back('*');
  appendHex(s, anonce, 32);
  s.push_back('*');
  appendHex(s, eapol, eapolLen);
  s.push_back('*');
  appendHex(s, &messagePair, 1);
  return s;
}

}  // namespace wifiaudit
