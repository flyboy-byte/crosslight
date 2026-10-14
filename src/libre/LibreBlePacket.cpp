#include "LibreBlePacket.h"

namespace libre {

namespace {

uint16_t readU16LE(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

uint32_t readU32LE(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

Trend trendFromWire(const uint8_t v) {
  switch (v) {
    case 1:
      return Trend::FallingFast;
    case 2:
      return Trend::Falling;
    case 3:
      return Trend::Steady;
    case 4:
      return Trend::Rising;
    case 5:
      return Trend::RisingFast;
    default:
      return Trend::Unknown;  // wire 0, or anything undefined
  }
}

}  // namespace

bool decodeBlePacket(const uint8_t* data, const size_t len, BlePacket& out) {
  if (!data || len != kBlePacketSize) return false;

  BlePacket p;
  p.valueMgdl = readU16LE(data);
  p.trend = trendFromWire(data[2]);
  const uint8_t flags = data[3];
  p.high = (flags & 0x01) != 0;
  p.low = (flags & 0x02) != 0;
  p.unavailable = (flags & 0x04) != 0;
  p.readingTimeUnix = readU32LE(data + 4);
  out = p;
  return true;
}

Reading bleReadingFromPacket(const BlePacket& packet, const int64_t nowUnix) {
  Reading r;
  r.trend = packet.trend;
  r.unit = "mg/dL";

  if (nowUnix <= 0) {
    r.status = ReadingStatus::Unavailable;
    return r;
  }

  const int64_t ageS = nowUnix - static_cast<int64_t>(packet.readingTimeUnix);
  if (ageS < 0 || static_cast<uint32_t>(ageS) >= kBleStaleAfterS) {
    r.status = ReadingStatus::Unavailable;
    return r;
  }
  r.ageS = static_cast<int>(ageS);

  if (packet.unavailable || packet.valueMgdl == kBleNoValueMgdl) {
    r.status = ReadingStatus::Unavailable;
    return r;
  }
  r.value = packet.valueMgdl;
  r.status = packet.high ? ReadingStatus::High : packet.low ? ReadingStatus::Low : ReadingStatus::Ok;
  return r;
}

}  // namespace libre
