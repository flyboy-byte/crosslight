#include <gtest/gtest.h>

#include "offensive/ActiveAuditGate.h"

using wifiaudit::ActiveAuditGate;

// The gate is process-wide static state shared across tests, so each test resets
// it first to stay independent of execution order.

TEST(ActiveAuditGate, FalseBeforeAnyConfirm) {
  ActiveAuditGate::reset();
  EXPECT_FALSE(ActiveAuditGate::confirmed());
}

TEST(ActiveAuditGate, TrueAfterConfirm) {
  ActiveAuditGate::reset();
  ActiveAuditGate::confirm();
  EXPECT_TRUE(ActiveAuditGate::confirmed());
}

TEST(ActiveAuditGate, ResetReturnsToFalse) {
  ActiveAuditGate::confirm();
  EXPECT_TRUE(ActiveAuditGate::confirmed());
  ActiveAuditGate::reset();
  EXPECT_FALSE(ActiveAuditGate::confirmed());
}

TEST(ActiveAuditGate, ConfirmIsIdempotent) {
  ActiveAuditGate::reset();
  ActiveAuditGate::confirm();
  ActiveAuditGate::confirm();
  EXPECT_TRUE(ActiveAuditGate::confirmed());
}
