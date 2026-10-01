#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// WPA/WPA2 key-exchange parsing for the hashcat pipeline, all passive: these
// frames are captured off the air (see CaptureScanner), and *cracking* them is a
// separate offline step on a real computer -- nothing here transmits or attacks.
//
// Two capture paths:
//   * PMKID (clientless): a single EAPOL message 1 can carry an RSN PMKID, which
//     is enough for an offline hashcat 22000 "WPA*01" line. No client handshake
//     needed -- the highest-value passive catch.
//   * 4-way handshake: messages 1-4 (ANonce, SNonce, MIC) assemble into a
//     hashcat 22000 "WPA*02" line.
//
// Everything here is pure and host-tested: the 802.11/LLC/EAPOL-Key offset walk
// and the hashcat formatting are exactly the off-by-one-prone code worth pinning
// down off-device.
namespace wifiaudit {

enum class EapolMessage : uint8_t { Unknown, M1, M2, M3, M4 };

// One parsed 802.1X EAPOL-Key frame. `keyData` points into the caller's buffer.
struct EapolKey {
  uint8_t descriptorType = 0;  // 2 = RSN, 254 = WPA v1
  uint16_t keyInfo = 0;
  uint8_t nonce[32] = {};  // ANonce (AP) or SNonce (STA)
  uint8_t mic[16] = {};    // zero in M1
  bool hasMic = false;     // the Key Info MIC bit
  const uint8_t* keyData = nullptr;
  uint16_t keyDataLen = 0;
};

// Parse an 802.1X EAPOL-Key frame starting at its version byte (the byte after
// the LLC/SNAP EtherType). False if it is not a well-formed EAPOL-Key frame.
bool parseEapolKey(const uint8_t* buf, size_t len, EapolKey& out);

// Which message of the 4-way handshake a Key Info field represents (pairwise).
EapolMessage classifyEapol(uint16_t keyInfo);

// Pull the RSN PMKID (16 bytes) from an EAPOL-Key's key data (present in some M1
// frames). False if there is no PMKID KDE or it is all zero (an AP that offers
// no PMKID sends zeros).
bool extractPmkid(const EapolKey& key, uint8_t pmkidOut[16]);

// Where the EAPOL payload sits inside a raw 802.11 data frame, plus the AP/STA
// MACs worked out from the ToDS/FromDS bits.
struct EapolLocation {
  const uint8_t* eapol = nullptr;
  size_t eapolLen = 0;
  uint8_t macAp[6] = {};
  uint8_t macSta[6] = {};
};

// Locate the 802.1X/EAPOL payload (EtherType 0x888E) in a cleartext 802.11 data
// frame and fill the MACs. False if the frame is not an unencrypted EAPOL data
// frame (encrypted M3/M4 have the Protected bit set and are skipped here).
bool findEapolInDataFrame(const uint8_t* frame, size_t len, EapolLocation& out);

// hashcat 22000 "WPA*01" PMKID line (no trailing newline).
std::string formatPmkid22000(const uint8_t pmkid[16], const uint8_t macAp[6], const uint8_t macSta[6],
                             const uint8_t* essid, size_t essidLen);

// hashcat 22000 "WPA*02" handshake line. `eapol`/`eapolLen` is the M2 (or M3)
// 802.1X frame with its MIC field zeroed; `messagePair` is hashcat's message-pair
// byte. Assembling the pair from captured M1/M2 is the caller's job.
std::string formatEapol22000(const uint8_t mic[16], const uint8_t macAp[6], const uint8_t macSta[6],
                             const uint8_t* essid, size_t essidLen, const uint8_t anonce[32], const uint8_t* eapol,
                             size_t eapolLen, uint8_t messagePair);

}  // namespace wifiaudit
