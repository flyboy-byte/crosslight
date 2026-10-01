#pragma once

#include <cstddef>
#include <cstdint>

// The one and only place CrossLight actually puts a frame on the air. Everything
// else in wifiaudit is passive (receive-only) or a pure byte builder; this is the
// active edge. It is double-gated, and has been since its first commit:
//
//   * ARDUINO_ARCH_ESP32        -- there is no radio on the host/simulator build.
//   * CROSSLIGHT_ENABLE_ACTIVE_AUDIT -- a compile flag set ONLY in the local/dev
//                                  x4pro env, never in x4pro-gh_release(_rc), so
//                                  public release binaries physically cannot
//                                  transmit (the call compiles to a no-op).
//
// At runtime it additionally refuses unless ActiveAuditGate::confirmed() -- the
// operator must have cleared the one-time warning this boot.
namespace wifiaudit {

// Transmit a raw 802.11 frame (as built by FrameBuilder) via the STA interface.
// Returns true only if the frame was accepted by the driver. On any build missing
// either compile guard, or before the audit gate is cleared, it does nothing and
// returns false.
bool transmitFrame(const uint8_t* buf, size_t len);

}  // namespace wifiaudit
