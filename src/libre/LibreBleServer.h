#pragma once

#include "LibreBlePacket.h"
#include "RelayFrame.h"

// NimBLE GATT peripheral/server for the companion app's glucose relay
// (frozen contract + UUIDs: extraction repo docs/pinetime.md, DECISIONS.md
// D-017/D-020). Mirrors bleaudit::BleScanner's shape -- minimal work in the
// NimBLE host-task callback (copy bytes into a fixed queue), real decoding
// on the UI task via drain-on-poll -- but as the GATT *server* role, which
// nothing else in this codebase uses yet (BleScanner/BleSpoofer are
// central/broadcaster only).
//
// Bonded-only: the characteristic requires an encrypted link to write
// (NIMBLE_PROPERTY::WRITE_ENC), so an unbonded device's write is rejected by
// the BLE stack itself, never reaching application code. Foreground-only
// like the rest of Utilities: begin()/end() bracket the Glucose Monitor
// screen's lifetime, matching GlucoseMonitorActivity's Wi-Fi radio
// discipline (radio only up while the screen is open).
//
// One held connection, not connect-per-reading (crosslight/PLAN.md "Planned:
// Bluetooth glucose relay", power design): on connect, this requests a long
// connection interval + slave latency, since the companion only writes about
// once a minute -- there's no reason to keep the radio awake faster than
// that between writes.
namespace libre {

constexpr char kBleServiceUuid[] = "0c0cd14d-6582-46f7-91b6-b4cf2a7cebac";
constexpr char kBleCharacteristicUuid[] = "4e9715ab-ed4d-4a77-b8e7-f70fb5e26fc5";
// Time sync (minted 2026-10-19, same service): write 4 bytes, u32 little-endian
// unix UTC seconds. The reader sets its clock from it; no response data.
constexpr char kBleTimeCharacteristicUuid[] = "84b38d8a-686e-4663-bbe8-73ba7d3e1774";
// Claude relay (docs/crosslight/ble-claude-relay.md): TX = reader -> phone NOTIFY,
// RX = phone -> reader WRITE. Frame format in RelayFrame.h.
constexpr char kBleRelayTxUuid[] = "a65f2d9a-0d09-4190-a32b-63ba6c91daac";
constexpr char kBleRelayRxUuid[] = "b9e7080d-9fd5-4a71-ad1e-67186b5b2d9d";

class LibreBleServer {
 public:
  // Brings up the GATT server/advertising. False if there's no radio (e.g.
  // the desktop simulator, which does not emulate BLE) or it's already
  // running.
  bool begin();
  // Stops advertising and disconnects any peer. Safe if begin() failed or
  // was never called.
  void end();
  bool running() const { return active; }
  bool connected() const;

  // Drains whatever the companion app has written since the last call.
  // Returns true and fills `out` with the most recent valid packet if one
  // arrived; a burst of writes between polls only ever yields the latest --
  // older ones are dropped, since only the newest reading is ever shown.
  bool takeLatest(BlePacket& out);

  // Returns true with the phone's UTC epoch if a time-sync write arrived since
  // the last call. Range-checked (2025-2100); out-of-range writes are dropped.
  bool takeTimeSync(int64_t& epochOut);

  // --- Claude relay ---
  // True once the phone has enabled notifications on the relay TX characteristic
  // (the reader must not notify a request before that).
  bool relaySubscribed() const;
  // Sends one message as notify frames sized to the negotiated MTU. False if
  // not subscribed or a notify failed.
  bool relaySend(const relay::Message& msg);
  // Drains received frames; true with a completed message in `out`.
  bool relayTake(relay::Message& out);
  // Drops any half-assembled message and queued frames (start of a new request).
  void relayReset();

 private:
  bool active = false;
};

}  // namespace libre
