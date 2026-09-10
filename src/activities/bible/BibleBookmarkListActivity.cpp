#include "BibleBookmarkListActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "bible/BibleBookmarkStore.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

BibleBookmarkListActivity::BibleBookmarkListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("BibleBookmarkList", renderer, mappedInput) {}

void BibleBookmarkListActivity::onEnter() {
  UiListActivity::onEnter();

  const auto& bookmarks = BIBLE_BOOKMARKS.all();
  // reserve() is load-bearing, not an optimization: ListItem holds bare
  // const char* into these strings, so a reallocation mid-loop would dangle
  // every row built so far.
  labels.reserve(bookmarks.size());
  pageLabels.reserve(bookmarks.size());
  rowItems.reserve(bookmarks.size());
  for (const auto& bookmark : bookmarks) {
    labels.push_back(bookmark.book + " " + std::to_string(bookmark.chapter));
    // Without the page, two bookmarks in the same chapter render identically.
    // It's pagination-dependent (a font-size change renumbers it), so it's a
    // disambiguator here, not a promise about where the jump lands.
    pageLabels.push_back("p" + std::to_string(bookmark.page + 1));
    fui::ListItem item;
    item.label = labels.back().c_str();
    item.value = pageLabels.back().c_str();
    item.actionValue = static_cast<int16_t>(rowItems.size());
    rowItems.push_back(item);
  }
}

const char* BibleBookmarkListActivity::headerTitle() const { return tr(STR_BOOKMARKS); }

void BibleBookmarkListActivity::activateIndex(const int index) {
  const auto& bookmarks = BIBLE_BOOKMARKS.all();
  if (index < 0 || index >= static_cast<int>(bookmarks.size())) return;
  app.clearTapFlash();
  nav.selected = index;
  setResult(BibleBookmarkResult{bookmarks[index].book, bookmarks[index].chapter, bookmarks[index].page});
  finish();
}

void BibleBookmarkListActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (rowItems.empty()) {
    screen.centeredText(tr(STR_NO_BOOKMARKS), screen.theme().bodyText);
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
