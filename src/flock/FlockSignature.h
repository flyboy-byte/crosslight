#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Passive detection of surveillance devices (Flock ALPR cameras and similar)
// by their Wi-Fi fingerprint. Purely receive-side: the scanner listens in
// promiscuous mode and never transmits, so this observes what is already being
// broadcast into the air -- it is awareness, not interference.
//
// A signature matches a device's radio by the OUI of its MAC (the first three
// bytes, assigned per manufacturer) and/or a substring of the SSID it beacons.
// Fingerprints change as vendors rotate hardware and firmware, so signatures
// live in an SD file the user maintains (/flock/signatures.json), never baked
// into the image. This header is the pure model plus the matcher; JSON loading
// is separate (FlockSignatures.cpp) so the matcher stays dependency-free and
// host-testable.
namespace flock {

struct Signature {
  std::string name;      // human label, e.g. "Flock Falcon"
  std::string category;  // free-form tag, e.g. "alpr" / "audio"
  // 24-bit OUIs (top three MAC bytes, e.g. 0xAABBCC). A frame matches if its
  // source OUI is in this list. Empty = OUI is not part of this signature.
  std::vector<uint32_t> ouis;
  // Case-insensitive SSID substrings. A frame matches if its SSID contains any
  // of these. Empty = SSID is not part of this signature.
  std::vector<std::string> ssidContains;
};

// One sniffed frame reduced to what matching needs.
struct Observation {
  uint8_t mac[6] = {};
  std::string ssid;  // may be empty (e.g. a probe request with no SSID)
};

// Index of the first matching signature, or -1 if none match. A signature with
// both OUI and SSID criteria matches on EITHER (any listed OUI, or any listed
// substring): vendors sometimes randomize one but not the other, so requiring
// both would miss real devices. A signature with neither criterion never
// matches, so an empty/malformed entry cannot flag every device.
int match(const std::vector<Signature>& signatures, const Observation& obs);

// Top three MAC bytes as a 24-bit OUI.
uint32_t ouiOf(const uint8_t mac[6]);

// Extract the source MAC and (when present) the SSID from a raw 802.11
// management frame -- beacon, probe request, or probe response. False if the
// frame is too short or is not one of those subtypes. Pure and host-tested:
// the fixed-body offsets and tagged-parameter walk are exactly the kind of
// off-by-one that is painful to diagnose from a moving car with a serial log.
//
// `buf`/`len` are the 802.11 frame as delivered by promiscuous mode (starting
// at the MAC header, no radiotap). The SSID bytes are copied verbatim; they
// are not guaranteed to be valid UTF-8 and a hidden network yields an empty
// SSID.
bool parseManagementFrame(const uint8_t* buf, size_t len, Observation& out);

}  // namespace flock
