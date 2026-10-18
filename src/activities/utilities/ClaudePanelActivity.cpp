#include "ClaudePanelActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "TrustedTime.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr uint32_t kAutoIntervalMs = 30000;
constexpr int kBarHeight = 28;

// Days since 1970-01-01 for a civil date (Howard Hinnant's algorithm), same
// approach ClaudeClient.cpp uses for its Date header -- avoids timegm/strptime
// portability questions.
long long daysFromCivil(long long y, const unsigned m, const unsigned d) {
  y -= m <= 2;
  const long long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<long long>(doe) - 719468;
}

// "2026-10-18T03:50:00.000Z" -> unix seconds, 0 on any mismatch (including
// empty, which the bridge sends for "unknown").
long long parseIso8601Utc(const std::string& s) {
  int year = 0, mon = 0, day = 0, hh = 0, mm = 0, ss = 0;
  if (sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d", &year, &mon, &day, &hh, &mm, &ss) != 6) return 0;
  return daysFromCivil(year, static_cast<unsigned>(mon), static_cast<unsigned>(day)) * 86400LL + hh * 3600 + mm * 60 +
         ss;
}
}  // namespace

ClaudePanelActivity::ClaudePanelActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("ClaudePanel", renderer, mappedInput) {}

void ClaudePanelActivity::onEnter() {
  Activity::onEnter();
  statusLine = tr(STR_CLAUDE_CHECKING);
  requestUpdate();
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    onWifiSelectionComplete(true);
    return;
  }
  WiFi.mode(WIFI_STA);
  startActivityForResult(
      std::make_unique<WifiSelectionActivity>(renderer, mappedInput, /*autoConnect=*/true, /*quiet=*/true),
      [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
}

void ClaudePanelActivity::onExit() {
  Activity::onExit();
  // Same radio teardown as BibleDownloadActivity: TLS leaves the heap too
  // fragmented to keep running, so a silent restart reclaims it.
  if (WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

void ClaudePanelActivity::onWifiSelectionComplete(const bool ok) {
  connected = ok;
  if (!ok) {
    RenderLock lock(*this);
    statusLine = tr(STR_WIFI_CONN_FAILED);
    requestUpdate();
    return;
  }
  refresh();
}

void ClaudePanelActivity::refresh() {
  if (WiFi.status() != WL_CONNECTED) {
    RenderLock lock(*this);
    statusLine = tr(STR_CLAUDE_NO_WIFI);
    stale = haveUsage;
    lastFetchMs = millis();
    requestUpdate();
    return;
  }
  {
    RenderLock lock(*this);
    statusLine = tr(STR_CLAUDE_CHECKING);
  }
  requestUpdateAndWait();
  claude::BridgeUsage fresh;
  const claude::UsageResult r = claude::fetchUsageViaBridge(fresh);
  RenderLock lock(*this);
  lastFetchMs = millis();
  switch (r.error) {
    case claude::BridgeError::Ok:
      usage = fresh;
      haveUsage = true;
      stale = false;
      statusLine.clear();
      break;
    case claude::BridgeError::NoConfig:
      statusLine = tr(STR_CLAUDE_NO_BRIDGE_CONFIG);
      stale = haveUsage;
      break;
    case claude::BridgeError::Unauthorized:
    case claude::BridgeError::Forbidden:
      statusLine = tr(STR_CLAUDE_TOKEN_REJECTED);
      stale = haveUsage;
      break;
    case claude::BridgeError::LowMemory:
      statusLine = tr(STR_CLAUDE_LOW_MEMORY);
      stale = haveUsage;
      break;
    case claude::BridgeError::RateLimited:
      statusLine = tr(STR_CLAUDE_BRIDGE_RATE_LIMITED);
      stale = haveUsage;
      break;
    case claude::BridgeError::Unavailable:
      statusLine = tr(STR_CLAUDE_BRIDGE_UNAVAILABLE);
      stale = haveUsage;
      break;
    default:
      statusLine = tr(STR_CLAUDE_REQUEST_FAILED);
      stale = haveUsage;
      break;
  }
  requestUpdate();
}

void ClaudePanelActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (!connected) return;

  bool doRefresh = mappedInput.wasReleased(MappedInputManager::Button::Confirm);
  bool toggle = mappedInput.wasReleased(MappedInputManager::Button::Left);
  int x = 0;
  int y = 0;
  if (mappedInput.wasScreenTapped(x, y)) {
    // The top band under the header is the mode chip; any other tap refreshes.
    const auto& metrics = UITheme::getInstance().getMetrics();
    if (y < metrics.topPadding + metrics.headerHeight + 48) {
      toggle = true;
    } else {
      doRefresh = true;
    }
  }
  if (toggle) {
    autoRefresh = !autoRefresh;
    lastFetchMs = millis();
    RenderLock lock(*this);
    requestUpdate();
    return;
  }
  if (doRefresh || (autoRefresh && millis() - lastFetchMs >= kAutoIntervalMs)) refresh();
}

std::string ClaudePanelActivity::resetText(const claude::UsageWindow& w) const {
  const long long resetEpoch = parseIso8601Utc(w.resetsAt);
  const int64_t now = trustedtime::trustedNow();
  if (resetEpoch <= 0 || now <= 0) return "";
  long long secs = std::max(0LL, resetEpoch - now);
  const long long days = secs / 86400;
  const long long hours = (secs % 86400) / 3600;
  const long long mins = (secs % 3600) / 60;
  char buf[48];
  if (days > 0) {
    snprintf(buf, sizeof(buf), "%s %lldd %lldh", tr(STR_CLAUDE_RESETS_IN), days, hours);
  } else {
    snprintf(buf, sizeof(buf), "%s %lldh %02lldm", tr(STR_CLAUDE_RESETS_IN), hours, mins);
  }
  return buf;
}

void ClaudePanelActivity::drawWindow(const int y, const char* title, const claude::UsageWindow& w, const int pad,
                                     const int width) const {
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  const int pct = w.hasUsedPct ? std::clamp(w.usedPct, 0, 100) : 0;

  renderer.drawText(UI_10_FONT_ID, pad, y, title, true, EpdFontFamily::BOLD);
  char pctText[8];
  if (w.hasUsedPct) {
    snprintf(pctText, sizeof(pctText), "%d%%", pct);
  } else {
    snprintf(pctText, sizeof(pctText), "--");
  }
  const int pctW = renderer.getTextWidth(UI_12_FONT_ID, pctText);
  renderer.drawText(UI_12_FONT_ID, width - pad - pctW, y - 4, pctText, true, EpdFontFamily::BOLD);

  const int barY = y + lineH + 6;
  const int barW = width - 2 * pad;
  renderer.drawRect(pad, barY, barW, kBarHeight);
  const int fill = (barW - 4) * pct / 100;
  if (w.hasUsedPct && fill > 0) renderer.fillRect(pad + 2, barY + 2, fill, kBarHeight - 4);

  // The bridge reports one `state` for both windows together, not a
  // per-window status -- show it once here when it isn't the normal case.
  std::string detail = resetText(w);
  if (!usage.state.empty() && usage.state != "allowed") {
    detail += (detail.empty() ? "" : "  ·  ") + usage.state;
  }
  if (!detail.empty()) renderer.drawText(UI_10_FONT_ID, pad, barY + kBarHeight + 6, detail.c_str());
}

void ClaudePanelActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int pad = metrics.contentSidePadding;
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_CLAUDE_PANEL));

  int y = metrics.topPadding + metrics.headerHeight + 12;
  // Mode chip (tap or Left to toggle) plus the stale marker.
  const char* modeText =
      I18n::getInstance().get(autoRefresh ? StrId::STR_CLAUDE_AUTO_30S : StrId::STR_CLAUDE_MANUAL);
  std::string chip = std::string(tr(STR_CLAUDE_MODE_HINT)) + ": " + modeText;
  if (stale) chip += std::string("  ·  ") + tr(STR_CLAUDE_STALE);
  renderer.drawText(UI_10_FONT_ID, pad, y, chip.c_str());
  y += lineH + 40;

  if (haveUsage) {
    drawWindow(y, tr(STR_CLAUDE_5H), usage.fiveHour, pad, pageWidth);
    y += lineH + kBarHeight + lineH + 48;
    drawWindow(y, tr(STR_CLAUDE_7D), usage.sevenDay, pad, pageWidth);
    y += lineH + kBarHeight + lineH + 48;
  }
  if (!statusLine.empty()) renderer.drawText(UI_10_FONT_ID, pad, std::min(y, pageHeight - 120), statusLine.c_str());

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CLAUDE_REFRESH), tr(STR_CLAUDE_MODE_HINT), "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
