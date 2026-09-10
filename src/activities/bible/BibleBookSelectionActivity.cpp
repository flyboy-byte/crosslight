#include "BibleBookSelectionActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <utility>

#include "MappedInputManager.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

BibleBookSelectionActivity::BibleBookSelectionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                        std::string translationPath)
    : UiListActivity("BibleBookSelection", renderer, mappedInput), translationPath(std::move(translationPath)) {}

void BibleBookSelectionActivity::onEnter() {
  UiListActivity::onEnter();

  loaded = BibleChapterLoader::loadBookIndex(translationPath.c_str(), books);
  if (!loaded) return;

  // Built once here, not per buildScreen() call: labels are static and
  // activateIndex() finishes the activity immediately on selection.
  labels.reserve(books.size());
  rowItems.reserve(books.size());
  for (size_t i = 0; i < books.size(); ++i) {
    labels.push_back(books[i].name);
    fui::ListItem item;
    item.label = labels[i].c_str();
    item.actionValue = static_cast<int16_t>(i);
    rowItems.push_back(item);
  }
}

const char* BibleBookSelectionActivity::headerTitle() const { return tr(STR_SELECT_BOOK); }

void BibleBookSelectionActivity::activateIndex(const int index) {
  if (index < 0 || index >= static_cast<int>(books.size())) return;
  app.clearTapFlash();
  nav.selected = index;
  setResult(BibleBookResult{books[index].name, books[index].chapterCount});
  finish();
}

void BibleBookSelectionActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (!loaded || books.empty()) {
    screen.centeredText(tr(STR_NO_BOOKS), screen.theme().bodyText);
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
