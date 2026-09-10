#include "BibleChapterSelectionActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <utility>

#include "MappedInputManager.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

BibleChapterSelectionActivity::BibleChapterSelectionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                              std::string bookName, const int chapterCount,
                                                              const int currentChapter)
    : UiListActivity("BibleChapterSelection", renderer, mappedInput),
      bookName(std::move(bookName)),
      chapterCount(chapterCount),
      currentChapter(currentChapter) {}

void BibleChapterSelectionActivity::onEnter() {
  UiListActivity::onEnter();

  // 1-based chapter numbers, current one at the top of the viewport.
  nav.selected = (currentChapter >= 1 && currentChapter <= chapterCount) ? currentChapter - 1 : 0;

  labels.reserve(chapterCount);
  rowItems.reserve(chapterCount);
  for (int i = 0; i < chapterCount; ++i) {
    labels.push_back(std::to_string(i + 1));
    fui::ListItem item;
    item.label = labels[i].c_str();
    item.actionValue = static_cast<int16_t>(i);
    rowItems.push_back(item);
  }
}

const char* BibleChapterSelectionActivity::headerTitle() const { return tr(STR_SELECT_CHAPTER); }

void BibleChapterSelectionActivity::activateIndex(const int index) {
  if (index < 0 || index >= chapterCount) return;
  app.clearTapFlash();
  nav.selected = index;
  setResult(BibleChapterResult{index + 1});
  finish();
}

void BibleChapterSelectionActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (rowItems.empty()) {
    screen.centeredText(tr(STR_NO_CHAPTERS), screen.theme().bodyText);
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
