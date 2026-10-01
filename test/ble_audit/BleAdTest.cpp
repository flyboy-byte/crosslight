#include <gtest/gtest.h>

#include <vector>

#include "bleaudit/BleAd.h"

namespace {

// Build one AD structure: [length][type][data...], length = 1 (type) + data.
std::vector<uint8_t> ad(const uint8_t type, const std::vector<uint8_t>& data) {
  std::vector<uint8_t> s;
  s.push_back(static_cast<uint8_t>(1 + data.size()));
  s.push_back(type);
  s.insert(s.end(), data.begin(), data.end());
  return s;
}

void append(std::vector<uint8_t>& dst, const std::vector<uint8_t>& src) {
  dst.insert(dst.end(), src.begin(), src.end());
}

bleaudit::BleAdvertisement parse(const std::vector<uint8_t>& buf) {
  bleaudit::BleAdvertisement out;
  EXPECT_TRUE(bleaudit::parseAdvertisement(buf.data(), buf.size(), out));
  return out;
}

}  // namespace

TEST(BleAd, EmptyPayloadReturnsFalse) {
  bleaudit::BleAdvertisement out;
  EXPECT_FALSE(bleaudit::parseAdvertisement(nullptr, 0, out));
  const uint8_t dummy = 0x00;
  EXPECT_FALSE(bleaudit::parseAdvertisement(&dummy, 0, out));
}

TEST(BleAd, ParsesCompleteLocalName) {
  const auto out = parse(ad(0x09, {'F', 'l', 'i', 'p', 'p', 'e', 'r'}));
  EXPECT_EQ(out.name, "Flipper");
}

TEST(BleAd, ParsesShortenedLocalName) {
  const auto out = parse(ad(0x08, {'A', 'B', 'C'}));
  EXPECT_EQ(out.name, "ABC");
}

TEST(BleAd, CompleteNameOverridesShortened) {
  std::vector<uint8_t> buf;
  append(buf, ad(0x08, {'s', 'h', 'o', 'r', 't'}));
  append(buf, ad(0x09, {'c', 'o', 'm', 'p', 'l', 'e', 't', 'e'}));
  EXPECT_EQ(parse(buf).name, "complete");
}

TEST(BleAd, ShortenedDoesNotClobberComplete) {
  std::vector<uint8_t> buf;
  append(buf, ad(0x09, {'c', 'o', 'm', 'p', 'l', 'e', 't', 'e'}));
  append(buf, ad(0x08, {'s', 'h', 'o', 'r', 't'}));
  EXPECT_EQ(parse(buf).name, "complete");
}

TEST(BleAd, ManufacturerCompanyIdIsLittleEndian) {
  // 0xFF manufacturer data, company ID bytes 4C 00 -> 0x004C.
  const auto out = parse(ad(0xFF, {0x4C, 0x00, 0x10, 0x05}));
  EXPECT_TRUE(out.hasManufacturerData);
  EXPECT_TRUE(out.hasCompanyId);
  EXPECT_EQ(out.companyId, 0x004C);
}

TEST(BleAd, AppleTypeByteDecodedForAppleCompany) {
  // Apple (0x004C) then type 0x12 (FindMy).
  const auto out = parse(ad(0xFF, {0x4C, 0x00, 0x12, 0x19, 0x00}));
  EXPECT_TRUE(out.hasCompanyId);
  EXPECT_EQ(out.companyId, 0x004C);
  EXPECT_TRUE(out.hasAppleType);
  EXPECT_EQ(out.appleType, 0x12);
}

TEST(BleAd, AppleTypeNotSetForNonAppleCompany) {
  // Microsoft (0x0006) manufacturer data: company decoded, but no Apple type.
  const auto out = parse(ad(0xFF, {0x06, 0x00, 0x12}));
  EXPECT_TRUE(out.hasCompanyId);
  EXPECT_EQ(out.companyId, 0x0006);
  EXPECT_FALSE(out.hasAppleType);
}

TEST(BleAd, AppleTypeNeedsTypeByte) {
  // Apple company ID but no following type byte.
  const auto out = parse(ad(0xFF, {0x4C, 0x00}));
  EXPECT_TRUE(out.hasCompanyId);
  EXPECT_EQ(out.companyId, 0x004C);
  EXPECT_FALSE(out.hasAppleType);
}

TEST(BleAd, ManufacturerDataTooShortForCompanyId) {
  // A single manufacturer byte: present, but not a full company ID.
  const auto out = parse(ad(0xFF, {0x4C}));
  EXPECT_TRUE(out.hasManufacturerData);
  EXPECT_FALSE(out.hasCompanyId);
}

TEST(BleAd, Parses16BitServiceUuidsLittleEndian) {
  // Two UUIDs: ED FE -> 0xFEED, AA FE -> 0xFEAA.
  const auto out = parse(ad(0x03, {0xED, 0xFE, 0xAA, 0xFE}));
  ASSERT_EQ(out.serviceUuids16.size(), 2u);
  EXPECT_EQ(out.serviceUuids16[0], 0xFEED);
  EXPECT_EQ(out.serviceUuids16[1], 0xFEAA);
}

TEST(BleAd, Incomplete16BitServiceUuidsAlsoRead) {
  const auto out = parse(ad(0x02, {0x2C, 0xFE}));
  ASSERT_EQ(out.serviceUuids16.size(), 1u);
  EXPECT_EQ(out.serviceUuids16[0], 0xFE2C);
}

TEST(BleAd, OddTrailingByteIn16BitListIsIgnored) {
  // 3 data bytes: one full UUID (AD DE -> 0xDEAD) plus a stray byte.
  const auto out = parse(ad(0x03, {0xAD, 0xDE, 0xFF}));
  ASSERT_EQ(out.serviceUuids16.size(), 1u);
  EXPECT_EQ(out.serviceUuids16[0], 0xDEAD);
}

TEST(BleAd, Parses128BitServiceUuidRawBytes) {
  std::vector<uint8_t> uuid(16);
  for (int i = 0; i < 16; ++i) uuid[i] = static_cast<uint8_t>(i + 1);
  const auto out = parse(ad(0x07, uuid));
  ASSERT_EQ(out.serviceUuids128.size(), 1u);
  for (int i = 0; i < 16; ++i) EXPECT_EQ(out.serviceUuids128[0][i], static_cast<uint8_t>(i + 1));
}

TEST(BleAd, Partial128BitUuidIsIgnored) {
  const auto out = parse(ad(0x06, std::vector<uint8_t>(15, 0xAB)));  // one byte short
  EXPECT_TRUE(out.serviceUuids128.empty());
}

TEST(BleAd, ParsesFlags) {
  const auto out = parse(ad(0x01, {0x06}));
  EXPECT_TRUE(out.hasFlags);
  EXPECT_EQ(out.flags, 0x06);
}

TEST(BleAd, ParsesMultipleStructures) {
  std::vector<uint8_t> buf;
  append(buf, ad(0x01, {0x06}));                          // flags
  append(buf, ad(0x09, {'T', 'i', 'l', 'e'}));            // name
  append(buf, ad(0x03, {0xED, 0xFE}));                    // service UUID 0xFEED
  append(buf, ad(0xFF, {0x4C, 0x00, 0x07}));              // Apple, type 0x07
  const auto out = parse(buf);
  EXPECT_TRUE(out.hasFlags);
  EXPECT_EQ(out.flags, 0x06);
  EXPECT_EQ(out.name, "Tile");
  ASSERT_EQ(out.serviceUuids16.size(), 1u);
  EXPECT_EQ(out.serviceUuids16[0], 0xFEED);
  EXPECT_EQ(out.companyId, 0x004C);
  EXPECT_EQ(out.appleType, 0x07);
}

TEST(BleAd, TruncatedStructureDoesNotOverread) {
  // A valid flags structure, then a name structure that claims 10 data bytes but
  // supplies only 3. The claimed length overruns the buffer, so it is skipped --
  // the flags before it must still be parsed, with no over-read.
  std::vector<uint8_t> buf = {0x02, 0x01, 0x06, 0x0B, 0x09, 'a', 'b', 'c'};
  const auto out = parse(buf);
  EXPECT_TRUE(out.hasFlags);
  EXPECT_EQ(out.flags, 0x06);
  EXPECT_TRUE(out.name.empty());
}

TEST(BleAd, ZeroLengthByteTerminatesCleanly) {
  // A valid name, then a zero length byte (padding), then junk that must not be
  // read.
  std::vector<uint8_t> buf;
  append(buf, ad(0x09, {'O', 'K'}));
  buf.push_back(0x00);  // early-termination length
  buf.push_back(0xAA);  // padding -- must be ignored
  buf.push_back(0xBB);
  const auto out = parse(buf);
  EXPECT_EQ(out.name, "OK");
}

TEST(BleAd, SingleZeroByteYieldsEmptyAdvertisement) {
  const uint8_t buf = 0x00;
  bleaudit::BleAdvertisement out;
  ASSERT_TRUE(bleaudit::parseAdvertisement(&buf, 1, out));
  EXPECT_TRUE(out.name.empty());
  EXPECT_FALSE(out.hasCompanyId);
  EXPECT_FALSE(out.hasFlags);
  EXPECT_TRUE(out.serviceUuids16.empty());
}

TEST(BleAd, OutIsResetBetweenParses) {
  // A field set by a first parse must not survive into a second.
  bleaudit::BleAdvertisement out;
  const auto first = ad(0x09, {'X'});
  ASSERT_TRUE(bleaudit::parseAdvertisement(first.data(), first.size(), out));
  EXPECT_EQ(out.name, "X");
  const auto second = ad(0x01, {0x04});
  ASSERT_TRUE(bleaudit::parseAdvertisement(second.data(), second.size(), out));
  EXPECT_TRUE(out.name.empty());
  EXPECT_TRUE(out.hasFlags);
}
