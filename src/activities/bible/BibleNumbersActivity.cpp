#include "BibleNumbersActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <utility>

#include "MappedInputManager.h"
#include "activities/bible/BibleNumberDetailActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

BibleNumbersActivity::BibleNumbersActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                           std::string translationPath)
    : UiListActivity("BibleNumbers", renderer, mappedInput), translationPath(std::move(translationPath)) {}

void BibleNumbersActivity::onEnter() {
  UiListActivity::onEnter();
  studyPaths = bible_numbers::availableStudies();

  labels.clear();
  rowItems.clear();
  labels.reserve(studyPaths.size());
  rowItems.reserve(studyPaths.size());

  for (const auto& path : studyPaths) {
    bible_numbers::NumberStudy peek;
    if (!bible_numbers::loadStudy(path.c_str(), peek)) continue;
    // ListItem holds a bare const char* into this, so it must not reallocate.
    labels.push_back(peek.name);
    fui::ListItem item;
    item.label = labels.back().c_str();
    item.actionValue = static_cast<int16_t>(rowItems.size());
    rowItems.push_back(item);
  }
}

const char* BibleNumbersActivity::headerTitle() const { return tr(STR_BIBLE_NUMBERS); }

void BibleNumbersActivity::activateIndex(const int index) {
  if (index < 0 || index >= static_cast<int>(rowItems.size())) return;
  app.clearTapFlash();
  nav.selected = index;
  startActivityForResult(
      std::make_unique<BibleNumberDetailActivity>(renderer, mappedInput, studyPaths[index], translationPath),
      [](const ActivityResult&) {});
}

void BibleNumbersActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (rowItems.empty()) {
    screen.centeredText(tr(STR_NO_BIBLE_NUMBERS), screen.theme().bodyText);
    return;
  }

  fui::ListProps props;
  props.items = rowItems.data();
  props.count = static_cast<uint16_t>(rowItems.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // physical buttons stay in loop()
  syncListViewport(screen, props);
  screen.list(props);
}
