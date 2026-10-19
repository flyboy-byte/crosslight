#include "GlucoseMonitorActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <HalClock.h>
#include <TrustedTime.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdio>
#include <vector>

#include "CrossPointSettings.h"
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

// The graph spans the bridge's default history window.
constexpr int64_t kGraphWindowMs = 3LL * 60 * 60 * 1000;
constexpr uint32_t kHistoryEveryMs = 5UL * 60 * 1000;  // history is a bigger fetch than "latest"; space it out
constexpr size_t kMaxBleHistory = 200;
}  // namespace

GlucoseMonitorActivity::GlucoseMonitorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("GlucoseMonitor", renderer, mappedInput) {}

void GlucoseMonitorActivity::onEnter() {
  Activity::onEnter();
  // Respect whatever was last picked on this screen's source switch, so
  // Bluetooth stays selected on the next open instead of defaulting back to
  // Wi-Fi every time.
  source = SETTINGS.glucoseSource != 0 ? Source::Ble : Source::Wifi;
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
    // Don't join Wi-Fi on open: the screen (and its source switch) must be
    // usable immediately, even out of range. Read starts the connection.
    connected = false;
    RenderLock lock(*this);
    statusLine = tr(STR_GLUCOSE_TAP_READ);
    requestUpdate();
  } else {
    beginBle();
  }
}

void GlucoseMonitorActivity::connectWifi() {
  {
    WiFi.mode(WIFI_STA);
    startActivityForResult(
        std::make_unique<WifiSelectionActivity>(renderer, mappedInput, /*autoConnect=*/true, /*quiet=*/true),
        [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
  }
}

void GlucoseMonitorActivity::beginBle() {
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

void GlucoseMonitorActivity::endSource(const bool returnToMonitor) {
  if (source == Source::Wifi) {
    // Same radio teardown as ClaudePanelActivity: TLS leaves the heap too
    // fragmented to keep running, so a silent restart reclaims it.
    if (WiFi.getMode() != WIFI_MODE_NULL) {
      WiFi.disconnect(false);
      delay(30);
      if (returnToMonitor) {
        silentRestartToGlucose();  // source already saved; the monitor reopens on it
      } else {
        silentRestart();
      }
    }
  } else {
    bleServer.end();
  }
}

void GlucoseMonitorActivity::switchSource(const Source next) {
  if (next == source) return;
  history.clear();
  // Persist first: leaving Wi-Fi reboots (heap defrag) and the monitor must reopen on the new source.
  SETTINGS.glucoseSource = (next == Source::Ble) ? 1 : 0;
  SETTINGS.saveToFile();
  endSource(/*returnToMonitor=*/true);
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
  // History rides along on a slower cadence, fetched before the render lock is
  // taken (it is a second TLS request). Failure just leaves the old graph.
  std::vector<libre::HistoryPoint> fetched;
  bool gotHistory = false;
  if (r.error == libre::Error::Ok && (history.empty() || millis() - historyFetchedMs >= kHistoryEveryMs)) {
    gotHistory = libre::fetchHistory(fetched) == libre::Error::Ok;
  }
  RenderLock lock(*this);
  lastFetchMs = millis();
  if (gotHistory) {
    history.clear();
    const int64_t base = static_cast<int64_t>(millis());
    for (const auto& pt : fetched) history.push_back({pt.value, base - static_cast<int64_t>(pt.ageS) * 1000});
    historyFetchedMs = millis();
  }
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

GlucoseMonitorActivity::Layout GlucoseMonitorActivity::computeLayout() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int pad = metrics.contentSidePadding;
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  constexpr int kCtlH = 56;

  Layout l;
  const int top = metrics.topPadding + metrics.headerHeight + 12;
  const int half = (pageWidth - 2 * pad) / 2;
  l.wifi = {pad, top, half, kCtlH};
  l.ble = {pad + half, top, half, kCtlH};
  int y = top + kCtlH + 14;
  l.infoY = y;
  // The line under the switch is the auto/manual mode on Wi-Fi (tappable) and
  // the forget-phone hint on BLE (text only), so it takes the same space either way.
  if (source == Source::Wifi) l.mode = {pad, y - 6, pageWidth - 2 * pad, lineH + 12};
  y += lineH + 28;
  l.bodyTop = y + 16;
  const int readH = 64;
  l.read = {pad, pageHeight - metrics.buttonHintsHeight - 16 - readH, pageWidth - 2 * pad, readH};
  return l;
}

void GlucoseMonitorActivity::readNow() {
  if (source == Source::Wifi) {
    stopped = false;  // an explicit Read is a deliberate retry, not an automatic one
    refresh();
    return;
  }
  // BLE is push-based: the phone writes, the reader can't pull. Read just
  // re-judges freshness now instead of waiting for the next recheck tick.
  forceBleRecheck = true;
  RenderLock lock(*this);
  requestUpdate();
}

void GlucoseMonitorActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    // Launched from Home with an empty stack, so a plain finish() lands on Home
    // with the top row selected; name the row so Home keeps Glucose highlighted
    // like every other Home-launched screen.
    activityManager.goHome(HomeMenuItem::GLUCOSE);
    return;
  }
  const Layout layout = computeLayout();
  int x = 0;
  int y = 0;
  const bool tapped = mappedInput.wasScreenTapped(x, y);
  // Switching source must work even if the current one failed to connect
  // (Wi-Fi join failure, or no BLE radio), so this runs before the
  // `!connected` gate below.
  if (tapped) {
    if (layout.wifi.contains(x, y)) {
      switchSource(Source::Wifi);
      return;
    }
    if (layout.ble.contains(x, y)) {
      switchSource(Source::Ble);
      return;
    }
  }
  if (source == Source::Wifi && !connected) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) || (tapped && layout.read.contains(x, y))) {
      connectWifi();
    }
    return;
  }
  if (!connected) return;

  if (source == Source::Ble) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) || (tapped && layout.read.contains(x, y))) {
      readNow();
    }
    pollBle();
    return;
  }

  // Wi-Fi: only a deliberate press reads or toggles -- a stray tap elsewhere
  // does nothing, unlike the old whole-screen tap-to-refresh.
  const bool doRefresh = mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
                         (tapped && layout.read.contains(x, y));
  const bool toggle = mappedInput.wasReleased(MappedInputManager::Button::Left) ||
                      (tapped && layout.mode.contains(x, y));
  if (doRefresh) stopped = false;
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

  int64_t phoneEpoch = 0;
  if (bleServer.takeTimeSync(phoneEpoch)) {
#if defined(ARDUINO_ARCH_ESP32)  // the desktop simulator's clock has no setter
    if (halClock.setUtc(static_cast<time_t>(phoneEpoch))) trustedtime::adopt(phoneEpoch);
#endif
  }
  const bool gotNew = bleServer.takeLatest(lastBlePacket);
  const uint32_t now = millis();
  if (gotNew) {
    // A real write just arrived: this is the receipt time freshness is judged
    // against from here on -- the reader's own clock is never consulted.
    lastBleWriteMs = now;
    LOG_INF("LIBRE_BLE", "write: value=%u trend=%u flags(H/L/U)=%u/%u/%u", lastBlePacket.valueMgdl,
            static_cast<unsigned>(lastBlePacket.trend), lastBlePacket.high, lastBlePacket.low,
            lastBlePacket.unavailable);
  } else {
    if (!haveReading) {
      forceBleRecheck = false;  // nothing received yet, so there is nothing to re-judge
      return;
    }
    if (!forceBleRecheck && now - lastFetchMs < kBleRecheckMs) return;  // not due for a staleness re-check
  }
  forceBleRecheck = false;
  lastFetchMs = now;

  RenderLock lock(*this);
  reading = libre::bleReadingFromPacket(lastBlePacket);
  // Freshness by receipt time: once no write has landed for the stale window,
  // drop to Unavailable no matter what the last packet said.
  const uint32_t sinceWriteMs = now - lastBleWriteMs;
  if (sinceWriteMs >= libre::kBleStaleAfterMs) {
    reading.status = libre::ReadingStatus::Unavailable;
  } else if (reading.status == libre::ReadingStatus::Ok) {
    reading.ageS = static_cast<int>(sinceWriteMs / 1000);
  }
  haveReading = true;
  if (gotNew && reading.status == libre::ReadingStatus::Ok) {
    if (history.size() >= kMaxBleHistory) history.erase(history.begin());
    history.push_back({static_cast<int16_t>(reading.value), static_cast<int64_t>(now)});
  }
  statusLine = "";
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

void GlucoseMonitorActivity::drawGraph(const int x, const int y, const int w, const int h) const {
  const int64_t nowMs = static_cast<int64_t>(millis());
  int lo = 70;
  int hi = 180;
  int count = 0;
  int minV = 1000;
  int maxV = 0;
  for (const auto& pt : history) {
    if (nowMs - pt.atMs > kGraphWindowMs) continue;
    minV = std::min<int>(minV, pt.value);
    maxV = std::max<int>(maxV, pt.value);
    ++count;
  }
  if (count < 2) return;
  lo = std::min(lo, minV - 10);
  hi = std::max(hi, maxV + 10);
  const int range = hi - lo;

  auto yFor = [&](const int v) { return y + h - 1 - (v - lo) * (h - 1) / range; };
  renderer.drawRect(x, y, w, h, 1, true);
  // Target range guides, 70 and 180 mg/dL.
  for (const int guide : {70, 180}) {
    const int gy = yFor(guide);
    for (int gx = x + 2; gx < x + w - 2; gx += 8) renderer.drawLine(gx, gy, gx + 3, gy, true);
  }
  int px = -1;
  int py = -1;
  for (const auto& pt : history) {
    const int64_t age = nowMs - pt.atMs;
    if (age > kGraphWindowMs || age < 0) continue;
    const int gx = x + w - 1 - static_cast<int>(age * (w - 1) / kGraphWindowMs);
    const int gy = yFor(pt.value);
    if (px >= 0) renderer.drawLine(px, py, gx, gy, 2, true);
    px = gx;
    py = gy;
  }
  char cap[48];
  const int capY = y + h + 6;
  snprintf(cap, sizeof(cap), tr(STR_GLUCOSE_HISTORY_FMT), minV, maxV, count);
  renderer.drawText(UI_10_FONT_ID, x, capY, cap);
}

void GlucoseMonitorActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int pad = metrics.contentSidePadding;
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_GLUCOSE_MONITOR));

  const Layout layout = computeLayout();

  // Source switch: the selected half is filled, the other an outline. Both
  // halves are touch targets (see loop()).
  auto drawSegment = [&](const Box& b, const char* label, bool selected) {
    if (selected) {
      renderer.fillRect(b.x, b.y, b.w, b.h, true);
    } else {
      renderer.drawRect(b.x, b.y, b.w, b.h, 2, true);
    }
    const int tw = renderer.getTextWidth(UI_12_FONT_ID, label, EpdFontFamily::BOLD);
    const int th = renderer.getLineHeight(UI_12_FONT_ID);
    renderer.drawText(UI_12_FONT_ID, b.x + (b.w - tw) / 2, b.y + (b.h - th) / 2, label, /*black=*/!selected,
                      EpdFontFamily::BOLD);
  };
  drawSegment(layout.wifi, tr(STR_GLUCOSE_SOURCE_WIFI), source == Source::Wifi);
  drawSegment(layout.ble, tr(STR_GLUCOSE_SOURCE_BLE), source == Source::Ble);

  if (source == Source::Wifi) {
    // Tapping this line flips auto/manual (see loop()).
    const char* modeText =
        I18n::getInstance().get(autoRefresh ? StrId::STR_GLUCOSE_AUTO_60S : StrId::STR_GLUCOSE_MANUAL);
    renderer.drawText(UI_10_FONT_ID, pad, layout.infoY, modeText);
  }
  int y = layout.bodyTop;

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
  if (!statusLine.empty()) {
    renderer.drawText(UI_10_FONT_ID, pad, std::min(y, layout.read.y - lineH - 12), statusLine.c_str());
    y += lineH + 12;
  }
  // Trend graph in whatever room is left above the Read button.
  const int graphBottom = layout.read.y - 2 * lineH - 24;
  if (history.size() >= 2 && graphBottom - y >= 90) drawGraph(pad, y, pageWidth - 2 * pad, graphBottom - y);

  // Read button, bottom of the content area.
  renderer.drawRect(layout.read.x, layout.read.y, layout.read.w, layout.read.h, 2, true);
  {
    const char* label = tr(STR_GLUCOSE_READ);
    const int tw = renderer.getTextWidth(UI_12_FONT_ID, label, EpdFontFamily::BOLD);
    const int th = renderer.getLineHeight(UI_12_FONT_ID);
    renderer.drawText(UI_12_FONT_ID, layout.read.x + (layout.read.w - tw) / 2,
                      layout.read.y + (layout.read.h - th) / 2, label, true, EpdFontFamily::BOLD);
  }

  // Confirm is Read on both sources. Source is only ever changed on the on-screen switch.
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_GLUCOSE_READ),
                                             source == Source::Wifi ? tr(STR_GLUCOSE_MODE_HINT) : "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
