#include "BibleMemoryLessonActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <utility>

#include "MappedInputManager.h"
#include "bible/BibleChapterLoader.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Long ranges (Matthew 5:38-44 is the worst in the Prep Class booklet) still
// wrap to well under this; the cap only stops a malformed range from running
// away with the page.
constexpr int MAX_WRAPPED_LINES = 64;
}  // namespace

BibleMemoryLessonActivity::BibleMemoryLessonActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                     bible_memory::Lesson lesson, std::string translationPath)
    : Activity("BibleMemoryLesson", renderer, mappedInput),
      lesson(std::move(lesson)),
      translationPath(std::move(translationPath)) {}

void BibleMemoryLessonActivity::onEnter() {
  Activity::onEnter();
  {
    RenderLock lock(*this);
    GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
  }
  buildLines();
  ready = true;
  requestUpdate();
}

// Verse text comes from the chapter cache, the same path the reader uses -- a
// few ms per chapter. Chapters are loaded once each and reused across the
// lesson's verses, since a lesson often quotes one chapter more than once.
void BibleMemoryLessonActivity::buildLines() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int textWidth = renderer.getScreenWidth() - metrics.contentSidePadding * 2;

  std::vector<BibleBookInfo> books;
  if (!BibleChapterLoader::loadCachedBookIndex(translationPath.c_str(), books)) {
    // No usable cache (first run, or the translation changed). Building it is
    // the same one-time cost the reader pays, and every later lesson is fast.
    BibleChapterLoader::buildCache(translationPath.c_str(), books);
  }

  const auto addWrapped = [&](const std::string& text, const bool bold) {
    for (auto& wrapped : renderer.wrappedText(UI_10_FONT_ID, text.c_str(), textWidth, MAX_WRAPPED_LINES,
                                              bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR)) {
      lines.push_back(Line{std::move(wrapped), bold});
    }
  };

  std::string loadedBook;
  int loadedChapter = 0;
  std::vector<BibleVerse> verses;

  for (const auto& item : lesson.items) {
    if (!lines.empty()) lines.push_back(Line{"", false});

    if (item.kind != bible_memory::Item::Kind::Verse) {
      addWrapped(item.text, item.kind == bible_memory::Item::Kind::Heading);
      continue;
    }

    addWrapped(item.reference(), true);

    if (item.book != loadedBook || item.chapter != loadedChapter) {
      verses.clear();
      const auto book = std::find_if(books.begin(), books.end(),
                                     [&](const BibleBookInfo& candidate) { return candidate.name == item.book; });
      if (book != books.end()) {
        if (!BibleChapterLoader::loadCachedChapter(translationPath.c_str(), *book, item.chapter, verses)) {
          BibleChapterLoader::loadChapter(translationPath.c_str(), item.book.c_str(), item.chapter, verses);
        }
      } else {
        LOG_ERR("BIBLEMEM", "%s is not in this translation", item.book.c_str());
      }
      loadedBook = item.book;
      loadedChapter = item.chapter;
    }

    // Verse numbers are dropped for a single verse (the reference above already
    // says which one) but kept for a range, so a multi-verse passage stays
    // readable.
    std::string passage;
    for (const auto& verse : verses) {
      if (verse.number < item.verse || verse.number > item.end) continue;
      if (!passage.empty()) passage += " ";
      if (item.end > item.verse) passage += std::to_string(verse.number) + " ";
      passage += verse.text;
    }
    addWrapped(passage.empty() ? tr(STR_MEMORY_VERSE_MISSING) : passage, false);
  }
}

int BibleMemoryLessonActivity::pageCount() const {
  if (lines.empty() || linesPerPage < 1) return 1;
  return (static_cast<int>(lines.size()) + linesPerPage - 1) / linesPerPage;
}

void BibleMemoryLessonActivity::turnPage(const int delta) {
  const int target = page + delta;
  if (target < 0 || target >= pageCount()) return;
  RenderLock lock(*this);
  page = target;
  requestUpdate();
}

void BibleMemoryLessonActivity::loop() {
  if (!ready) return;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::PageForward) ||
      mappedInput.wasReleased(MappedInputManager::Button::Down) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    turnPage(1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::PageBack) ||
      mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    turnPage(-1);
    return;
  }
  int x = 0;
  int y = 0;
  if (mappedInput.wasScreenTapped(x, y)) {
    // Same split as the reader: the left third goes back, everything else
    // forward.
    turnPage(x < renderer.getScreenWidth() / 3 ? -1 : 1);
  }
}

void BibleMemoryLessonActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int top = safe.y + metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int bottom = safe.y + safe.height;

  // Touch boards draw no button hints, so the hint row's page counter never
  // appears there. Reserve a line for one of our own instead.
  linesPerPage = (bottom - top - lineH) / lineH;
  if (linesPerPage < 1) linesPerPage = 1;
  if (page >= pageCount()) page = pageCount() - 1;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, lesson.title.c_str());

  int y = top;
  for (int i = page * linesPerPage; i < static_cast<int>(lines.size()) && i < (page + 1) * linesPerPage; ++i) {
    const Line& line = lines[i];
    if (!line.text.empty()) {
      renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, y, line.text.c_str(), true,
                        line.bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
    }
    y += lineH;
  }

  const std::string position = std::to_string(page + 1) + " / " + std::to_string(pageCount());
  renderer.drawCenteredText(UI_10_FONT_ID, bottom - lineH, position.c_str());
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), position.c_str(), tr(STR_PREV_PAGE), tr(STR_NEXT_PAGE));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
