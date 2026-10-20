#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Frame codec for the Claude relay over BLE (docs/crosslight/ble-claude-relay.md,
// agreed with the companion app 2026-10-19). Pure and radio-free so it is host
// tested; the GATT plumbing lives in LibreBleServer.
//
//   byte 0 type, byte 1 request id, byte 2 status, byte 3 flags
//   first frame of a message only: bytes 4-5 total payload length (u16 LE)
//   continuation frames: payload follows byte 3 directly
namespace relay {

enum Type : uint8_t { kPrompt = 1, kUsage = 2, kCancel = 4 };

enum Status : uint8_t {
  kOk = 0,
  kForbidden = 1,
  kBusy = 2,
  kTimeout = 3,
  kServerError = 4,
  kBadRequest = 5,
  kNotConfigured = 6,
  kUnauthorized = 7,
  kOther = 255,
};

constexpr uint8_t kFlagMore = 1;
constexpr uint8_t kFlagTruncated = 2;
constexpr size_t kHeaderSize = 4;
constexpr size_t kFirstExtra = 2;  // u16 total length
constexpr size_t kMinFrame = kHeaderSize + kFirstExtra + 1;
constexpr size_t kMaxPayload = 65535;

struct Message {
  uint8_t type = 0;
  uint8_t id = 0;
  uint8_t status = 0;
  uint8_t flags = 0;  // MORE is cleared on a completed message
  std::string payload;
  bool truncated() const { return (flags & kFlagTruncated) != 0; }
};

// Splits a message into frames of at most `maxFrame` bytes (ATT payload, i.e.
// MTU - 3). An empty payload is a single header+length frame.
std::vector<std::string> encode(const Message& msg, size_t maxFrame);

// Reassembles frames into messages. Malformed input never yields a partial
// message: a length mismatch ends the message with status kBadRequest and an
// empty payload; a continuation for a different type/id is dropped.
class Reassembler {
 public:
  // Returns true when a message completed with this frame (take it with take()).
  bool feed(const uint8_t* data, size_t len);
  Message take();
  void reset();

 private:
  bool open = false;
  size_t expected = 0;
  Message current;
  Message done;
};

}  // namespace relay
