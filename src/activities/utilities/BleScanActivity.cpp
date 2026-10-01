#include "BleScanActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <vector>

#include "MappedInputManager.h"
#include "bleaudit/BleSignatures.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Repaint cadence when nothing changed, so the elapsed time and advert counter
// tick over without thrashing the panel.
constexpr uint32_t HEARTBEAT_MS = 1000;
}  // namespace

BleScanActivity::BleScanActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("BleScan", renderer, mappedInput) {}

void BleScanActivity::onEnter() {
  Activity::onEnter();

  std::vector<bleaudit::BleSignature> sigs;
  if (!bleaudit::loadSignatures(bleaudit::SIGNATURE_PATH, sigs)) {
    state = State::NoSignatures;
    requestUpdate();
    return;
  }
  if (!scanner.begin(std::move(sigs))) {
    state = State::NoRadio;
    requestUpdate();
    return;
  }
  state = State::Scanning;
  startedMs = lastPaintMs = millis();
  requestUpdate();
}

void BleScanActivity::onExit() {
  // Receive-only, so there is no connection to drop; just stop the passive scan
  // and release the radio. Whether normal BLE/Wi-Fi works again without a reboot
  // is the key on-device check for this feature.
  scanner.end();
  Activity::onExit();
}

void BleScanActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (state != State::Scanning) return;

  const uint32_t now = millis();
  const bool changed = scanner.drain();

  if (changed || now - lastPaintMs >= HEARTBEAT_MS) {
    lastPaintMs = now;
    RenderLock lock(*this);
    requestUpdate();
  }
}

void BleScanActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  const int pad = metrics.contentSidePadding;
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BLE_SCAN));

  if (state == State::NoSignatures) {
    UITheme::drawCenteredWrappedText(renderer, Rect{pad, y, pageWidth - pad * 2, lineH * 6}, UI_10_FONT_ID,
                                     tr(STR_BLE_SCAN_NO_SIGNATURES), 6);
    renderer.displayBuffer();
    return;
  }
  if (state == State::NoRadio) {
    renderer.drawCenteredText(UI_10_FONT_ID, y + lineH * 2, tr(STR_BLE_SCAN_NO_RADIO));
    renderer.displayBuffer();
    return;
  }

  // Status line: adverts seen, elapsed seconds.
  const uint32_t elapsed = (millis() - startedMs) / 1000;
  char status[64];
  snprintf(status, sizeof(status), "%u adverts   %us", static_cast<unsigned>(scanner.advertsSeen()),
           static_cast<unsigned>(elapsed));
  renderer.drawText(UI_10_FONT_ID, pad, y, status);
  y += lineH + metrics.verticalSpacing;
  renderer.drawLine(pad, y, pageWidth - pad, y);
  y += metrics.verticalSpacing;

  const auto& hits = scanner.detections();
  if (hits.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, y + lineH * 2, tr(STR_BLE_SCAN_SCANNING));
  } else {
    for (const auto& d : hits) {
      if (y > renderer.getScreenHeight() - lineH * 3) break;
      char line[96];
      snprintf(line, sizeof(line), "%s  %02X:%02X:%02X:%02X:%02X:%02X", d.name.c_str(), d.address[0], d.address[1],
               d.address[2], d.address[3], d.address[4], d.address[5]);
      renderer.drawText(UI_10_FONT_ID, pad, y, line, true, EpdFontFamily::BOLD);
      y += lineH;
      char detail[96];
      snprintf(detail, sizeof(detail), "  %s  %ddBm  x%u", d.serviceInfo.empty() ? "(no info)" : d.serviceInfo.c_str(),
               d.rssi, static_cast<unsigned>(d.count));
      renderer.drawText(UI_10_FONT_ID, pad, y, detail);
      y += lineH + metrics.verticalSpacing / 2;
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
