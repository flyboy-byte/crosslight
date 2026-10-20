#include "RelayFrame.h"

#include <algorithm>

namespace relay {

std::vector<std::string> encode(const Message& msg, const size_t maxFrame) {
  std::vector<std::string> frames;
  const size_t firstRoom = maxFrame > kHeaderSize + kFirstExtra ? maxFrame - kHeaderSize - kFirstExtra : 0;
  const size_t contRoom = maxFrame > kHeaderSize ? maxFrame - kHeaderSize : 0;
  if (firstRoom == 0 || contRoom == 0 || msg.payload.size() > kMaxPayload) return frames;

  const size_t total = msg.payload.size();
  size_t pos = 0;
  bool first = true;
  do {
    const size_t room = first ? firstRoom : contRoom;
    const size_t n = std::min(room, total - pos);
    const bool more = pos + n < total;
    std::string f;
    f.push_back(static_cast<char>(msg.type));
    f.push_back(static_cast<char>(msg.id));
    f.push_back(static_cast<char>(msg.status));
    // TRUNCATED rides on the first frame only; MORE is per frame.
    uint8_t flags = more ? kFlagMore : 0;
    if (first) flags |= (msg.flags & kFlagTruncated);
    f.push_back(static_cast<char>(flags));
    if (first) {
      f.push_back(static_cast<char>(total & 0xFF));
      f.push_back(static_cast<char>((total >> 8) & 0xFF));
    }
    f.append(msg.payload, pos, n);
    frames.push_back(std::move(f));
    pos += n;
    first = false;
  } while (pos < total);
  return frames;
}

void Reassembler::reset() {
  open = false;
  expected = 0;
  current = Message{};
}

bool Reassembler::feed(const uint8_t* data, const size_t len) {
  if (len < kHeaderSize) return false;
  const uint8_t type = data[0];
  const uint8_t id = data[1];
  const uint8_t status = data[2];
  const uint8_t flags = data[3];

  size_t payloadAt = kHeaderSize;
  if (!open) {
    if (len < kHeaderSize + kFirstExtra) return false;
    current = Message{};
    current.type = type;
    current.id = id;
    current.status = status;
    current.flags = flags & kFlagTruncated;
    expected = static_cast<size_t>(data[4]) | (static_cast<size_t>(data[5]) << 8);
    payloadAt = kHeaderSize + kFirstExtra;
    open = true;
  } else if (type != current.type || id != current.id) {
    return false;  // stray continuation for another request
  }

  current.payload.append(reinterpret_cast<const char*>(data + payloadAt), len - payloadAt);
  if ((flags & kFlagMore) != 0 && current.payload.size() <= expected) return false;

  // Message over (MORE clear, or more data than declared): verify the length.
  done = std::move(current);
  if (done.payload.size() != expected || (flags & kFlagMore) != 0) {
    done.status = kBadRequest;
    done.payload.clear();
    done.flags &= static_cast<uint8_t>(~kFlagTruncated);
  }
  done.flags &= static_cast<uint8_t>(~kFlagMore);
  reset();
  return true;
}

Message Reassembler::take() { return std::move(done); }

}  // namespace relay
