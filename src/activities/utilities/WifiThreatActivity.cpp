#include "WifiThreatActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "wifiaudit/ThreatDetect.h"

namespace {
constexpr uint32_t CHANNEL_DWELL_MS = 300;
constexpr uint32_t HEARTBEAT_MS = 1000;
// A deauth flood is a burst, not the odd legitimate frame: flag when this many
// deauth/disassoc frames land within the window. Tuned conservatively; a real
// attack sprays hundreds per second.
constexpr uint32_t DEAUTH_WINDOW_MS = 10000;
constexpr uint32_t DEAUTH_FLOOD_THRESHOLD = 30;
}  // namespace

WifiThreatActivity::WifiThreatActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("WifiThreat", renderer, mappedInput) {}

void WifiThreatActivity::onEnter() {
  Activity::onEnter();

  if (!scanner.begin()) {
    state = State::NoRadio;
    requestUpdate();
    return;
  }
  state = State::Scanning;
  startedMs = lastHopMs = lastPaintMs = millis();
  requestUpdate();
}

void WifiThreatActivity::onExit() {
  scanner.end();
  Activity::onExit();
}

void WifiThreatActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (state != State::Scanning) return;

  const uint32_t now = millis();
  bool changed = false;
  if (now - lastHopMs >= CHANNEL_DWELL_MS) {
    scanner.drain();
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

void WifiThreatActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  const int pad = metrics.contentSidePadding;
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_WIFI_THREAT));

  if (state == State::NoRadio) {
    renderer.drawCenteredText(UI_10_FONT_ID, y + lineH * 2, tr(STR_WIFI_SCAN_NO_RADIO));
    renderer.displayBuffer();
    return;
  }

  const uint32_t now = millis();
  const uint32_t elapsed = (now - startedMs) / 1000;
  char status[72];
  snprintf(status, sizeof(status), "Ch %d   %u APs   %u deauth   %us", scanner.currentChannel(),
           static_cast<unsigned>(scanner.accessPoints().size()), static_cast<unsigned>(scanner.deauthsSeen()),
           static_cast<unsigned>(elapsed));
  renderer.drawText(UI_10_FONT_ID, pad, y, status);
  y += lineH + metrics.verticalSpacing;
  renderer.drawLine(pad, y, pageWidth - pad, y);
  y += metrics.verticalSpacing;

  bool anyThreat = false;

  if (wifiaudit::deauthFloodActive(scanner.deauthEventsMs(), now, DEAUTH_WINDOW_MS, DEAUTH_FLOOD_THRESHOLD)) {
    anyThreat = true;
    const uint32_t n = wifiaudit::deauthCountInWindow(scanner.deauthEventsMs(), now, DEAUTH_WINDOW_MS);
    char line[80];
    snprintf(line, sizeof(line), "%s (%u/10s)", tr(STR_WIFI_THREAT_DEAUTH_FLOOD), static_cast<unsigned>(n));
    renderer.drawText(UI_10_FONT_ID, pad, y, line, true, EpdFontFamily::BOLD);
    y += lineH + metrics.verticalSpacing / 2;
  }

  const auto twins = wifiaudit::findEvilTwins(scanner.accessPoints());
  for (const auto& t : twins) {
    if (y > renderer.getScreenHeight() - lineH * 3) break;
    anyThreat = true;
    renderer.drawText(UI_10_FONT_ID, pad, y, t.ssid.empty() ? "(hidden)" : t.ssid.c_str(), true, EpdFontFamily::BOLD);
    y += lineH;
    char detail[80];
    snprintf(detail, sizeof(detail), "  %s: %u BSSIDs%s", tr(STR_WIFI_THREAT_EVIL_TWIN),
             static_cast<unsigned>(t.bssidCount), t.encryptionMismatch ? tr(STR_WIFI_THREAT_ENC_MISMATCH) : "");
    renderer.drawText(UI_10_FONT_ID, pad, y, detail);
    y += lineH + metrics.verticalSpacing / 2;
  }

  if (!anyThreat) {
    renderer.drawCenteredText(UI_10_FONT_ID, y + lineH * 2, tr(STR_WIFI_THREAT_NONE));
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
