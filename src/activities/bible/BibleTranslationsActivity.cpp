#include "BibleTranslationsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "activities/bible/BibleDownloadActivity.h"
#include "activities/util/ChoiceActivity.h"
#include "bible/BibleTranslations.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

BibleTranslationsActivity::BibleTranslationsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("BibleTranslations", renderer, mappedInput) {}

void BibleTranslationsActivity::onEnter() {
  UiListActivity::onEnter();
  buildRows();
}

void BibleTranslationsActivity::buildRows() {
  currentAbbr = BibleTranslations::current();
  const auto installed = BibleTranslations::installed();
  size_t presetCount = 0;
  const BiblePreset* presets = BibleTranslations::presets(presetCount);

  abbrs.clear();
  for (const auto& abbr : installed) abbrs.push_back(abbr);
  installedCount = abbrs.size();
  for (size_t i = 0; i < presetCount; ++i) {
    if (!BibleTranslations::isInstalled(presets[i].abbr)) abbrs.push_back(presets[i].abbr);
  }

  // Two strings per row (label, subtitle); reserve so ListItem pointers stay valid.
  labels.clear();
  labels.reserve(abbrs.size() * 2);
  rowItems.clear();
  rowItems.reserve(abbrs.size());
  for (size_t i = 0; i < abbrs.size(); ++i) {
    const bool isInstalled = i < installedCount;
    const auto* preset = BibleTranslations::findPreset(abbrs[i]);
    labels.push_back(BibleTranslations::displayName(abbrs[i]));
    labels.push_back(BibleTranslations::shortLabel(abbrs[i]) + (preset ? "  -  " + std::string(preset->license) : ""));
    fui::ListItem item;
    item.label = labels[labels.size() - 2].c_str();
    item.subtitle = labels.back().c_str();
    item.value = isInstalled ? (abbrs[i] == currentAbbr ? tr(STR_SELECTED) : "") : tr(STR_DOWNLOAD);
    item.actionValue = static_cast<int16_t>(i);
    if (i == 0 && installedCount > 0) item.sectionHeading = tr(STR_INSTALLED);
    if (i == installedCount) item.sectionHeading = tr(STR_DOWNLOAD);
    rowItems.push_back(item);
  }
}

const char* BibleTranslationsActivity::headerTitle() const { return tr(STR_TRANSLATIONS); }

void BibleTranslationsActivity::activateIndex(const int index) {
  if (index < 0 || index >= static_cast<int>(abbrs.size())) return;
  app.clearTapFlash();
  nav.selected = index;
  if (static_cast<size_t>(index) < installedCount) {
    BibleTranslations::setCurrent(abbrs[index]);
    finish();
    return;
  }
  confirmDownload(abbrs[index]);
}

void BibleTranslationsActivity::confirmDownload(const std::string& abbr) {
  const auto* preset = BibleTranslations::findPreset(abbr);
  std::string body = std::string(tr(STR_LICENSE)) + ": " + (preset ? preset->license : "?") + "\n" +
                     tr(STR_DOWNLOAD_SIZE_HINT);
  std::vector<std::string> options = {tr(STR_DOWNLOAD), tr(STR_CANCEL)};
  startActivityForResult(std::make_unique<ChoiceActivity>(renderer, mappedInput, BibleTranslations::displayName(abbr),
                                                          std::move(body), std::move(options)),
                         [this, abbr](const ActivityResult& res) {
                           const auto* choice = std::get_if<MenuResult>(&res.data);
                           if (res.isCancelled || !choice || choice->action != 0) return;
                           // Leaving the download screen restarts the device (Wi-Fi
                           // teardown), so it selects the translation itself on success.
                           startActivityForResult(std::make_unique<BibleDownloadActivity>(renderer, mappedInput, abbr),
                                                  [this](const ActivityResult&) {
                                                    RenderLock lock(*this);
                                                    buildRows();
                                                  });
                         });
}

void BibleTranslationsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  fui::ListProps props;
  props.items = rowItems.data();
  props.count = static_cast<uint16_t>(rowItems.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // physical buttons stay in loop()
  syncListViewport(screen, props);
  screen.list(props);
}
