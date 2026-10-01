#include <gtest/gtest.h>

#include "bleaudit/BleBeacon.h"

using bleaudit::buildIBeaconManufacturerData;
using bleaudit::IBEACON_MANUF_LEN;

namespace {
const uint8_t kUuid[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                           0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
}  // namespace

TEST(BleBeacon, WritesFullLength) {
  uint8_t out[IBEACON_MANUF_LEN];
  EXPECT_EQ(buildIBeaconManufacturerData(out, sizeof(out), kUuid, 0, 0, -59), IBEACON_MANUF_LEN);
}

TEST(BleBeacon, HeaderIsAppleIBeacon) {
  uint8_t out[IBEACON_MANUF_LEN];
  buildIBeaconManufacturerData(out, sizeof(out), kUuid, 0, 0, -59);
  EXPECT_EQ(out[0], 0x4C);  // company id little-endian 0x004C
  EXPECT_EQ(out[1], 0x00);
  EXPECT_EQ(out[2], 0x02);  // iBeacon type
  EXPECT_EQ(out[3], 0x15);  // length 21
}

TEST(BleBeacon, CopiesUuidInOrder) {
  uint8_t out[IBEACON_MANUF_LEN];
  buildIBeaconManufacturerData(out, sizeof(out), kUuid, 0, 0, -59);
  for (int i = 0; i < 16; ++i) EXPECT_EQ(out[4 + i], kUuid[i]) << "uuid byte " << i;
}

TEST(BleBeacon, MajorAndMinorAreBigEndian) {
  uint8_t out[IBEACON_MANUF_LEN];
  buildIBeaconManufacturerData(out, sizeof(out), kUuid, 0x1234, 0xABCD, -59);
  EXPECT_EQ(out[20], 0x12);  // major MSB
  EXPECT_EQ(out[21], 0x34);  // major LSB
  EXPECT_EQ(out[22], 0xAB);  // minor MSB
  EXPECT_EQ(out[23], 0xCD);  // minor LSB
}

TEST(BleBeacon, TxPowerIsLastByte) {
  uint8_t out[IBEACON_MANUF_LEN];
  buildIBeaconManufacturerData(out, sizeof(out), kUuid, 0, 0, -59);
  EXPECT_EQ(static_cast<int8_t>(out[24]), -59);
}

TEST(BleBeacon, RefusesTooSmallBuffer) {
  uint8_t out[IBEACON_MANUF_LEN - 1];
  EXPECT_EQ(buildIBeaconManufacturerData(out, sizeof(out), kUuid, 0, 0, -59), 0u);
}
