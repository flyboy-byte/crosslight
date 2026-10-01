#include "PmkidHarvestActivity.h"

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

std::string nextOutputPath() {
  char candidate[48];
  for (int i = 0; i < 1000; ++i) {
    snprintf(candidate, sizeof(candidate), "%s/pmkid%03d.22000", CAPTURE_DIR, i);
    if (!Storage.exists(candidate)) return std::string(candidate);
  }
  snprintf(candidate, sizeof(candidate), "%s/pmkid000.22000", CAPTURE_DIR);
  return std::string(candidate);
}
}  // namespace

PmkidHarvestActivity::PmkidHarvestActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("PmkidHarvest", renderer, mappedInput) {}

void PmkidHarvestActivity::onEnter() {
  Activity::onEnter();

  path = nextOutputPath();
  if (!scanner.begin(path.c_str())) {
    state = State::Unavailable;
    requestUpdate();
    return;
  }
  state = State::Harvesting;
  startedMs = lastHopMs = lastPaintMs = millis();
  requestUpdate();
}

void PmkidHarvestActivity::onExit() {
  scanner.end();
  Activity::onExit();
}

void PmkidHarvestActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (state != State::Harvesting) return;

  const uint32_t now = millis();
  bool caught = false;
  if (now - lastHopMs >= CHANNEL_DWELL_MS) {
    scanner.drain();
    scanner.hopChannel();
    lastHopMs = now;
  }
  caught = scanner.drain();

  if (caught || now - lastPaintMs >= HEARTBEAT_MS) {
    lastPaintMs = now;
    RenderLock lock(*this);
    requestUpdate();
  }
}

void PmkidHarvestActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  const int pad = metrics.contentSidePadding;
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_PMKID_HARVEST));

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

  char big[32];
  snprintf(big, sizeof(big), "%u", static_cast<unsigned>(scanner.pmkidCount()));
  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, y + lineH, big);
  y += lineH * 3;
  renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_PMKID_HARVEST_CAUGHT));
  y += lineH + metrics.verticalSpacing;

  char line[72];
  snprintf(line, sizeof(line), "Ch %d   seen %u   %us", scanner.currentChannel(),
           static_cast<unsigned>(scanner.framesSeen()), static_cast<unsigned>(elapsed));
  renderer.drawCenteredText(UI_10_FONT_ID, y, line);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
