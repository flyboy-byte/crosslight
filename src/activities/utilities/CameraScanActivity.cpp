#include "CameraScanActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <vector>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "flock/FlockSignatures.h"

namespace {
// Dwell per channel. Beacons come ~every 100ms, so ~300ms gives a couple of
// chances to hear each AP before moving on -- a full 1-13 sweep every ~4s.
constexpr uint32_t CHANNEL_DWELL_MS = 300;
// Repaint cadence when nothing changed, so the elapsed time and channel tick
// over without thrashing the panel.
constexpr uint32_t HEARTBEAT_MS = 1000;
}  // namespace

CameraScanActivity::CameraScanActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("CameraScan", renderer, mappedInput) {}

void CameraScanActivity::onEnter() {
  Activity::onEnter();

  std::vector<flock::Signature> sigs;
  if (!flock::loadSignatures(flock::SIGNATURE_PATH, sigs)) {
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
  startedMs = lastHopMs = lastPaintMs = millis();
  requestUpdate();
}

void CameraScanActivity::onExit() {
  // Receive-only, so there is no association to drop; just leave promiscuous
  // mode and release the radio. Whether normal Wi-Fi works again without a
  // reboot is the key on-device check for this feature.
  scanner.end();
  Activity::onExit();
}

void CameraScanActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (state != State::Scanning) return;

  const uint32_t now = millis();
  bool changed = false;
  if (now - lastHopMs >= CHANNEL_DWELL_MS) {
    scanner.drain();  // sweep up this channel's frames before leaving it
    scanner.hopChannel();
    lastHopMs = now;
  }
  changed = scanner.drain();

  if (changed || now - lastPaintMs >= HEARTBEAT_MS) {
    lastPaintMs = now;
    RenderLock lock(*this);
    requestUpdate();
  }
}

void CameraScanActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  const int pad = metrics.contentSidePadding;
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_CAMERA_SCAN));

  if (state == State::NoSignatures) {
    UITheme::drawCenteredWrappedText(renderer, Rect{pad, y, pageWidth - pad * 2, lineH * 6}, UI_10_FONT_ID,
                                     tr(STR_CAMERA_SCAN_NO_SIGNATURES), 6);
    renderer.displayBuffer();
    return;
  }
  if (state == State::NoRadio) {
    renderer.drawCenteredText(UI_10_FONT_ID, y + lineH * 2, tr(STR_CAMERA_SCAN_NO_RADIO));
    renderer.displayBuffer();
    return;
  }

  // Status line: channel, frames seen, elapsed seconds.
  const uint32_t elapsed = (millis() - startedMs) / 1000;
  char status[64];
  snprintf(status, sizeof(status), "Ch %d   %u frames   %us", scanner.currentChannel(),
           static_cast<unsigned>(scanner.framesSeen()), static_cast<unsigned>(elapsed));
  renderer.drawText(UI_10_FONT_ID, pad, y, status);
  y += lineH + metrics.verticalSpacing;
  renderer.drawLine(pad, y, pageWidth - pad, y);
  y += metrics.verticalSpacing;

  const auto& hits = scanner.detections();
  if (hits.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, y + lineH * 2, tr(STR_CAMERA_SCAN_SCANNING));
  } else {
    for (const auto& d : hits) {
      if (y > renderer.getScreenHeight() - lineH * 3) break;
      char line[96];
      snprintf(line, sizeof(line), "%s  %02X:%02X:%02X:%02X:%02X:%02X", d.name.c_str(), d.mac[0], d.mac[1], d.mac[2],
               d.mac[3], d.mac[4], d.mac[5]);
      renderer.drawText(UI_10_FONT_ID, pad, y, line, true, EpdFontFamily::BOLD);
      y += lineH;
      char detail[80];
      snprintf(detail, sizeof(detail), "  %s  %ddBm  x%u", d.ssid.empty() ? "(hidden)" : d.ssid.c_str(), d.rssi,
               static_cast<unsigned>(d.count));
      renderer.drawText(UI_10_FONT_ID, pad, y, detail);
      y += lineH + metrics.verticalSpacing / 2;
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
