#pragma once

// One-time, per-boot confirmation that the operator has accepted the active-audit
// warning. The active tools (deauth/disassoc/beacon transmit) put energy on the
// air and are lawful only against hardware the operator is authorized to test, so
// nothing transmits until the UI has shown the warning once and called confirm().
//
// The state is a single process-wide bool that starts false every boot and is NOT
// persisted: re-confirming after a reboot is deliberate, so a freshly powered
// device can never transmit by accident. AttackTx consults this before it touches
// the radio, independently of whatever the UI does -- defense in depth. Keeping it
// a trivial static makes it host-testable with no dependencies.
namespace wifiaudit {

class ActiveAuditGate {
 public:
  // True only after confirm() has been called since boot (or since the last
  // reset()). Starts false.
  static bool confirmed();

  // Record that the operator accepted the one-time active-audit warning.
  static void confirm();

  // Clear the confirmation. Called on reboot / when leaving the audit screen, and
  // used by host tests to return to the initial state.
  static void reset();
};

}  // namespace wifiaudit
