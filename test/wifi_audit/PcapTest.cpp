#include <gtest/gtest.h>

#include <cstdint>

#include "offensive/Pcap.h"

TEST(Pcap, GlobalHeaderBytes) {
  uint8_t h[wifiaudit::PCAP_GLOBAL_HEADER_LEN] = {};
  wifiaudit::pcapWriteGlobalHeader(h, wifiaudit::PCAP_LINKTYPE_IEEE802_11, wifiaudit::PCAP_SNAPLEN);

  // magic a1b2c3d4 stored little-endian.
  EXPECT_EQ(h[0], 0xD4);
  EXPECT_EQ(h[1], 0xC3);
  EXPECT_EQ(h[2], 0xB2);
  EXPECT_EQ(h[3], 0xA1);
  // version 2.4
  EXPECT_EQ(h[4], 0x02);
  EXPECT_EQ(h[5], 0x00);
  EXPECT_EQ(h[6], 0x04);
  EXPECT_EQ(h[7], 0x00);
  // thiszone + sigfigs are zero
  for (int i = 8; i < 16; ++i) EXPECT_EQ(h[i], 0x00);
  // snaplen 2324 = 0x00000914
  EXPECT_EQ(h[16], 0x14);
  EXPECT_EQ(h[17], 0x09);
  EXPECT_EQ(h[18], 0x00);
  EXPECT_EQ(h[19], 0x00);
  // linktype 105 = 0x69 (IEEE802_11)
  EXPECT_EQ(h[20], 0x69);
  EXPECT_EQ(h[21], 0x00);
  EXPECT_EQ(h[22], 0x00);
  EXPECT_EQ(h[23], 0x00);
}

TEST(Pcap, RecordHeaderBytesLittleEndian) {
  uint8_t r[wifiaudit::PCAP_RECORD_HEADER_LEN] = {};
  wifiaudit::pcapWriteRecordHeader(r, 0x11223344, 0x55667788, 0x000000AA, 0x000000BB);
  // ts_sec
  EXPECT_EQ(r[0], 0x44);
  EXPECT_EQ(r[1], 0x33);
  EXPECT_EQ(r[2], 0x22);
  EXPECT_EQ(r[3], 0x11);
  // ts_usec
  EXPECT_EQ(r[4], 0x88);
  EXPECT_EQ(r[5], 0x77);
  EXPECT_EQ(r[6], 0x66);
  EXPECT_EQ(r[7], 0x55);
  // incl_len
  EXPECT_EQ(r[8], 0xAA);
  EXPECT_EQ(r[9], 0x00);
  // orig_len
  EXPECT_EQ(r[12], 0xBB);
  EXPECT_EQ(r[13], 0x00);
}
