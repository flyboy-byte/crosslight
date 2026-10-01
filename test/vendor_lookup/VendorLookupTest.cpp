#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "util/VendorLookup.h"

using namespace vendordb;

namespace {
// Build an in-memory db (header + sorted records) from {key -> name} pairs.
std::vector<uint8_t> makeDb(const std::vector<std::pair<uint32_t, std::string>>& sortedEntries) {
  std::vector<uint8_t> buf(HEADER_SIZE + sortedEntries.size() * RECORD_SIZE, 0);
  std::memcpy(buf.data(), "CLV1", 4);
  const uint32_t count = static_cast<uint32_t>(sortedEntries.size());
  const uint32_t rsize = RECORD_SIZE;
  std::memcpy(buf.data() + 4, &count, 4);
  std::memcpy(buf.data() + 8, &rsize, 4);
  size_t off = HEADER_SIZE;
  for (const auto& [key, name] : sortedEntries) {
    std::memcpy(buf.data() + off, &key, 4);
    std::memcpy(buf.data() + off + 4, name.c_str(), std::min<size_t>(name.size(), NAME_LEN));
    off += RECORD_SIZE;
  }
  return buf;
}
}  // namespace

TEST(VendorLookup, ParsesValidHeader) {
  auto db = makeDb({{1, "A"}, {2, "B"}});
  uint32_t count = 0;
  EXPECT_TRUE(parseHeader(db.data(), count));
  EXPECT_EQ(count, 2u);
}

TEST(VendorLookup, RejectsBadMagic) {
  auto db = makeDb({{1, "A"}});
  db[0] = 'X';
  uint32_t count = 0;
  EXPECT_FALSE(parseHeader(db.data(), count));
}

TEST(VendorLookup, FindsFirstMiddleLast) {
  auto db = makeDb({{0x000000, "Xerox"}, {0x00AABB, "Mid"}, {0xFCFFAA, "Last"}});
  const uint8_t* recs = db.data() + HEADER_SIZE;
  EXPECT_EQ(lookupInBuffer(recs, 3, 0x000000), "Xerox");
  EXPECT_EQ(lookupInBuffer(recs, 3, 0x00AABB), "Mid");
  EXPECT_EQ(lookupInBuffer(recs, 3, 0xFCFFAA), "Last");
}

TEST(VendorLookup, MissingKeyReturnsEmpty) {
  auto db = makeDb({{10, "A"}, {20, "B"}, {30, "C"}});
  const uint8_t* recs = db.data() + HEADER_SIZE;
  EXPECT_EQ(lookupInBuffer(recs, 3, 15), "");
  EXPECT_EQ(lookupInBuffer(recs, 3, 5), "");
  EXPECT_EQ(lookupInBuffer(recs, 3, 99), "");
}

TEST(VendorLookup, EmptyDb) {
  auto db = makeDb({});
  EXPECT_EQ(lookupInBuffer(db.data() + HEADER_SIZE, 0, 1), "");
}

TEST(VendorLookup, OuiKeyFromMac) {
  const uint8_t mac[6] = {0x28, 0x6F, 0xB9, 0x11, 0x22, 0x33};
  EXPECT_EQ(ouiKey(mac), 0x286FB9u);
}

TEST(VendorLookup, NameTruncatedToField) {
  // A 32-char name fills the field exactly, with no NUL terminator inside it.
  const std::string full(NAME_LEN, 'z');
  auto db = makeDb({{1, full}});
  EXPECT_EQ(lookupInBuffer(db.data() + HEADER_SIZE, 1, 1), full);
}
