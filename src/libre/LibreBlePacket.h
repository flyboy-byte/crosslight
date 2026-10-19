#pragma once

#include <cstddef>
#include <cstdint>

#include "LibreBridgeClient.h"

// Decoder for the frozen companion-app -> receiver BLE packet shared with the
// PineTime watch (extraction repo: docs/pinetime.md "Data contract", minted
// UUIDs D-020). Pure, host-testable logic only -- no NimBLE/Arduino here; the
// GATT server (LibreBleServer) hands bytes to decodeBlePacket() and feeds the
// result through bleReadingFromPacket() before touching the UI.
namespace libre {

constexpr size_t kBlePacketSize = 8;
constexpr uint16_t kBleNoValueMgdl = 0xFFFF;

// 10 minutes. The relay judges freshness by RECEIPT time (how long since the
// reader last got a write), not by the packet's own reading_time against the
// reader's clock -- a reader without Wi-Fi has no trustworthy clock, so an
// otherwise-fine reading used to render Unavailable after any reboot. Receipt
// time comes from millis() uptime, which no reboot can corrupt. The activity
// owns that comparison; this constant is the window it uses.
constexpr uint32_t kBleStaleAfterS = 600;
constexpr uint32_t kBleStaleAfterMs = kBleStaleAfterS * 1000;

struct BlePacket {
  uint16_t valueMgdl = kBleNoValueMgdl;
  Trend trend = Trend::Unknown;
  bool high = false;
  bool low = false;
  bool unavailable = false;
  uint32_t readingTimeUnix = 0;  // unix UTC seconds of the companion's source_time
};

// Decodes exactly `kBlePacketSize` bytes: u16 value_mgdl LE, u8 trend, u8
// flags (bit0 HIGH, bit1 LOW, bit2 UNAVAILABLE), u32 reading_time LE. Returns
// false (leaving `out` untouched) for a null pointer or any length other than
// kBlePacketSize -- a contract change means a new characteristic UUID per
// D-020, never a length that this quietly tolerates.
bool decodeBlePacket(const uint8_t* data, size_t len, BlePacket& out);

// Turns a decoded packet into the same Reading shape the Wi-Fi path produces,
// so the Glucose Monitor screen renders either source identically. Pure
// function of the packet's own contents -- status comes from the flags and the
// value sentinel only:
//   - HIGH / LOW flag  -> High / Low (value kept, but never shown for these)
//   - UNAVAILABLE flag or 0xFFFF sentinel -> Unavailable
//   - otherwise         -> Ok with the value.
// Freshness is NOT decided here: the caller tracks when the last write arrived
// (receipt time) and forces Unavailable once that exceeds kBleStaleAfterMs,
// since the reader's own clock is not trustworthy offline. ageS is left 0 for
// the caller to fill from receipt time.
Reading bleReadingFromPacket(const BlePacket& packet);

}  // namespace libre
