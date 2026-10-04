#include "BeaconFloodActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "offensive/ActiveAuditGate.h"
#include "offensive/AttackTx.h"
#include "offensive/FrameBuilder.h"
#include "offensive/RandomMac.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_random.h>
#endif

namespace {
// Generic, non-targeted placeholder names -- there is nothing to fabricate or
// get wrong here (unlike BLE/Flock signature data): these are just filler
// network names a flood advertises, not a claim about any real device.
constexpr const char* kFloodSsids[] = {
    "Test-Network-01", "Test-Network-02", "Test-Network-03", "Test-Network-04", "Test-Network-05", "Test-Network-06",
    "Test-Network-07", "Test-Network-08", "Test-Network-09", "Test-Network-10", "Test-Network-11", "Test-Network-12",
};
constexpr int FLOOD_SSID_COUNT = sizeof(kFloodSsids) / sizeof(kFloodSsids[0]);

constexpr uint8_t MIN_CHANNEL = 1;
constexpr uint8_t MAX_CHANNEL = 13;  // 2.4GHz, world-safe range (14 is Japan-only)

// ~5 fake beacons/sec: enough to clutter a scan list quickly without
// saturating the channel or outrunning what the e-ink screen needs to show.
constexpr uint32_t TX_INTERVAL_MS = 200;
// Repaint cadence while running, so the frame counter visibly ticks over
// without a full repaint on every single transmitted frame.
constexpr uint32_t HEARTBEAT_MS = 500;
}  // namespace

BeaconFloodActivity::BeaconFloodActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("BeaconFlood", renderer, mappedInput) {}

void BeaconFloodActivity::onEnter() {
  Activity::onEnter();
  if (!txRadio.begin()) {
    state = State::NoRadio;
  } else {
    txRadio.setChannel(MIN_CHANNEL);
    state = wifiaudit::buildSupportsActiveAudit() ? State::Idle : State::Disabled;
  }
  requestUpdate();
}

void BeaconFloodActivity::onExit() {
  txRadio.end();
  Activity::onExit();
}

Rect BeaconFloodActivity::channelRowRect() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int top = metrics.topPadding + metrics.headerHeight + renderer.getLineHeight(UI_10_FONT_ID) * 2;
  const int rowH = renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing;
  return Rect{0, top, renderer.getScreenWidth(), rowH};
}

void BeaconFloodActivity::cycleChannel(const int delta) {
  const int next = static_cast<int>(txRadio.currentChannel()) + delta;
  const int wrapped = next < MIN_CHANNEL ? MAX_CHANNEL : (next > MAX_CHANNEL ? MIN_CHANNEL : next);
  txRadio.setChannel(static_cast<uint8_t>(wrapped));
}

void BeaconFloodActivity::sendOneFrame() {
  uint8_t bssid[6];
#if defined(ARDUINO_ARCH_ESP32)
  const uint32_t a = esp_random();
  const uint32_t b = esp_random();
  bssid[0] = static_cast<uint8_t>(a);
  bssid[1] = static_cast<uint8_t>(a >> 8);
  bssid[2] = static_cast<uint8_t>(a >> 16);
  bssid[3] = static_cast<uint8_t>(a >> 24);
  bssid[4] = static_cast<uint8_t>(b);
  bssid[5] = static_cast<uint8_t>(b >> 8);
#else
  for (uint8_t& b : bssid) b = 0;
#endif
  wifiaudit::makeLocallyAdministered(bssid);

  const char* ssid = kFloodSsids[ssidIndex];
  uint8_t frame[64];
  const size_t len = wifiaudit::buildBeacon(frame, sizeof(frame), bssid, ssid, strlen(ssid), txRadio.currentChannel());
  if (len > 0) wifiaudit::transmitFrame(frame, len);

  ssidIndex = (ssidIndex + 1) % FLOOD_SSID_COUNT;
  framesSent++;
}

void BeaconFloodActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (state == State::NoRadio || state == State::Disabled) return;

  int x = 0;
  int y = 0;
  if (mappedInput.wasScreenTapped(x, y)) {
    const Rect ch = channelRowRect();
    if (y >= ch.y && y < ch.y + ch.height) {
      RenderLock lock(*this);
      cycleChannel(x < renderer.getScreenWidth() / 3 ? -1 : 1);
      requestUpdate();
    } else if (state == State::Idle || state == State::Running) {
      RenderLock lock(*this);
      if (state == State::Idle) {
        wifiaudit::ActiveAuditGate::confirm();  // per-boot, auto -- see header comment
        framesSent = 0;
        ssidIndex = 0;
        startedMs = lastTxMs = lastPaintMs = millis();
        state = State::Running;
      } else {
        state = State::Idle;
      }
      requestUpdate();
    }
    return;
  }

  if (state != State::Running) return;
  const uint32_t now = millis();
  if (now - lastTxMs >= TX_INTERVAL_MS) {
    lastTxMs = now;
    sendOneFrame();
  }
  if (now - lastPaintMs >= HEARTBEAT_MS) {
    lastPaintMs = now;
    RenderLock lock(*this);
    requestUpdate();
  }
}

void BeaconFloodActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pad = metrics.contentSidePadding;
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BEACON_FLOOD));

  int y = metrics.topPadding + metrics.headerHeight;
  renderer.drawText(UI_10_FONT_ID, pad, y, tr(STR_BEACON_FLOOD_WARN));
  y += lineH * 2;

  if (state == State::NoRadio) {
    renderer.drawCenteredText(UI_10_FONT_ID, y + lineH * 2, tr(STR_BEACON_FLOOD_NO_RADIO));
    renderer.displayBuffer();
    return;
  }
  if (state == State::Disabled) {
    renderer.drawCenteredText(UI_10_FONT_ID, y + lineH * 2, tr(STR_BEACON_FLOOD_DISABLED));
    renderer.displayBuffer();
    return;
  }

  // Channel selector row.
  const Rect ch = channelRowRect();
  renderer.drawLine(0, ch.y, pageWidth, ch.y);
  char chLabel[32];
  snprintf(chLabel, sizeof(chLabel), "<  %s %u  >", tr(STR_BEACON_FLOOD_CHANNEL),
           static_cast<unsigned>(txRadio.currentChannel()));
  const int chW = renderer.getTextWidth(UI_12_FONT_ID, chLabel);
  renderer.drawText(UI_12_FONT_ID, (pageWidth - chW) / 2,
                    ch.y + (ch.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2, chLabel);
  y = ch.y + ch.height;
  renderer.drawLine(0, y, pageWidth, y);
  y += metrics.verticalSpacing * 2;

  // Status + the big tap-to-toggle label.
  if (state == State::Running) {
    const uint32_t elapsed = (millis() - startedMs) / 1000;
    char status[64];
    snprintf(status, sizeof(status), "%u frames   %us", static_cast<unsigned>(framesSent),
             static_cast<unsigned>(elapsed));
    renderer.drawCenteredText(UI_10_FONT_ID, y, status);
    y += lineH + metrics.verticalSpacing;
    renderer.drawCenteredText(UI_10_FONT_ID, y, kFloodSsids[ssidIndex]);
    y += lineH + metrics.verticalSpacing * 2;
    renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_BEACON_FLOOD_STOP));
  } else {
    renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_BEACON_FLOOD_IDLE));
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
