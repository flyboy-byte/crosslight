#pragma once

#include <HalStorage.h>

#include <cstdint>
#include <string>

#include "util/VendorLookup.h"

// On-SD vendor-name lookup: binary-searches one of the fixed-width sorted
// databases (scripts/gen_vendor_db.py -> /vendordb/oui.bin, /vendordb/btcid.bin)
// directly on the card, so a 40k-entry registry costs ~0 RAM and ~0 flash and a
// lookup is ~log2(N) seek+reads of one 36-byte record. The parse/search core is
// VendorLookup.h (host-tested); this class is just the HalFile I/O around it.
//
// Open once per scan session, reuse for every device, close on exit. A lookup
// miss (or a missing db file) returns "" -- vendor labels are a nice-to-have, so
// the tools work fine without the db on the card.
class VendorDb {
 public:
  // Opens `path` and validates the header. False (and stays closed) if the file
  // is missing or not a CLV1 db. Safe to call when the file isn't present.
  bool open(const char* path);
  bool isOpen() const { return file.isOpen(); }
  void close();

  // Name for `key`, or "" if closed / not found. `key` is an OUI (0x00AABBCC)
  // for oui.bin or a 16-bit company id for btcid.bin.
  std::string lookup(uint32_t key);

 private:
  HalFile file;
  uint32_t count = 0;
};
