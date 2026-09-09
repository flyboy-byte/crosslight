#include "BibleActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void BibleActivity::onEnter() {
  Activity::onEnter();
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
  renderer.drawText(UI_10_FONT_ID, x, y, "Genesis 1 (KJV)", true, EpdFontFamily::BOLD);
  y += lineH * 2;

  // Placeholder scripture data (pre-wrapped to fit): stands in until the
  // translation loader + byte-offset index exist. Not a localizable UI string.
  static const char* const kLines[] = {
      "1  In the beginning God created",
      "   the heaven and the earth.",
      "2  And the earth was without form,",
      "   and void; and darkness was upon",
      "   the face of the deep.",
      "3  And God said, Let there be light:",
      "   and there was light.",
  };
  for (const char* line : kLines) {
    renderer.drawText(UI_10_FONT_ID, x, y, line, true);
    y += lineH;
  }

  y += lineH;
  renderer.drawText(UI_10_FONT_ID, x, y, tr(STR_BIBLE_PLACEHOLDER), true, EpdFontFamily::ITALIC);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
