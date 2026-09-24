#include "BibleHubActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "activities/bible/BibleReaderActivity.h"
#include "activities/bible/BibleMemoryWorkActivity.h"
#include "activities/bible/BibleTranslationsActivity.h"
#include "bible/BibleBookmarkStore.h"
#include "bible/BibleMemoryWork.h"
#include "bible/BibleReadingStateStore.h"
#include "bible/BibleTranslations.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

BibleHubActivity::BibleHubActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("BibleHub", renderer, mappedInput) {}

void BibleHubActivity::onEnter() {
  UiListActivity::onEnter();
  BIBLE_READING_STATE.loadFromFile();
  BIBLE_BOOKMARKS.loadFromFile();
  buildRows();
}

void BibleHubActivity::buildRows() {
  const std::string abbr = BibleTranslations::current();
  const bool haveTranslation = !abbr.empty();
  const bool haveMemoryWork = !bible_memory::availableCourses().empty();

  translationLabel = haveTranslation ? BibleTranslations::shortLabel(abbr) : "";
  continueSubtitle.clear();
  if (BIBLE_READING_STATE.hasSavedPosition()) {
    continueSubtitle = BIBLE_READING_STATE.bookName + " " + std::to_string(BIBLE_READING_STATE.chapter);
    if (haveTranslation) continueSubtitle += " (" + translationLabel + ")";
  }
  bookmarkCount = std::to_string(BIBLE_BOOKMARKS.all().size());

  rowItems.clear();
  rowItems.reserve(7);  // add() hands back a reference into the vector
  auto add = [this](const Row row, const char* label, const bool enabled) -> fui::ListItem& {
    fui::ListItem item;
    item.label = label;
    item.actionValue = row;
    item.enabled = enabled;
    rowItems.push_back(item);
    return rowItems.back();
  };
  add(Continue, tr(STR_CONTINUE_READING), haveTranslation).subtitle =
      continueSubtitle.empty() ? nullptr : continueSubtitle.c_str();
  add(SelectBook, tr(STR_SELECT_BOOK), haveTranslation);
  add(GoToVerse, tr(STR_GO_TO_VERSE), haveTranslation);
  add(Search, tr(STR_SEARCH_BIBLE), haveTranslation);
  add(Bookmarks, tr(STR_BOOKMARKS), haveTranslation && !BIBLE_BOOKMARKS.all().empty()).value = bookmarkCount.c_str();
  add(MemoryWork, tr(STR_MEMORY_WORK), haveTranslation && haveMemoryWork);
  add(Translations, tr(STR_TRANSLATIONS), true).value = translationLabel.c_str();
}

const char* BibleHubActivity::headerTitle() const { return tr(STR_BIBLE); }

void BibleHubActivity::activateIndex(const int index) {
  if (index < 0 || index >= static_cast<int>(rowItems.size()) || !rowItems[index].enabled) return;
  app.clearTapFlash();
  nav.selected = index;

  using Action = BibleReaderActivity::InitialAction;
  switch (static_cast<Row>(rowItems[index].actionValue)) {
    case Continue:
      activityManager.replaceActivity(std::make_unique<BibleReaderActivity>(renderer, mappedInput, Action::Resume));
      return;
    case SelectBook:
      activityManager.replaceActivity(
          std::make_unique<BibleReaderActivity>(renderer, mappedInput, Action::BookPicker));
      return;
    case GoToVerse:
      activityManager.replaceActivity(std::make_unique<BibleReaderActivity>(renderer, mappedInput, Action::VerseJump));
      return;
    case Search:
      activityManager.replaceActivity(std::make_unique<BibleReaderActivity>(renderer, mappedInput, Action::Search));
      return;
    case Bookmarks:
      activityManager.replaceActivity(std::make_unique<BibleReaderActivity>(renderer, mappedInput, Action::Bookmarks));
      return;
    case MemoryWork:
      startActivityForResult(std::make_unique<BibleMemoryWorkActivity>(renderer, mappedInput),
                             [](const ActivityResult&) {});
      return;
    case Translations:
      startActivityForResult(std::make_unique<BibleTranslationsActivity>(renderer, mappedInput),
                             [this](const ActivityResult&) {
                               RenderLock lock(*this);
                               buildRows();
                             });
      return;
  }
}

void BibleHubActivity::buildScreen(UiScreen& screen) {
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
