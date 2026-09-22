#include "BibleSearchResultsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <utility>

#include "MappedInputManager.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

BibleSearchResultsActivity::BibleSearchResultsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                       std::string query,
                                                       std::vector<BibleChapterLoader::SearchHit> hits,
                                                       const bool truncated)
    : UiListActivity("BibleSearchResults", renderer, mappedInput),
      title("\"" + std::move(query) + "\""),
      hits(std::move(hits)),
      truncated(truncated) {}

void BibleSearchResultsActivity::onEnter() {
  UiListActivity::onEnter();

  // reserve() is load-bearing: ListItem holds bare const char* into these strings.
  labels.reserve(hits.size());
  rowItems.reserve(hits.size());
  for (const auto& hit : hits) {
    labels.push_back(hit.book + " " + std::to_string(hit.chapter) + ":" + std::to_string(hit.verse));
    fui::ListItem item;
    item.label = labels.back().c_str();
    item.subtitle = hit.snippet.c_str();
    item.actionValue = static_cast<int16_t>(rowItems.size());
    rowItems.push_back(item);
  }
  if (truncated && !rowItems.empty()) rowItems.front().sectionHeading = tr(STR_FIRST_100_MATCHES);
}

void BibleSearchResultsActivity::activateIndex(const int index) {
  if (index < 0 || index >= static_cast<int>(hits.size())) return;
  app.clearTapFlash();
  nav.selected = index;
  setResult(BibleVerseResult{hits[index].book, hits[index].chapter, hits[index].verse});
  finish();
}

void BibleSearchResultsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (rowItems.empty()) {
    screen.centeredText(tr(STR_NO_MATCHES), screen.theme().bodyText);
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
