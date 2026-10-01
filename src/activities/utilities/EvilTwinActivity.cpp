#include "EvilTwinActivity.h"

#include <Arduino.h>
#include <DNSServer.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <WebServer.h>
#include <WiFi.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "activities/ActivityResult.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "wifiaudit/ActiveAuditGate.h"
#include "wifiaudit/AttackTx.h"

namespace {
constexpr uint8_t AP_CHANNEL = 6;
constexpr uint8_t AP_MAX_CONNECTIONS = 4;
constexpr uint16_t DNS_PORT = 53;
constexpr size_t SSID_ENTRY_MAX_LEN = 32;

// A plain, clearly-labelled test notice -- deliberately not a login form or
// anything that asks for input. See the header comment for why.
constexpr const char* LANDING_PAGE =
    "<!doctype html><html><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>Test Network</title></head>"
    "<body style=\"font-family:sans-serif;max-width:480px;margin:40px auto;padding:0 16px\">"
    "<h1>This is a test access point</h1>"
    "<p>You're connected to an evil-twin access point created by CrossLight for a personal "
    "Wi-Fi security test. No information was collected and none was requested.</p>"
    "</body></html>";

// Repaint cadence while the portal is running, so the client count and
// elapsed time tick over without a full repaint on every loop() pass.
constexpr uint32_t HEARTBEAT_MS = 1000;
}  // namespace

EvilTwinActivity::EvilTwinActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("EvilTwin", renderer, mappedInput) {}

void EvilTwinActivity::onEnter() {
  Activity::onEnter();
  state = wifiaudit::buildSupportsActiveAudit() ? State::Idle : State::Disabled;
  requestUpdate();
}

void EvilTwinActivity::onExit() {
  stopPortal();
  Activity::onExit();
}

Rect EvilTwinActivity::ssidRowRect() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int top = metrics.topPadding + metrics.headerHeight + renderer.getLineHeight(UI_10_FONT_ID) * 2;
  const int rowH =
      renderer.getLineHeight(UI_10_FONT_ID) + renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing * 2;
  return Rect{0, top, renderer.getScreenWidth(), rowH};
}

void EvilTwinActivity::startPortal() {
  if (ssid.empty()) return;

  WiFi.mode(WIFI_AP);
  delay(100);
  const bool apStarted = WiFi.softAP(ssid.c_str(), nullptr, AP_CHANNEL, false, AP_MAX_CONNECTIONS);
  if (!apStarted) {
    WiFi.mode(WIFI_OFF);
    lastStartFailed = true;
    requestUpdate();
    return;
  }
  delay(100);

  const IPAddress apIP = WiFi.softAPIP();
  dnsServer = new DNSServer();
  dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer->start(DNS_PORT, "*", apIP);

  webServer = new WebServer(80);
  webServer->onNotFound([this]() { webServer->send(200, "text/html", LANDING_PAGE); });
  webServer->begin();

  wifiaudit::ActiveAuditGate::confirm();  // per-boot, auto -- see header comment
  lastStartFailed = false;
  startedMs = lastPaintMs = millis();
  state = State::Running;
}

void EvilTwinActivity::stopPortal() {
  if (webServer) {
    webServer->stop();
    delete webServer;
    webServer = nullptr;
  }
  if (dnsServer) {
    dnsServer->stop();
    delete dnsServer;
    dnsServer = nullptr;
  }
  if (state == State::Running) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    state = State::Idle;
  }
}

void EvilTwinActivity::openSsidEntry() {
  startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_EVIL_TWIN_SSID_LABEL),
                                                                 ssid, SSID_ENTRY_MAX_LEN, InputType::Text),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) return;
                           const auto& kb = std::get<KeyboardResult>(result.data);
                           RenderLock lock(*this);
                           ssid = kb.text;
                           requestUpdate();
                         });
}

void EvilTwinActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (state == State::Disabled) return;

  if (state == State::Running) {
    dnsServer->processNextRequest();
    webServer->handleClient();
    const uint32_t now = millis();
    if (now - lastPaintMs >= HEARTBEAT_MS) {
      lastPaintMs = now;
      RenderLock lock(*this);
      requestUpdate();
    }
  }

  int x = 0;
  int y = 0;
  if (!mappedInput.wasScreenTapped(x, y)) return;

  const Rect row = ssidRowRect();
  if (y >= row.y && y < row.y + row.height) {
    if (state != State::Running) openSsidEntry();
    return;
  }

  RenderLock lock(*this);
  if (state == State::Idle) {
    startPortal();
  } else if (state == State::Running) {
    stopPortal();
  }
  requestUpdate();
}

void EvilTwinActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pad = metrics.contentSidePadding;
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_EVIL_TWIN));

  int y = metrics.topPadding + metrics.headerHeight;
  renderer.drawText(UI_10_FONT_ID, pad, y, tr(STR_EVIL_TWIN_WARN));
  y += lineH * 2;

  if (state == State::Disabled) {
    renderer.drawCenteredText(UI_10_FONT_ID, y + lineH * 2, tr(STR_EVIL_TWIN_DISABLED));
    renderer.displayBuffer();
    return;
  }

  // SSID row.
  const Rect row = ssidRowRect();
  renderer.drawLine(0, row.y, pageWidth, row.y);
  renderer.drawText(UI_10_FONT_ID, pad, row.y + metrics.verticalSpacing, tr(STR_EVIL_TWIN_SSID_LABEL));
  const char* ssidShown = ssid.empty() ? tr(STR_EVIL_TWIN_SSID_EMPTY) : ssid.c_str();
  renderer.drawText(UI_12_FONT_ID, pad, row.y + metrics.verticalSpacing + lineH, ssidShown, true, EpdFontFamily::BOLD);
  y = row.y + row.height;
  renderer.drawLine(0, y, pageWidth, y);
  y += metrics.verticalSpacing * 2;

  if (state == State::Running) {
    char status[64];
    const uint32_t elapsed = (millis() - startedMs) / 1000;
    snprintf(status, sizeof(status), "%u client(s)   %us", static_cast<unsigned>(WiFi.softAPgetStationNum()),
             static_cast<unsigned>(elapsed));
    renderer.drawCenteredText(UI_10_FONT_ID, y, status);
    y += lineH + metrics.verticalSpacing * 2;
    renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_EVIL_TWIN_STOP));
  } else {
    if (lastStartFailed) {
      renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_EVIL_TWIN_FAILED));
      y += lineH + metrics.verticalSpacing;
    }
    renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_EVIL_TWIN_IDLE));
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
