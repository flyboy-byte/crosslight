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

// 10 minutes -- same cutoff as agent-api.md, the laptop widget, and the
// Wi-Fi path's own STALE handling. Frozen alongside the packet contract.
constexpr uint32_t kBleStaleAfterS = 600;

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

// Turns a decoded packet plus a trusted "now" into the same Reading shape the
// Wi-Fi path produces, so the Glucose Monitor screen renders either source
// identically.
//
// `nowUnix` must come from a trusted clock (trustedtime::trustedNow() on
// device); pass <= 0 to mean "no trusted clock available". This relay's own
// rule (extraction DECISIONS.md D-017) is stricter than the Wi-Fi path's own
// STALE status: once a reading is untimed, future-dated (clock skew) or
// older than kBleStaleAfterS, NO NUMBER is shown at all -- so those cases map
// to ReadingStatus::Unavailable (which already renders without one) rather
// than Stale (which still renders the last known value on the Wi-Fi path).
Reading bleReadingFromPacket(const BlePacket& packet, int64_t nowUnix);

}  // namespace libre
