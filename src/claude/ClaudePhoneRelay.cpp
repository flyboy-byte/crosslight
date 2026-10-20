#include "ClaudePhoneRelay.h"

#include <Arduino.h>
#include <Logging.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>

#include "libre/LibreBleServer.h"
#include "libre/RelayFrame.h"

namespace claude {

namespace {

constexpr size_t kPromptCapBytes = 4000;
constexpr uint32_t kUsageStaleAfterS = 600;  // like glucose: older than 10 minutes is "unknown"

uint8_t g_nextId = 0;

uint8_t newRequestId() {
  g_nextId = g_nextId == 255 ? 1 : static_cast<uint8_t>(g_nextId + 1);
  return g_nextId;
}

BridgeError mapStatus(const uint8_t status) {
  switch (status) {
    case relay::kOk:
      return BridgeError::Ok;
    case relay::kForbidden:
      return BridgeError::Forbidden;
    case relay::kBusy:
      return BridgeError::RateLimited;
    case relay::kTimeout:
      return BridgeError::Network;
    case relay::kServerError:
      return BridgeError::Unavailable;
    case relay::kBadRequest:
      return BridgeError::BadRequest;
    case relay::kNotConfigured:
      return BridgeError::NoConfig;
    case relay::kUnauthorized:
      return BridgeError::Unauthorized;
    default:
      return BridgeError::Http;
  }
}

uint32_t le32(const std::string& s, const size_t at) {
  return static_cast<uint32_t>(static_cast<uint8_t>(s[at])) | (static_cast<uint32_t>(static_cast<uint8_t>(s[at + 1])) << 8) |
         (static_cast<uint32_t>(static_cast<uint8_t>(s[at + 2])) << 16) |
         (static_cast<uint32_t>(static_cast<uint8_t>(s[at + 3])) << 24);
}

// Runs one request/response exchange. On return `reply` is valid iff error == Ok-or-mapped.
// `noPhone` is set when the phone never connected.
BridgeError exchange(libre::LibreBleServer& ble, const relay::Message& req, relay::Message& reply, bool& noPhone) {
  noPhone = false;
  const unsigned long connectStart = millis();
  while (!ble.relaySubscribed()) {
    if (millis() - connectStart > kPhoneConnectWaitMs) {
      noPhone = true;
      return BridgeError::Network;
    }
    delay(50);
  }
  ble.relayReset();
  if (!ble.relaySend(req)) return BridgeError::Network;

  const unsigned long start = millis();
  while (millis() - start < kPhoneReplyWaitMs) {
    relay::Message m;
    if (ble.relayTake(m)) {
      if (m.id != req.id || m.type != req.type) continue;  // stale reply for an earlier request
      reply = std::move(m);
      return mapStatus(reply.status);
    }
    if (!ble.relaySubscribed()) return BridgeError::Network;  // link dropped mid-request
    delay(20);
  }
  relay::Message cancel;
  cancel.type = relay::kCancel;
  cancel.id = req.id;
  ble.relaySend(cancel);
  return BridgeError::Network;
}

}  // namespace

PromptResult promptViaPhone(const std::string& prompt) {
  PromptResult out;
  if (prompt.size() > kPromptCapBytes) {
    out.error = BridgeError::TooLarge;
    return out;
  }
  libre::LibreBleServer ble;
  if (!ble.begin()) return out;  // no radio: Network

  relay::Message req;
  req.type = relay::kPrompt;
  req.id = newRequestId();
  req.payload = prompt;
  relay::Message reply;
  bool noPhone = false;
  out.error = exchange(ble, req, reply, noPhone);
  ble.end();
  if (noPhone) {
    out.errorCode = "no_phone";
    return out;
  }
  if (out.error == BridgeError::NoConfig || out.error == BridgeError::Unauthorized ||
      out.error == BridgeError::Forbidden) {
    out.errorCode = "via_phone";  // the key problem is in the companion app, not on the SD card
  }
  if (out.error == BridgeError::Ok) {
    out.reply = reply.payload;
    if (reply.truncated()) out.reply += " [cut off]";
  } else if (out.error == BridgeError::RateLimited) {
    out.retryAfterS = std::atoi(reply.payload.c_str());
  }
  LOG_INF("CLAUDE", "phone prompt: error=%d status=%u", static_cast<int>(out.error), static_cast<unsigned>(reply.status));
  return out;
}

UsageResult fetchUsageViaPhone(BridgeUsage& usage) {
  UsageResult out;
  libre::LibreBleServer ble;
  if (!ble.begin()) return out;

  relay::Message req;
  req.type = relay::kUsage;
  req.id = newRequestId();
  relay::Message reply;
  bool noPhone = false;
  out.error = exchange(ble, req, reply, noPhone);
  ble.end();
  if (noPhone) {
    out.errorCode = "no_phone";
    return out;
  }
  if (out.error == BridgeError::NoConfig || out.error == BridgeError::Unauthorized ||
      out.error == BridgeError::Forbidden) {
    out.errorCode = "via_phone";
  }
  if (out.error != BridgeError::Ok) return out;
  if (reply.payload.size() != 16) {
    out.error = BridgeError::Http;
    return out;
  }

  const uint32_t ageS = le32(reply.payload, 12);
  const bool fresh = ageS != 0xFFFFFFFFu && ageS <= kUsageStaleAfterS;
  auto parseWindow = [&](const size_t at, UsageWindow& w) {
    const uint8_t pct = static_cast<uint8_t>(reply.payload[at]);
    const uint32_t reset = le32(reply.payload, at + 1);
    // Old usage is "unknown", never a number (same rule as old glucose).
    w.hasUsedPct = fresh && pct <= 100;
    w.usedPct = w.hasUsedPct ? pct : 0;
    w.resetsAt.clear();
    if (fresh && reset != 0) {
      const time_t t = static_cast<time_t>(reset);
      struct tm g;
      gmtime_r(&t, &g);
      char buf[32];
      strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S.000Z", &g);
      w.resetsAt = buf;
    }
  };
  parseWindow(0, usage.fiveHour);
  parseWindow(6, usage.sevenDay);
  usage.state.clear();
  usage.ageS = ageS == 0xFFFFFFFFu ? 0 : static_cast<int>(ageS);
  return out;
}

}  // namespace claude
