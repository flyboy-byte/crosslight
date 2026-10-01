#include "BleSpoofActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "wifiaudit/ActiveAuditGate.h"
#include "wifiaudit/AttackTx.h"

namespace {
// Repaint cadence while advertising, so elapsed time ticks over.
constexpr uint32_t HEARTBEAT_MS = 1000;
}  // namespace

BleSpoofActivity::BleSpoofActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("BleSpoof", renderer, mappedInput) {}

void BleSpoofActivity::onEnter() {
  Activity::onEnter();
  if (!spoofer.begin()) {
    state = State::NoRadio;
  } else {
    state = wifiaudit::buildSupportsActiveAudit() ? State::Idle : State::Disabled;
  }
  requestUpdate();
}

void BleSpoofActivity::onExit() {
  spoofer.end();
  Activity::onExit();
}

Rect BleSpoofActivity::profileRowRect() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int top = metrics.topPadding + metrics.headerHeight + renderer.getLineHeight(UI_10_FONT_ID) * 2;
  const int rowH = renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing;
  return Rect{0, top, renderer.getScreenWidth(), rowH};
}

void BleSpoofActivity::cycleProfile(const int delta) {
  const int n = bleaudit::BleSpoofer::profileCount();
  profile = ((profile + delta) % n + n) % n;
}

void BleSpoofActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (state == State::NoRadio || state == State::Disabled) return;

  int x = 0;
  int y = 0;
  if (mappedInput.wasScreenTapped(x, y)) {
    const Rect row = profileRowRect();
    if (y >= row.y && y < row.y + row.height) {
      // Changing the profile while running restarts the advertiser on it.
      RenderLock lock(*this);
      cycleProfile(x < renderer.getScreenWidth() / 3 ? -1 : 1);
      if (state == State::Running) spoofer.advertise(profile);
      requestUpdate();
      return;
    }
    RenderLock lock(*this);
    if (state == State::Idle) {
      wifiaudit::ActiveAuditGate::confirm();  // per-boot, auto -- see header comment
      if (spoofer.advertise(profile)) {
        startedMs = lastPaintMs = millis();
        state = State::Running;
      }
    } else if (state == State::Running) {
      spoofer.stop();
      state = State::Idle;
    }
    requestUpdate();
    return;
  }

  if (state != State::Running) return;
  const uint32_t now = millis();
  if (now - lastPaintMs >= HEARTBEAT_MS) {
    lastPaintMs = now;
    RenderLock lock(*this);
    requestUpdate();
  }
}

void BleSpoofActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pad = metrics.contentSidePadding;
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BLE_SPOOF));

  int y = metrics.topPadding + metrics.headerHeight;
  renderer.drawText(UI_10_FONT_ID, pad, y, tr(STR_BLE_SPOOF_WARN));
  y += lineH * 2;

  if (state == State::NoRadio) {
    renderer.drawCenteredText(UI_10_FONT_ID, y + lineH * 2, tr(STR_BLE_SPOOF_NO_RADIO));
    renderer.displayBuffer();
    return;
  }
  if (state == State::Disabled) {
    renderer.drawCenteredText(UI_10_FONT_ID, y + lineH * 2, tr(STR_BLE_SPOOF_DISABLED));
    renderer.displayBuffer();
    return;
  }

  // Profile selector row.
  const Rect row = profileRowRect();
  renderer.drawLine(0, row.y, pageWidth, row.y);
  char label[48];
  snprintf(label, sizeof(label), "<  %s  >", bleaudit::BleSpoofer::profileName(profile));
  const int lw = renderer.getTextWidth(UI_12_FONT_ID, label);
  renderer.drawText(UI_12_FONT_ID, (pageWidth - lw) / 2,
                    row.y + (row.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2, label);
  y = row.y + row.height;
  renderer.drawLine(0, y, pageWidth, y);
  y += metrics.verticalSpacing * 2;

  if (state == State::Running) {
    char status[48];
    const uint32_t elapsed = (millis() - startedMs) / 1000;
    snprintf(status, sizeof(status), "advertising   %us", static_cast<unsigned>(elapsed));
    renderer.drawCenteredText(UI_10_FONT_ID, y, status);
    y += lineH + metrics.verticalSpacing * 2;
    renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_BLE_SPOOF_STOP));
  } else {
    renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_BLE_SPOOF_IDLE));
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
