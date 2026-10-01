#include "WifiScanActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <vector>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Dwell per channel. Beacons come ~every 100ms, so ~300ms gives a couple of
// chances to hear each AP before moving on -- a full 1-13 sweep every ~4s.
constexpr uint32_t CHANNEL_DWELL_MS = 300;
// Repaint cadence when nothing changed, so elapsed time and channel tick over
// without thrashing the panel.
constexpr uint32_t HEARTBEAT_MS = 1000;
}  // namespace

WifiScanActivity::WifiScanActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("WifiScan", renderer, mappedInput) {}

void WifiScanActivity::onEnter() {
  Activity::onEnter();

  ouiDb.open("/vendordb/oui.bin");  // optional: vendor labels when the db is on the card

  if (!scanner.begin()) {
    state = State::NoRadio;
    requestUpdate();
    return;
  }
  state = State::Scanning;
  startedMs = lastHopMs = lastPaintMs = millis();
  requestUpdate();
}

void WifiScanActivity::onExit() {
  // Receive-only: no association to drop, just leave promiscuous mode and
  // release the radio.
  scanner.end();
  ouiDb.close();
  Activity::onExit();
}

const std::string& WifiScanActivity::vendorFor(const uint8_t bssid[6]) {
  const uint32_t key = vendordb::ouiKey(bssid);
  auto it = ouiCache.find(key);
  if (it == ouiCache.end()) it = ouiCache.emplace(key, ouiDb.lookup(key)).first;
  return it->second;
}

void WifiScanActivity::loop() {
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

void WifiScanActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  const int pad = metrics.contentSidePadding;
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_WIFI_SCAN));

  if (state == State::NoRadio) {
    renderer.drawCenteredText(UI_10_FONT_ID, y + lineH * 2, tr(STR_WIFI_SCAN_NO_RADIO));
    renderer.displayBuffer();
    return;
  }

  // Status line: channel, APs found, elapsed seconds.
  const uint32_t elapsed = (millis() - startedMs) / 1000;
  char status[64];
  snprintf(status, sizeof(status), "Ch %d   %u APs   %us", scanner.currentChannel(),
           static_cast<unsigned>(scanner.accessPoints().size()), static_cast<unsigned>(elapsed));
  renderer.drawText(UI_10_FONT_ID, pad, y, status);
  y += lineH + metrics.verticalSpacing;
  renderer.drawLine(pad, y, pageWidth - pad, y);
  y += metrics.verticalSpacing;

  const auto& aps = scanner.accessPoints();
  if (aps.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, y + lineH * 2, tr(STR_WIFI_SCAN_SCANNING));
  } else {
    // Strongest signal first, so the nearest networks stay at the top.
    std::vector<const wifiaudit::ApRecord*> sorted;
    sorted.reserve(aps.size());
    for (const auto& r : aps) sorted.push_back(&r);
    std::sort(sorted.begin(), sorted.end(),
              [](const wifiaudit::ApRecord* a, const wifiaudit::ApRecord* b) { return a->rssi > b->rssi; });

    for (const auto* r : sorted) {
      if (y > renderer.getScreenHeight() - lineH * 3) break;
      const auto& ap = r->ap;
      renderer.drawText(UI_10_FONT_ID, pad, y, ap.ssid.empty() ? "(hidden)" : ap.ssid.c_str(), true,
                        EpdFontFamily::BOLD);
      y += lineH;
      const std::string& vendor = vendorFor(ap.bssid);
      if (!vendor.empty()) {
        renderer.drawText(UI_10_FONT_ID, pad, y, ("  " + vendor).c_str());
        y += lineH;
      }
      char detail[96];
      snprintf(detail, sizeof(detail), "  %02X:%02X:%02X:%02X:%02X:%02X  ch%u  %s  %ddBm", ap.bssid[0], ap.bssid[1],
               ap.bssid[2], ap.bssid[3], ap.bssid[4], ap.bssid[5], static_cast<unsigned>(ap.channel),
               wifiaudit::encryptionLabel(ap.encryption), r->rssi);
      renderer.drawText(UI_10_FONT_ID, pad, y, detail);
      y += lineH + metrics.verticalSpacing / 2;
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
