#include "BibleDownloadActivity.h"

#include <FontCacheManager.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include <utility>

#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "bible/BibleTranslations.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"

namespace {
constexpr int PROGRESS_STEP_PERCENT = 5;
constexpr unsigned long PROGRESS_MIN_UPDATE_MS = 5000;
}  // namespace

BibleDownloadActivity::BibleDownloadActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string abbr)
    : Activity("BibleDownload", renderer, mappedInput), abbr(std::move(abbr)) {}

void BibleDownloadActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    onWifiSelectionComplete(true);
    return;
  }
  WiFi.mode(WIFI_STA);
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
}

void BibleDownloadActivity::onExit() {
  Activity::onExit();
  if (WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

void BibleDownloadActivity::onWifiSelectionComplete(const bool connected) {
  if (!connected) {
    RenderLock lock(*this);
    state = FAILED;
    failure = tr(STR_WIFI_CONN_FAILED);
    requestUpdate();
    return;
  }
  download();
}

void BibleDownloadActivity::download() {
  {
    RenderLock lock(*this);
    state = DOWNLOADING;
    downloaded = total = 0;
  }
  requestUpdateAndWait();

  const std::string finalPath = BibleTranslations::pathFor(abbr);
  const std::string partPath = finalPath + ".part";
  const std::string dir = finalPath.substr(0, finalPath.find_last_of('/'));
  auto fail = [this](const char* why) {
    RenderLock lock(*this);
    state = FAILED;
    failure = why;
    requestUpdate();
  };
  if (!Storage.ensureDirectoryExists(dir.c_str())) {
    fail(tr(STR_DOWNLOAD_FAILED));
    return;
  }

  // A multi-MB TLS transfer needs heap the SD-font caches may be holding (see
  // OpdsBookBrowserActivity::downloadBook); they repopulate on demand.
  if (auto* fcm = renderer.getFontCacheManager()) fcm->releaseSdFontCaches();
  if (ESP.getFreeHeap() < HttpDownloader::MIN_TLS_FREE_HEAP ||
      ESP.getMaxAllocHeap() < HttpDownloader::MIN_TLS_MAX_ALLOC) {
    LOG_ERR("BIBLE", "Low heap for download (%u free, %u max block)", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    fail(tr(STR_DOWNLOAD_FAILED));
    return;
  }

  const std::string url = BibleTranslations::downloadUrl(abbr);
  LOG_INF("BIBLE", "Downloading %s -> %s", url.c_str(), partPath.c_str());
  Storage.remove(partPath.c_str());
  int lastPercent = -1;
  unsigned long lastUpdateMs = 0;
  const auto result = HttpDownloader::downloadToFile(
      url, partPath,
      [this, &lastPercent, &lastUpdateMs](const size_t done, const size_t size) {
        downloaded = done;
        total = size;
        // The loop is blocked for the whole transfer; pump input here so Back can cancel.
        mappedInput.update(true);
        if (mappedInput.wasReleased(MappedInputManager::Button::Back)) cancelRequested = true;
        const int percent = size > 0 ? static_cast<int>(static_cast<uint64_t>(done) * 100 / size) : 0;
        const unsigned long now = millis();
        if (percent >= 100 || lastPercent < 0 || percent >= lastPercent + PROGRESS_STEP_PERCENT ||
            now - lastUpdateMs >= PROGRESS_MIN_UPDATE_MS) {
          lastPercent = percent;
          lastUpdateMs = now;
          requestUpdate(true);
        }
      },
      &cancelRequested);

  if (result != HttpDownloader::OK) {
    Storage.remove(partPath.c_str());
    LOG_ERR("BIBLE", "Download of %s failed (%d)", abbr.c_str(), static_cast<int>(result));
    if (result == HttpDownloader::ABORTED) {
      finish();
      return;
    }
    fail(tr(STR_DOWNLOAD_FAILED));
    return;
  }
  // Only a complete file ever gets the final name, so a dropped transfer can't look installed.
  Storage.remove(finalPath.c_str());
  if (!Storage.rename(partPath.c_str(), finalPath.c_str())) {
    fail(tr(STR_DOWNLOAD_FAILED));
    return;
  }
  BibleTranslations::setCurrent(abbr);
  LOG_INF("BIBLE", "Installed %s (%u bytes)", abbr.c_str(), static_cast<unsigned>(downloaded));
  RenderLock lock(*this);
  state = DONE;
  requestUpdate();
}

void BibleDownloadActivity::loop() {
  if (state != DONE && state != FAILED) return;
  int x = 0;
  int y = 0;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm) || mappedInput.wasScreenTapped(x, y)) {
    finish();
  }
}

void BibleDownloadActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  const int top = pageHeight / 3;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_TRANSLATIONS));
  const std::string name = BibleTranslations::displayName(abbr);
  renderer.drawCenteredText(UI_10_FONT_ID, top, name.c_str(), true, EpdFontFamily::BOLD);

  const int y = top + lineH * 2;
  if (state == CONNECTING) {
    renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_LOADING_POPUP));
  } else if (state == DOWNLOADING) {
    renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_DOWNLOADING));
    const int barY = y + lineH + metrics.verticalSpacing;
    const int percent = total > 0 ? static_cast<int>(static_cast<uint64_t>(downloaded) * 100 / total) : 0;
    GUI.drawProgressBar(renderer,
                        Rect{metrics.contentSidePadding, barY, pageWidth - metrics.contentSidePadding * 2,
                             metrics.progressBarHeight},
                        percent, 100);
    const std::string sizes = std::to_string(downloaded / 1024) + " / " + std::to_string(total / 1024) + " KB";
    renderer.drawCenteredText(UI_10_FONT_ID, barY + metrics.progressBarHeight + lineH, sizes.c_str());
  } else if (state == DONE) {
    renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_DOWNLOAD_COMPLETE), true, EpdFontFamily::BOLD);
  } else {
    renderer.drawCenteredText(UI_10_FONT_ID, y, failure ? failure : tr(STR_DOWNLOAD_FAILED), true,
                              EpdFontFamily::BOLD);
  }

  if (state == DONE || state == FAILED) {
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  renderer.displayBuffer();
}
