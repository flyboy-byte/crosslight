#include <gtest/gtest.h>

#include "bleaudit/BleSignature.h"

namespace {

bleaudit::BleSignature sigCompany(const std::string& name, const uint16_t companyId) {
  bleaudit::BleSignature s;
  s.name = name;
  s.category = "test";
  s.companyId = companyId;
  s.hasCompanyId = true;
  return s;
}

bleaudit::BleSignature sigService(const std::string& name, const std::vector<uint16_t>& uuids) {
  bleaudit::BleSignature s;
  s.name = name;
  s.category = "test";
  s.serviceUuids16 = uuids;
  return s;
}

bleaudit::BleSignature sigName(const std::string& name, const std::vector<std::string>& contains) {
  bleaudit::BleSignature s;
  s.name = name;
  s.category = "test";
  s.nameContains = contains;
  return s;
}

bleaudit::BleAdvertisement advCompany(const uint16_t companyId) {
  bleaudit::BleAdvertisement a;
  a.companyId = companyId;
  a.hasCompanyId = true;
  return a;
}

bleaudit::BleAdvertisement advService(const std::vector<uint16_t>& uuids) {
  bleaudit::BleAdvertisement a;
  a.serviceUuids16 = uuids;
  return a;
}

bleaudit::BleAdvertisement advName(const std::string& name) {
  bleaudit::BleAdvertisement a;
  a.name = name;
  return a;
}

}  // namespace

TEST(BleMatcher, MatchesByCompanyId) {
  const std::vector<bleaudit::BleSignature> sigs = {sigCompany("Apple", 0x004C)};
  EXPECT_EQ(bleaudit::matchBle(sigs, advCompany(0x004C)), 0);
  EXPECT_EQ(bleaudit::matchBle(sigs, advCompany(0x0006)), -1);
}

TEST(BleMatcher, CompanySignatureNeedsCompanyInAdvert) {
  // The advert carries no manufacturer data, so a company signature cannot match.
  const std::vector<bleaudit::BleSignature> sigs = {sigCompany("Apple", 0x004C)};
  bleaudit::BleAdvertisement a;  // hasCompanyId == false
  EXPECT_EQ(bleaudit::matchBle(sigs, a), -1);
}

TEST(BleMatcher, MatchesByServiceUuidAnyInList) {
  const std::vector<bleaudit::BleSignature> sigs = {sigService("Samsung", {0xFD5A, 0xFD59})};
  EXPECT_EQ(bleaudit::matchBle(sigs, advService({0xFD59})), 0);
  EXPECT_EQ(bleaudit::matchBle(sigs, advService({0x1234, 0xFD5A})), 0);
  EXPECT_EQ(bleaudit::matchBle(sigs, advService({0x1234})), -1);
}

TEST(BleMatcher, MatchesByNameSubstringCaseInsensitive) {
  const std::vector<bleaudit::BleSignature> sigs = {sigName("Flipper", {"flipper"})};
  EXPECT_EQ(bleaudit::matchBle(sigs, advName("Flipper a1b2")), 0);
  EXPECT_EQ(bleaudit::matchBle(sigs, advName("myFLIPPERzero")), 0);
  EXPECT_EQ(bleaudit::matchBle(sigs, advName("Garmin")), -1);
}

TEST(BleMatcher, EmptyNameDoesNotMatchNameRule) {
  const std::vector<bleaudit::BleSignature> sigs = {sigName("Flipper", {"flipper"})};
  EXPECT_EQ(bleaudit::matchBle(sigs, advName("")), -1);
}

TEST(BleMatcher, AnyCriterionMatches) {
  // One signature with all three criteria; an advert satisfying only one still
  // matches, because devices rotate or omit individual fields.
  bleaudit::BleSignature s = sigCompany("Combo", 0x004C);
  s.serviceUuids16 = {0xFEED};
  s.nameContains = {"tile"};
  const std::vector<bleaudit::BleSignature> sigs = {s};
  EXPECT_EQ(bleaudit::matchBle(sigs, advCompany(0x004C)), 0);
  EXPECT_EQ(bleaudit::matchBle(sigs, advService({0xFEED})), 0);
  EXPECT_EQ(bleaudit::matchBle(sigs, advName("my Tile tracker")), 0);
  // None of the three present -> no match.
  EXPECT_EQ(bleaudit::matchBle(sigs, advName("something else")), -1);
}

TEST(BleMatcher, EmptySignatureNeverMatches) {
  // No criteria at all: must not flag every device on the air.
  bleaudit::BleSignature blank;
  blank.name = "Blank";
  const std::vector<bleaudit::BleSignature> sigs = {blank};
  EXPECT_EQ(bleaudit::matchBle(sigs, advCompany(0x004C)), -1);
  EXPECT_EQ(bleaudit::matchBle(sigs, advName("anything")), -1);
}

TEST(BleMatcher, ReturnsFirstMatchingIndex) {
  const std::vector<bleaudit::BleSignature> sigs = {sigCompany("A", 0x1111), sigCompany("B", 0x004C),
                                                    sigCompany("C", 0x004C)};
  EXPECT_EQ(bleaudit::matchBle(sigs, advCompany(0x004C)), 1);
}

TEST(BleMatcher, NoSignaturesNoMatch) {
  const std::vector<bleaudit::BleSignature> sigs;
  EXPECT_EQ(bleaudit::matchBle(sigs, advCompany(0x004C)), -1);
}
