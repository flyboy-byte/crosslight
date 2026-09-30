#include "BibleMenuActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

BibleMenuActivity::BibleMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                     const bool currentLocationBookmarked, const int bookmarkCount)
    : UiListActivity("BibleMenu", renderer, mappedInput),
      currentLocationBookmarked(currentLocationBookmarked),
      bookmarkCount(bookmarkCount) {}

void BibleMenuActivity::onEnter() {
  UiListActivity::onEnter();

  // labels owns the backing storage for every non-literal row string; ListItem
  // holds bare const char*, so these must outlive the rows.
  labels.push_back(std::to_string(bookmarkCount));

  fui::ListItem goToBook;
  goToBook.label = tr(STR_SELECT_BOOK);
  goToBook.actionValue = GoToBook;
  rowItems.push_back(goToBook);

  fui::ListItem bookmarks;
  bookmarks.label = tr(STR_BOOKMARKS);
  bookmarks.value = labels[0].c_str();
  bookmarks.enabled = bookmarkCount > 0;
  bookmarks.actionValue = OpenBookmarks;
  rowItems.push_back(bookmarks);

  fui::ListItem toggle;
  toggle.label = tr(STR_TOGGLE_BOOKMARK);
  toggle.toggle = true;
  toggle.toggleChecked = currentLocationBookmarked;
  toggle.actionValue = ToggleBookmark;
  rowItems.push_back(toggle);
}

const char* BibleMenuActivity::headerTitle() const { return tr(STR_BIBLE); }

void BibleMenuActivity::activateIndex(const int index) {
  if (index < 0 || index >= static_cast<int>(rowItems.size())) return;
  if (!rowItems[index].enabled) return;
  app.clearTapFlash();
  nav.selected = index;
  setResult(MenuResult{rowItems[index].actionValue});
  finish();
}

void BibleMenuActivity::buildScreen(UiScreen& screen) {
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
