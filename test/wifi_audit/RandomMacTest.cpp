#include <gtest/gtest.h>

#include "wifiaudit/RandomMac.h"

using wifiaudit::makeLocallyAdministered;

TEST(RandomMac, SetsLocallyAdministeredBit) {
  uint8_t mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
  makeLocallyAdministered(mac);
  EXPECT_EQ(mac[0] & 0x02, 0x02);
}

TEST(RandomMac, ClearsMulticastBit) {
  uint8_t mac[6] = {0xFF, 0x11, 0x22, 0x33, 0x44, 0x55};
  makeLocallyAdministered(mac);
  EXPECT_EQ(mac[0] & 0x01, 0x00);
}

TEST(RandomMac, OnlyTouchesFirstOctet) {
  uint8_t mac[6] = {0x00, 0xAB, 0xCD, 0xEF, 0x12, 0x34};
  makeLocallyAdministered(mac);
  EXPECT_EQ(mac[1], 0xAB);
  EXPECT_EQ(mac[2], 0xCD);
  EXPECT_EQ(mac[3], 0xEF);
  EXPECT_EQ(mac[4], 0x12);
  EXPECT_EQ(mac[5], 0x34);
}

TEST(RandomMac, AllOnesBecomesValidLocallyAdministeredUnicast) {
  uint8_t mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  makeLocallyAdministered(mac);
  EXPECT_EQ(mac[0], 0xFE);  // 0x02 set, 0x01 cleared
}
