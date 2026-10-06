#include "CompareTranslationsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "bible/BibleReadingStateStore.h"
#include "bible/BibleTranslations.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int MAX_WRAPPED_LINES = 40;  // per verse per translation; generous, longest verse is ~8 lines
constexpr unsigned long LONG_PRESS_MS = 1000;
}  // namespace

CompareTranslationsActivity::CompareTranslationsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("CompareTranslations", renderer, mappedInput) {}

void CompareTranslationsActivity::onEnter() {
  Activity::onEnter();

  for (const auto& abbr : BibleTranslations::installed()) {
    translations.emplace_back(BibleTranslations::shortLabel(abbr), BibleTranslations::pathFor(abbr));
  }
  if (translations.size() < 2) {
    state = State::NeedTwo;
    requestUpdate();
    return;
  }
  state = State::Ready;

  // Seed from where the reader left off; fall back to Genesis 1.
  BIBLE_READING_STATE.loadFromFile();
  book = BIBLE_READING_STATE.hasSavedPosition() ? BIBLE_READING_STATE.bookName : "Genesis";
  chapter = BIBLE_READING_STATE.hasSavedPosition() ? std::max(1, BIBLE_READING_STATE.chapter) : 1;
  verse = 1;

  loadChapters();
  buildLines();
  requestUpdate();
}

void CompareTranslationsActivity::loadChapters() {
  chapters.clear();
  chapterCount = 1;
  bool builtAny = false;

  for (const auto& [label, path] : translations) {
    std::vector<BibleBookInfo> books;
    if (!BibleChapterLoader::loadCachedBookIndex(path.c_str(), books)) {
      if (!builtAny) {
        GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));  // first cold translation; one popup is enough
        builtAny = true;
      }
      BibleChapterLoader::buildCache(path.c_str(), books);
    }

    std::vector<BibleVerse> verses;
    const auto it = std::find_if(books.begin(), books.end(), [&](const BibleBookInfo& b) { return b.name == book; });
    if (it != books.end()) {
      chapterCount = std::max(chapterCount, it->chapterCount);
      if (!BibleChapterLoader::loadCachedChapter(path.c_str(), *it, chapter, verses)) {
        BibleChapterLoader::loadChapter(path.c_str(), book.c_str(), chapter, verses);
      }
    }
    chapters.emplace_back(label, std::move(verses));
  }

  chapter = std::clamp(chapter, 1, chapterCount);
  const int maxV = std::max(1, bible_compare::maxVerse(chapters));
  verse = std::clamp(verse, 1, maxV);
}

void CompareTranslationsActivity::buildLines() {
  lines.clear();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int textWidth = renderer.getScreenWidth() - metrics.contentSidePadding * 2;

  const auto rows = bible_compare::rowsForVerse(chapters, verse);
  for (const auto& row : rows) {
    lines.push_back(Line{row.label, true});
    const std::string& text = row.present ? row.text : std::string(tr(STR_COMPARE_MISSING));
    for (auto& wrapped :
         renderer.wrappedText(UI_10_FONT_ID, text.c_str(), textWidth, MAX_WRAPPED_LINES, EpdFontFamily::REGULAR)) {
      lines.push_back(Line{std::move(wrapped), false});
    }
    lines.push_back(Line{"", false});
  }

  const int avail = UITheme::getInstance().getScreenSafeArea(renderer, true, false).height +
                    UITheme::getInstance().getScreenSafeArea(renderer, true, false).y - contentTop() -
                    renderer.getLineHeight(UI_10_FONT_ID);  // leave room for the page footer
  linesPerPage = std::max(1, avail / renderer.getLineHeight(UI_10_FONT_ID));
  page = std::clamp(page, 0, std::max(0, pageCount() - 1));
}

int CompareTranslationsActivity::pageCount() const {
  if (lines.empty()) return 1;
  return (static_cast<int>(lines.size()) + linesPerPage - 1) / linesPerPage;
}

int CompareTranslationsActivity::contentTop() const {
  const Rect verseRow = selectorRect(1);
  return verseRow.y + verseRow.height + UITheme::getInstance().getMetrics().verticalSpacing;
}

Rect CompareTranslationsActivity::selectorRect(const int row) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  // header, then the book name line, then the two selector rows.
  const int top =
      metrics.topPadding + metrics.headerHeight + renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing;
  const int rowH = renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing;
  return Rect{0, top + row * rowH, renderer.getScreenWidth(), rowH};
}

void CompareTranslationsActivity::changeChapter(const int delta) {
  const int next = std::clamp(chapter + delta, 1, chapterCount);
  if (next == chapter) return;
  chapter = next;
  // Keep the current verse number rather than resetting to 1 -- loadChapters()
  // clamps it to the new chapter's max below, so this only matters when the new
  // chapter is at least as long (the common case: skimming a long chapter one at
  // a time shouldn't lose your place).
  page = 0;
  loadChapters();
  buildLines();
  requestUpdate();
}

void CompareTranslationsActivity::changeVerse(const int delta) {
  const int maxV = std::max(1, bible_compare::maxVerse(chapters));
  const int next = std::clamp(verse + delta, 1, maxV);
  if (next == verse) return;
  verse = next;
  page = 0;
  buildLines();
  requestUpdate();
}

void CompareTranslationsActivity::turnPage(const int delta) {
  const int count = pageCount();
  const int next = (page + delta % count + count) % count;
  if (next == page) return;
  page = next;
  requestUpdate();
}

void CompareTranslationsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (state != State::Ready) return;

  // Physical buttons: left/right = verse. The page-turn buttons (PageBack/PageForward --
  // same side buttons every other reader screen uses to turn pages, and honoring the
  // user's side-button-layout/swap setting) page through the comparison text; a long
  // press on either one changes chapter instead, so chapter stays reachable without
  // stealing the page-turn gesture everyone expects on this hardware.
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    RenderLock lock(*this);
    changeVerse(-1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    RenderLock lock(*this);
    changeVerse(1);
    return;
  }
  if (mappedInput.wasLongPressed(MappedInputManager::Button::PageBack, LONG_PRESS_MS)) {
    RenderLock lock(*this);
    changeChapter(-1);
    return;
  }
  if (mappedInput.wasLongPressed(MappedInputManager::Button::PageForward, LONG_PRESS_MS)) {
    RenderLock lock(*this);
    changeChapter(1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::PageBack)) {
    RenderLock lock(*this);
    turnPage(-1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::PageForward)) {
    RenderLock lock(*this);
    turnPage(1);
    return;
  }

  int x = 0;
  int y = 0;
  if (!mappedInput.wasScreenTapped(x, y)) return;
  const bool leftThird = x < renderer.getScreenWidth() / 3;

  const Rect chapterRow = selectorRect(0);
  if (y >= chapterRow.y && y < chapterRow.y + chapterRow.height) {
    RenderLock lock(*this);
    changeChapter(leftThird ? -1 : 1);
    return;
  }
  const Rect verseRow = selectorRect(1);
  if (y >= verseRow.y && y < verseRow.y + verseRow.height) {
    RenderLock lock(*this);
    changeVerse(leftThird ? -1 : 1);
    return;
  }
  // Tap in the content area pages through a long verse, if there's more than one page.
  if (y >= contentTop() && pageCount() > 1) {
    RenderLock lock(*this);
    turnPage(1);
  }
}

void CompareTranslationsActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pad = metrics.contentSidePadding;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_COMPARE_TRANSLATIONS));

  if (state == State::NeedTwo) {
    renderer.drawCenteredText(UI_10_FONT_ID,
                              metrics.topPadding + metrics.headerHeight + renderer.getLineHeight(UI_10_FONT_ID) * 3,
                              tr(STR_COMPARE_NEED_TWO));
    renderer.displayBuffer();
    return;
  }

  // Book name line.
  const int bookY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing / 2;
  renderer.drawText(UI_12_FONT_ID, pad, bookY, book.c_str(), true, EpdFontFamily::BOLD);

  // Chapter / verse selector rows.
  const char* prefix[2] = {tr(STR_COMPARE_CHAPTER), tr(STR_COMPARE_VERSE)};
  const int value[2] = {chapter, verse};
  for (int r = 0; r < 2; ++r) {
    const Rect row = selectorRect(r);
    renderer.drawLine(0, row.y, pageWidth, row.y);
    const int ty = row.y + (row.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
    char label[32];
    snprintf(label, sizeof(label), "<  %s %d  >", prefix[r], value[r]);
    const int lw = renderer.getTextWidth(UI_12_FONT_ID, label);
    renderer.drawText(UI_12_FONT_ID, (pageWidth - lw) / 2, ty, label);
  }

  // Comparison content (paged).
  int y = contentTop();
  renderer.drawLine(0, y - metrics.verticalSpacing / 2, pageWidth, y - metrics.verticalSpacing / 2);
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  const int start = page * linesPerPage;
  const int end = std::min(static_cast<int>(lines.size()), start + linesPerPage);
  for (int i = start; i < end; ++i) {
    renderer.drawText(UI_10_FONT_ID, pad, y, lines[i].text.c_str(), true,
                      lines[i].bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
    y += lineH;
  }

  if (pageCount() > 1) {
    char foot[24];
    snprintf(foot, sizeof(foot), "%d / %d", page + 1, pageCount());
    renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight() - lineH * 2, foot);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_COMPARE_HOLD_CHAPTER), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
