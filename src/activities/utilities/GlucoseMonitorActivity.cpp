#include "GlucoseMonitorActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <TrustedTime.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Readings land about once a minute (agent-api.md "Etiquette and limits");
// polling faster buys nothing, so this is both the auto-refresh period and
// the floor -- there is no separate "fast" mode to pick between.
constexpr uint32_t kAutoIntervalMs = 60000;

// BLE is a push, not a poll (crosslight/PLAN.md power design): a new write
// redraws immediately, but staleness must also catch up on its own when the
// companion stops writing entirely, so this re-derives the displayed status
// from the wall clock at a modest cadence even with no new packet -- the
// 10-minute cutoff doesn't need finer granularity than this.
constexpr uint32_t kBleRecheckMs = 10000;

// How long to hold Confirm, in BLE mode, to clear the bond and re-advertise
// for a fresh pairing (extraction docs/companion-app-onboarding.md section 4
// "Re-pair" flow -- this is the reader-side half of it). Long enough that a
// normal press never triggers it by accident.
constexpr unsigned long kForgetBondHoldMs = 1500;
}  // namespace

GlucoseMonitorActivity::GlucoseMonitorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("GlucoseMonitor", renderer, mappedInput) {}

void GlucoseMonitorActivity::onEnter() {
  Activity::onEnter();
  statusLine = tr(STR_GLUCOSE_CHECKING);
  requestUpdate();
  beginSource();
}

void GlucoseMonitorActivity::onExit() {
  Activity::onExit();
  endSource();
}

void GlucoseMonitorActivity::beginSource() {
  if (source == Source::Wifi) {
    if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
      onWifiSelectionComplete(true);
      return;
    }
    WiFi.mode(WIFI_STA);
    startActivityForResult(
        std::make_unique<WifiSelectionActivity>(renderer, mappedInput, /*autoConnect=*/true, /*quiet=*/true),
        [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
    return;
  }

  // BLE: nothing to await like Wi-Fi association -- advertising starts (or
  // doesn't, if there's no radio) and pollBle() drains whatever the
  // companion writes from here on.
  connected = true;
  haveReading = false;
  stale = false;
  stopped = false;
  lastBlePacket = libre::BlePacket{};
  lastFetchMs = millis();
  statusLine = bleServer.begin() ? tr(STR_GLUCOSE_BLE_WAITING) : tr(STR_GLUCOSE_BLE_NO_RADIO);
  RenderLock lock(*this);
  requestUpdate();
}

void GlucoseMonitorActivity::endSource() {
  if (source == Source::Wifi) {
    // Same radio teardown as ClaudePanelActivity: TLS leaves the heap too
    // fragmented to keep running, so a silent restart reclaims it.
    if (WiFi.getMode() != WIFI_MODE_NULL) {
      WiFi.disconnect(false);
      delay(30);
      silentRestart();
    }
  } else {
    bleServer.end();
  }
}

void GlucoseMonitorActivity::switchSource(const Source next) {
  if (next == source) return;
  endSource();
  source = next;
  haveReading = false;
  stale = false;
  stopped = false;
  {
    RenderLock lock(*this);
    statusLine = tr(STR_GLUCOSE_CHECKING);
    requestUpdate();
  }
  beginSource();
}

void GlucoseMonitorActivity::onWifiSelectionComplete(const bool ok) {
  connected = ok;
  if (!ok) {
    RenderLock lock(*this);
    statusLine = tr(STR_WIFI_CONN_FAILED);
    requestUpdate();
    return;
  }
  refresh();
}

void GlucoseMonitorActivity::refresh() {
  if (WiFi.status() != WL_CONNECTED) {
    RenderLock lock(*this);
    statusLine = tr(STR_GLUCOSE_NO_WIFI);
    stale = haveReading;
    lastFetchMs = millis();
    requestUpdate();
    return;
  }
  {
    RenderLock lock(*this);
    statusLine = tr(STR_GLUCOSE_CHECKING);
  }
  requestUpdateAndWait();
  const libre::FetchResult r = libre::fetchLatest();
  RenderLock lock(*this);
  lastFetchMs = millis();
  switch (r.error) {
    case libre::Error::Ok:
      reading = r.reading;
      haveReading = true;
      stale = false;
      statusLine.clear();
      break;
    case libre::Error::NoConfig:
      statusLine = tr(STR_GLUCOSE_NO_CONFIG);
      stale = haveReading;
      break;
    case libre::Error::Unauthorized:
      statusLine = tr(STR_GLUCOSE_TOKEN_REJECTED);
      stale = haveReading;
      // Never auto-retry a rejected key (agent-api.md: 10 bad-key requests
      // in 10 min locks out the whole IP for 15 min). A manual Confirm press
      // after the owner fixes the key on the SD card is a new, deliberate
      // attempt, not a retry -- that path still calls refresh().
      stopped = true;
      break;
    case libre::Error::LowMemory:
      statusLine = tr(STR_GLUCOSE_LOW_MEMORY);
      stale = haveReading;
      break;
    case libre::Error::NoData:
      statusLine = tr(STR_GLUCOSE_NO_DATA);
      stale = haveReading;
      break;
    default:
      statusLine = tr(STR_GLUCOSE_REQUEST_FAILED);
      stale = haveReading;
      break;
  }
  requestUpdate();
}

void GlucoseMonitorActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  // Switching source must work even if the current one failed to connect
  // (Wi-Fi join failure, or no BLE radio), so this runs before the
  // `!connected` gate below.
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    switchSource(source == Source::Wifi ? Source::Ble : Source::Wifi);
    return;
  }
  if (!connected) return;

  if (source == Source::Ble) {
    if (mappedInput.wasLongPressed(MappedInputManager::Button::Confirm, kForgetBondHoldMs)) {
      forgetBlePeer();
      return;
    }
    pollBle();
    return;
  }

  bool doRefresh = mappedInput.wasReleased(MappedInputManager::Button::Confirm);
  bool toggle = mappedInput.wasReleased(MappedInputManager::Button::Left);
  int x = 0;
  int y = 0;
  if (mappedInput.wasScreenTapped(x, y)) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    if (y < metrics.topPadding + metrics.headerHeight + 48) {
      toggle = true;
    } else {
      doRefresh = true;
    }
  }
  if (doRefresh) stopped = false;  // an explicit tap/Confirm is a deliberate retry, not an automatic one
  if (toggle) {
    autoRefresh = !autoRefresh;
    lastFetchMs = millis();
    RenderLock lock(*this);
    requestUpdate();
    return;
  }
  if (doRefresh || (autoRefresh && !stopped && millis() - lastFetchMs >= kAutoIntervalMs)) refresh();
}

void GlucoseMonitorActivity::pollBle() {
  if (!bleServer.running()) return;  // statusLine already says so from beginSource()

  const bool gotNew = bleServer.takeLatest(lastBlePacket);
  const uint32_t now = millis();
  if (!gotNew) {
    if (!haveReading) return;                           // never received a packet yet
    if (now - lastFetchMs < kBleRecheckMs) return;       // not due for a staleness re-check
  }
  lastFetchMs = now;

  const int64_t trustedNow = trustedtime::trustedNow();
  RenderLock lock(*this);
  reading = libre::bleReadingFromPacket(lastBlePacket, trustedNow);
  haveReading = true;
  statusLine = trustedNow <= 0 ? tr(STR_GLUCOSE_BLE_NO_CLOCK) : "";
  requestUpdate();
}

void GlucoseMonitorActivity::forgetBlePeer() {
  bleServer.forgetBond();
  haveReading = false;
  stale = false;
  lastBlePacket = libre::BlePacket{};
  lastFetchMs = millis();
  RenderLock lock(*this);
  statusLine = tr(STR_GLUCOSE_BLE_FORGOT);
  requestUpdate();
}

const char* GlucoseMonitorActivity::trendArrow() const {
  // Plain ASCII, not Unicode arrows: confirmed on real hardware (2026-10-13)
  // that NOTOSERIF_14 -- the only font guaranteed present once -DOMIT_FONTS
  // strips the rest for the release build -- has no glyphs for U+2191 etc.,
  // so they rendered as missing-glyph boxes. ASCII is in every font's range.
  switch (reading.trend) {
    case libre::Trend::FallingFast:
      return "vv";
    case libre::Trend::Falling:
      return "v";
    case libre::Trend::Steady:
      return "-";
    case libre::Trend::Rising:
      return "^";
    case libre::Trend::RisingFast:
      return "^^";
    default:
      return "";
  }
}

std::string GlucoseMonitorActivity::statusText() const {
  char buf[48];
  switch (reading.status) {
    case libre::ReadingStatus::High:
      return tr(STR_GLUCOSE_STATUS_HIGH);
    case libre::ReadingStatus::Low:
      return tr(STR_GLUCOSE_STATUS_LOW);
    case libre::ReadingStatus::Unavailable:
      return tr(STR_GLUCOSE_STATUS_UNAVAILABLE);
    case libre::ReadingStatus::Stale:
    case libre::ReadingStatus::Ok:
      if (reading.ageS < 60) {
        snprintf(buf, sizeof(buf), tr(STR_GLUCOSE_AGE_SEC_AGO), reading.ageS);
      } else {
        snprintf(buf, sizeof(buf), tr(STR_GLUCOSE_AGE_MIN_AGO), reading.ageS / 60);
      }
      if (reading.status == libre::ReadingStatus::Stale) {
        return std::string(tr(STR_GLUCOSE_STATUS_STALE)) + " \xc2\xb7 " + buf;  // "Stale · %d min ago"
      }
      return buf;
  }
  return "";
}

void GlucoseMonitorActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int pad = metrics.contentSidePadding;
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_GLUCOSE_MONITOR));

  int y = metrics.topPadding + metrics.headerHeight + 12;
  std::string chip;
  if (source == Source::Wifi) {
    const char* modeText =
        I18n::getInstance().get(autoRefresh ? StrId::STR_GLUCOSE_AUTO_60S : StrId::STR_GLUCOSE_MANUAL);
    chip = std::string(tr(STR_GLUCOSE_SOURCE_WIFI)) + " \xc2\xb7 " + modeText;  // "Wi-Fi · Auto 60s"
  } else {
    chip = tr(STR_GLUCOSE_SOURCE_BLE);
  }
  renderer.drawText(UI_10_FONT_ID, pad, y, chip.c_str());
  y += lineH + 40;

  // Values that show HI/LO/no-number states (agent-api.md "Status") never
  // render a number -- only OK/STALE carry one, and STALE must read as old,
  // not current.
  if (haveReading) {
    const bool showNumber =
        reading.status == libre::ReadingStatus::Ok || reading.status == libre::ReadingStatus::Stale;
    if (showNumber) {
      char valText[16];
      snprintf(valText, sizeof(valText), "%d", reading.value);
      renderer.drawText(NOTOSANS_14_FONT_ID, pad, y, valText, true, EpdFontFamily::BOLD);
      const int valW = renderer.getTextWidth(NOTOSANS_14_FONT_ID, valText);
      const int unitX = pad + valW + 12;
      renderer.drawText(UI_12_FONT_ID, unitX, y + 6, reading.unit.c_str());
      const char* arrow = trendArrow();
      if (arrow[0] != '\0') {
        renderer.drawText(NOTOSANS_14_FONT_ID, pageWidth - pad - 60, y, arrow, true, EpdFontFamily::BOLD);
      }
      y += renderer.getLineHeight(NOTOSANS_14_FONT_ID) + 16;
    } else {
      renderer.drawText(NOTOSANS_14_FONT_ID, pad, y, statusText().c_str(), true, EpdFontFamily::BOLD);
      y += renderer.getLineHeight(NOTOSANS_14_FONT_ID) + 16;
    }
    if (showNumber) renderer.drawText(UI_10_FONT_ID, pad, y, statusText().c_str());
    y += lineH + 32;
  }
  if (!statusLine.empty()) renderer.drawText(UI_10_FONT_ID, pad, std::min(y, pageHeight - 120), statusLine.c_str());

  // Confirm means "Refresh" on Wi-Fi, but "hold to forget the bond" on BLE --
  // it's push-based there, so a plain press has nothing to refresh. Mode
  // (auto/manual) only means something on Wi-Fi too. Source (Right button)
  // always works, so the user can escape a failed source either way.
  const auto labels = mappedInput.mapLabels(tr(STR_BACK),
                                             source == Source::Wifi ? tr(STR_GLUCOSE_REFRESH)
                                                                     : tr(STR_GLUCOSE_FORGET_HINT),
                                             source == Source::Wifi ? tr(STR_GLUCOSE_MODE_HINT) : "",
                                             tr(STR_GLUCOSE_SOURCE_HINT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
