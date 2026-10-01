#include "BibleNumberDetailActivity.h"

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
// A number's claim list is short by design (a handful per number), so this is
// generous headroom, not a tuned limit -- it only stops a malformed file from
// running away with the page.
constexpr int MAX_WRAPPED_LINES = 64;

const char* classificationLabel(const bible_numbers::Classification c) {
  switch (c) {
    case bible_numbers::Classification::Fact:
      return tr(STR_CLASS_FACT);
    case bible_numbers::Classification::Pattern:
      return tr(STR_CLASS_PATTERN);
    case bible_numbers::Classification::Tradition:
      return tr(STR_CLASS_TRADITION);
    case bible_numbers::Classification::Debate:
      return tr(STR_CLASS_DEBATE);
    case bible_numbers::Classification::Speculation:
    default:
      return tr(STR_CLASS_SPECULATION);
  }
}
}  // namespace

BibleNumberDetailActivity::BibleNumberDetailActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                     std::string studyPath, std::string translationPath)
    : Activity("BibleNumberDetail", renderer, mappedInput),
      studyPath(std::move(studyPath)),
      translationPath(std::move(translationPath)) {}

void BibleNumberDetailActivity::onEnter() {
  Activity::onEnter();
  {
    RenderLock lock(*this);
    GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
  }
  if (bible_numbers::loadStudy(studyPath.c_str(), study)) buildLines();
  ready = true;
  requestUpdate();
}

// Cited verses come from the chapter cache, the same path the reader and
// Memory Work use -- a few ms per chapter, each chapter loaded once and reused
// across the claims that cite it.
void BibleNumberDetailActivity::buildLines() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int textWidth = renderer.getScreenWidth() - metrics.contentSidePadding * 2;

  std::vector<BibleBookInfo> books;
  if (!BibleChapterLoader::loadCachedBookIndex(translationPath.c_str(), books)) {
    BibleChapterLoader::buildCache(translationPath.c_str(), books);
  }

  const auto addWrapped = [&](const std::string& text, const bool bold) {
    for (auto& wrapped : renderer.wrappedText(UI_10_FONT_ID, text.c_str(), textWidth, MAX_WRAPPED_LINES,
                                              bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR)) {
      lines.push_back(Line{std::move(wrapped), bold});
    }
  };

  if (!study.summary.empty()) {
    addWrapped(study.summary, false);
    lines.push_back(Line{"", false});
  }

  std::string loadedBook;
  int loadedChapter = 0;
  std::vector<BibleVerse> verses;

  for (const auto& claim : study.claims) {
    addWrapped(classificationLabel(claim.classification), true);
    addWrapped(claim.text, false);

    for (const auto& ref : claim.refs) {
      addWrapped(ref.reference(), true);

      if (ref.book != loadedBook || ref.chapter != loadedChapter) {
        verses.clear();
        const auto book = std::find_if(books.begin(), books.end(),
                                       [&](const BibleBookInfo& candidate) { return candidate.name == ref.book; });
        if (book != books.end()) {
          if (!BibleChapterLoader::loadCachedChapter(translationPath.c_str(), *book, ref.chapter, verses)) {
            BibleChapterLoader::loadChapter(translationPath.c_str(), ref.book.c_str(), ref.chapter, verses);
          }
        } else {
          LOG_ERR("BIBLENUM", "%s is not in this translation", ref.book.c_str());
        }
        loadedBook = ref.book;
        loadedChapter = ref.chapter;
      }

      std::string passage;
      for (const auto& verse : verses) {
        if (verse.number < ref.verse || verse.number > ref.end) continue;
        if (!passage.empty()) passage += " ";
        if (ref.end > ref.verse) passage += std::to_string(verse.number) + " ";
        passage += verse.text;
      }
      addWrapped(passage.empty() ? tr(STR_MEMORY_VERSE_MISSING) : passage, false);
    }
    lines.push_back(Line{"", false});
  }

  if (!study.sources.empty()) {
    addWrapped(tr(STR_SOURCES), true);
    for (const auto& source : study.sources) addWrapped(source, false);
  }
}

int BibleNumberDetailActivity::pageCount() const {
  if (lines.empty() || linesPerPage < 1) return 1;
  return (static_cast<int>(lines.size()) + linesPerPage - 1) / linesPerPage;
}

void BibleNumberDetailActivity::turnPage(const int delta) {
  const int target = page + delta;
  if (target < 0 || target >= pageCount()) return;
  RenderLock lock(*this);
  page = target;
  requestUpdate();
}

void BibleNumberDetailActivity::loop() {
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
    turnPage(x < renderer.getScreenWidth() / 3 ? -1 : 1);
  }
}

void BibleNumberDetailActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int top = safe.y + metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int bottom = safe.y + safe.height;

  linesPerPage = (bottom - top - lineH) / lineH;
  if (linesPerPage < 1) linesPerPage = 1;
  if (page >= pageCount()) page = pageCount() - 1;

  renderer.clearScreen();
  const std::string title = study.name.empty() ? tr(STR_BIBLE_NUMBERS) : study.name;
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, title.c_str());

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
