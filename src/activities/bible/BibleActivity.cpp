#include "BibleActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// SD-first: a translation dropped at this path (getBible v2 JSON format,
// e.g. downloaded from https://api.getbible.net/v2/kjv.json) just works,
// no network needed. See PLAN.md.
constexpr const char* KJV_PATH = "/Bible/KJV/kjv.json";
}  // namespace

void BibleActivity::onEnter() {
  Activity::onEnter();
  loaded = BibleChapterLoader::loadChapter(KJV_PATH, "Genesis", 1, verses);
  requestUpdate();
}

void BibleActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    goBack();
  }
}

void BibleActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int x = metrics.contentSidePadding;
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BIBLE));

  int y = metrics.topPadding + metrics.headerHeight + lineH;

  if (!loaded) {
    renderer.drawText(UI_10_FONT_ID, x, y, tr(STR_BIBLE_NOT_FOUND), true, EpdFontFamily::BOLD);
    y += lineH * 2;
    renderer.drawText(UI_10_FONT_ID, x, y, KJV_PATH, true);
  } else {
    renderer.drawText(UI_10_FONT_ID, x, y, "Genesis 1 (KJV)", true, EpdFontFamily::BOLD);
    y += lineH * 2;
    // Bound to the viewable area: this screen has no real line-wrapping or
    // pagination yet (that's reader work, not today's scope -- see
    // PLAN.md), so a chapter longer than one page is truncated rather than
    // drawn past the bottom edge, and each verse is truncated to one line
    // rather than drawn past the right edge.
    //
    // getOrientedViewableTRBL returns bezel INSETS (margin sizes), not
    // absolute coordinates -- confirmed by reading GfxRenderer.cpp after an
    // earlier version of this code misread it as a Y coordinate and made
    // every verse fail the bound check.
    int viewTop, viewRight, viewBottomInset, viewLeft;
    renderer.getOrientedViewableTRBL(&viewTop, &viewRight, &viewBottomInset, &viewLeft);
    const int viewableBottomY = renderer.getScreenHeight() - viewBottomInset;
    const int maxLineWidth = pageWidth - x - viewRight;
    for (const auto& verse : verses) {
      if (y + lineH > viewableBottomY) break;
      std::string line = std::to_string(verse.number) + "  " + verse.text;
      renderer.drawText(UI_10_FONT_ID, x, y, renderer.truncatedText(UI_10_FONT_ID, line.c_str(), maxLineWidth).c_str(),
                        true);
      y += lineH;
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
