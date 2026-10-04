#include "WifiCaptureActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr uint32_t CHANNEL_DWELL_MS = 300;
constexpr uint32_t HEARTBEAT_MS = 1000;
constexpr char CAPTURE_DIR[] = "/wifiaudit";

// First free /wifiaudit/capNNN.pcap, so repeated captures don't clobber. Falls
// back to cap000 if everything is somehow taken.
std::string nextCapturePath() {
  char candidate[48];
  for (int i = 0; i < 1000; ++i) {
    snprintf(candidate, sizeof(candidate), "%s/cap%03d.pcap", CAPTURE_DIR, i);
    if (!Storage.exists(candidate)) return std::string(candidate);
  }
  snprintf(candidate, sizeof(candidate), "%s/cap000.pcap", CAPTURE_DIR);
  return std::string(candidate);
}
}  // namespace

WifiCaptureActivity::WifiCaptureActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("WifiCapture", renderer, mappedInput) {}

void WifiCaptureActivity::onEnter() {
  Activity::onEnter();

  path = nextCapturePath();
  if (!scanner.begin(path.c_str())) {
    state = State::Unavailable;
    requestUpdate();
    return;
  }
  state = State::Capturing;
  startedMs = lastHopMs = lastPaintMs = millis();
  requestUpdate();
}

void WifiCaptureActivity::onExit() {
  scanner.end();
  Activity::onExit();
}

void WifiCaptureActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (state != State::Capturing) return;

  const uint32_t now = millis();
  bool wrote = false;
  if (now - lastHopMs >= CHANNEL_DWELL_MS) {
    scanner.drain();
    scanner.hopChannel();
    lastHopMs = now;
  }
  wrote = scanner.drain();

  if (wrote || now - lastPaintMs >= HEARTBEAT_MS) {
    lastPaintMs = now;
    RenderLock lock(*this);
    requestUpdate();
  }
}

void WifiCaptureActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  const int pad = metrics.contentSidePadding;
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_WIFI_CAPTURE));

  if (state == State::Unavailable) {
    UITheme::drawCenteredWrappedText(renderer, Rect{pad, y, pageWidth - pad * 2, lineH * 6}, UI_10_FONT_ID,
                                     tr(STR_WIFI_CAPTURE_UNAVAILABLE), 6);
    renderer.displayBuffer();
    return;
  }

  const uint32_t elapsed = (millis() - startedMs) / 1000;
  renderer.drawText(UI_10_FONT_ID, pad, y, path.c_str(), true, EpdFontFamily::BOLD);
  y += lineH + metrics.verticalSpacing;
  renderer.drawLine(pad, y, pageWidth - pad, y);
  y += metrics.verticalSpacing;

  char line[80];
  snprintf(line, sizeof(line), "Ch %d   seen %u", scanner.currentChannel(),
           static_cast<unsigned>(scanner.framesSeen()));
  renderer.drawText(UI_10_FONT_ID, pad, y, line);
  y += lineH + metrics.verticalSpacing / 2;
  snprintf(line, sizeof(line), "saved %u   %u KB   %us", static_cast<unsigned>(scanner.framesWritten()),
           static_cast<unsigned>(scanner.bytesWritten() / 1024), static_cast<unsigned>(elapsed));
  renderer.drawText(UI_10_FONT_ID, pad, y, line);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
