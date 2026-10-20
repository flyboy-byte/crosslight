#pragma once

#include <string>

#include "ClaudeBridgeClient.h"

// Claude features over Bluetooth through the companion app, with the reader's
// Wi-Fi off (format: docs/crosslight/ble-claude-relay.md). Same result types as
// the Wi-Fi bridge client, so callers swap one call for the other.
//
// Each call brings the BLE server up (advertising "CrossLight"), waits for the
// phone to connect and enable relay notifications, sends one request, waits for
// the reply and tears the server down again. Blocking, like the HTTP calls.
namespace claude {

constexpr unsigned long kPhoneConnectWaitMs = 30000;
constexpr unsigned long kPhoneReplyWaitMs = 60000;

// errorCode "no_phone" when no phone connected in time (error is Network).
PromptResult promptViaPhone(const std::string& prompt);
UsageResult fetchUsageViaPhone(BridgeUsage& usage);

}  // namespace claude
