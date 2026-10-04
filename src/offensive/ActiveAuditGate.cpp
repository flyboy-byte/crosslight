#include "ActiveAuditGate.h"

namespace wifiaudit {

namespace {
// A function-local static, so the flag has exactly one instance with no
// static-initialization-order worry, and reads as false before the first write.
// The default is the safe one: "not confirmed -> do not transmit."
bool& gateState() {
  static bool confirmedThisBoot = false;
  return confirmedThisBoot;
}
}  // namespace

bool ActiveAuditGate::confirmed() { return gateState(); }

void ActiveAuditGate::confirm() { gateState() = true; }

void ActiveAuditGate::reset() { gateState() = false; }

}  // namespace wifiaudit
