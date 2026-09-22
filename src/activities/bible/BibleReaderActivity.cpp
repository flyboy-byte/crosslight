#include "BibleReaderActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "activities/bible/BibleBookSelectionActivity.h"
#include "activities/bible/BibleBookmarkListActivity.h"
#include "activities/bible/BibleChapterSelectionActivity.h"
#include "activities/bible/BibleMenuActivity.h"
#include "activities/reader/ReaderUtils.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "bible/BibleBookmarkStore.h"
#include "bible/BibleReference.h"
#include "bible/BibleReadingStateStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// SD-first: a translation dropped at this path (getBible v2 JSON format,
// e.g. downloaded from https://api.getbible.net/v2/kjv.json) just works,
// no network needed. See PLAN.md.
constexpr const char* KJV_PATH = "/Bible/KJV/kjv.json";
// A single verse wrapping into more lines than this is not expected at this
// screen width; wrappedText truncates (with an ellipsis) past its cap rather
// than overflowing, so this is a safety ceiling, not a real limit.
constexpr int MAX_WRAP_LINES_PER_VERSE = 24;
}  // namespace

BibleReaderActivity::BibleReaderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : ReaderActivity("BibleReader", renderer, mappedInput, KJV_PATH, /*allowFastInitialRefresh=*/true) {}

void BibleReaderActivity::onEnter() {
  Activity::onEnter();

  if (!Storage.exists(KJV_PATH)) {
    LOG_ERR("BIBLE", "Translation file not found: %s", KJV_PATH);
    finish();
    return;
  }

  applyInitialOrientation();

  if (books.empty()) {
    const unsigned long start = millis();
    if (BibleChapterLoader::loadCachedBookIndex(KJV_PATH, books)) {
      LOG_INF("BIBLE", "Book index from cache: %u books in %lu ms", static_cast<unsigned>(books.size()),
              millis() - start);
    } else {
      GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
      if (!BibleChapterLoader::buildCache(KJV_PATH, books)) {
        finish();
        return;
      }
      LOG_INF("BIBLE", "Chapter cache built: %u books in %lu ms", static_cast<unsigned>(books.size()),
              millis() - start);
    }
  }

  currentBookIndex = 0;
  currentChapter = 1;
  currentPageIndex = 0;
  BIBLE_BOOKMARKS.loadFromFile();
  BIBLE_READING_STATE.loadFromFile();
  if (BIBLE_READING_STATE.hasSavedPosition()) {
    for (size_t i = 0; i < books.size(); ++i) {
      if (books[i].name == BIBLE_READING_STATE.bookName) {
        currentBookIndex = static_cast<int>(i);
        break;
      }
    }
    currentChapter = std::clamp(BIBLE_READING_STATE.chapter, 1, books[currentBookIndex].chapterCount);
    currentPageIndex = std::max(0, BIBLE_READING_STATE.page);
  }

  // loadCurrentChapter() persists whatever currentPageIndex is set to above;
  // clamp and re-persist afterward only in the (unexpected) case a saved page
  // no longer fits the freshly-built page count.
  loadCurrentChapter();
  if (currentPageIndex >= static_cast<int>(pages.size())) {
    currentPageIndex = std::max(0, static_cast<int>(pages.size()) - 1);
    persistPosition();
  }
}

void BibleReaderActivity::onExit() {
  Activity::onExit();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
}

void BibleReaderActivity::loadCurrentChapter() {
  const unsigned long start = millis();
  const BibleBookInfo& book = books[currentBookIndex];
  chapterLoaded = BibleChapterLoader::loadCachedChapter(KJV_PATH, book, currentChapter, verses) ||
                  BibleChapterLoader::loadChapter(KJV_PATH, book.name.c_str(), currentChapter, verses);
  LOG_INF("BIBLE", "Chapter %s %d: %u verses in %lu ms", books[currentBookIndex].name.c_str(), currentChapter,
          static_cast<unsigned>(verses.size()), millis() - start);
  buildPages();
  persistPosition();
  requestUpdate();
}

void BibleReaderActivity::persistPosition() const {
  if (books.empty()) return;
  BIBLE_READING_STATE.save(books[currentBookIndex].name, currentChapter, currentPageIndex);
}

bool BibleReaderActivity::isCenterColumnTap() const {
  if (!mappedInput.hasTouch() || SETTINGS.showReaderMenu != CrossPointSettings::READER_MENU_TAP) return false;
  int x = 0;
  int y = 0;
  if (!mappedInput.wasScreenTapped(x, y)) return false;
  const int zoneWidth = renderer.getScreenWidth() / 3;
  return x >= zoneWidth && x < renderer.getScreenWidth() - zoneWidth;
}

bool BibleReaderActivity::handleFormatInput() {
  // The X4 Pro has no Confirm button, so touch is its only way in. ReaderUtils' menu tap
  // only accepts the center ninth; above or below it in the center column a tap hit
  // neither the menu nor a page-turn zone, so the Bible takes the whole column.
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
      ReaderUtils::isTouchMenuGesture(renderer, mappedInput) || isCenterColumnTap()) {
    openMenu();
    return true;
  }
  return false;
}

void BibleReaderActivity::openMenu() {
  if (books.empty()) return;
  const std::string& book = books[currentBookIndex].name;
  const bool bookmarked = BIBLE_BOOKMARKS.indexOf(book, currentChapter, currentPageIndex) >= 0;

  startActivityForResult(
      std::make_unique<BibleMenuActivity>(renderer, mappedInput, bookmarked,
                                          static_cast<int>(BIBLE_BOOKMARKS.all().size())),
      [this](const ActivityResult& result) {
        if (result.isCancelled) return;
        switch (std::get<MenuResult>(result.data).action) {
          case BibleMenuActivity::GoToBook:
            openBookPicker();
            break;
          case BibleMenuActivity::GoToVerse:
            openVerseJump();
            break;
          case BibleMenuActivity::OpenBookmarks:
            openBookmarkList();
            break;
          case BibleMenuActivity::ToggleBookmark:
            // Bookmarks a page index, so it resolves back to the exact screen
            // that was on display -- not just the chapter.
            BIBLE_BOOKMARKS.toggle(books[currentBookIndex].name, currentChapter, currentPageIndex);
            requestUpdate();
            break;
          default:
            break;
        }
      });
}

void BibleReaderActivity::openBookPicker() {
  startActivityForResult(
      std::make_unique<BibleBookSelectionActivity>(renderer, mappedInput, KJV_PATH),
      [this](const ActivityResult& result) {
        if (result.isCancelled) return;
        const auto& bookResult = std::get<BibleBookResult>(result.data);
        for (size_t i = 0; i < books.size(); ++i) {
          if (books[i].name == bookResult.name) {
            currentBookIndex = static_cast<int>(i);
            break;
          }
        }
        openChapterPicker();
      });
}

void BibleReaderActivity::openChapterPicker() {
  startActivityForResult(
      std::make_unique<BibleChapterSelectionActivity>(renderer, mappedInput, books[currentBookIndex].name,
                                                       books[currentBookIndex].chapterCount, currentChapter),
      [this](const ActivityResult& result) {
        if (result.isCancelled) return;
        const auto& chapterResult = std::get<BibleChapterResult>(result.data);
        currentChapter = chapterResult.chapter;
        currentPageIndex = 0;
        loadCurrentChapter();
      });
}

// Free-text reference entry ("John 3:16"), on the same keyboard the WiFi and
// OPDS screens use. Faster than book picker -> chapter picker for a known
// reference, and the only way to land on a specific *verse* rather than a
// chapter's first page.
void BibleReaderActivity::openVerseJump() {
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_VERSE_REFERENCE), lastVerseQuery,
                                              MAX_REFERENCE_LENGTH),
      [this](const ActivityResult& result) {
        if (result.isCancelled) return;
        const std::string& input = std::get<KeyboardResult>(result.data).text;
        BibleReferenceQuery query;
        if (!parseBibleReference(input, books, query)) {
          // No toast facility exists, so an unparseable reference reopens the
          // keyboard with the text intact rather than silently doing nothing.
          // Cancel is the way out; that's why this can't loop forever.
          lastVerseQuery = input;
          openVerseJump();
          return;
        }
        lastVerseQuery.clear();
        goTo(query.book, query.chapter, 0);
        if (query.verse > 0) goToVerse(query.verse);
      });
}

// Moves to the page a verse starts on, after goTo() has loaded the chapter.
// Silently stays put when the verse doesn't exist (the book index carries
// chapter counts, not verse counts, so the parser cannot reject it earlier).
void BibleReaderActivity::goToVerse(const int verseNumber) {
  for (size_t i = 0; i < verses.size() && i < versePages.size(); ++i) {
    if (verses[i].number != verseNumber) continue;
    currentPageIndex = std::clamp(versePages[i], 0, std::max(0, static_cast<int>(pages.size()) - 1));
    persistPosition();
    requestUpdate();
    return;
  }
}

void BibleReaderActivity::openBookmarkList() {
  startActivityForResult(std::make_unique<BibleBookmarkListActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) return;
                           const auto& bookmark = std::get<BibleBookmarkResult>(result.data);
                           goTo(bookmark.book, bookmark.chapter, bookmark.page);
                         });
}

// Jumps to a saved location. The page clamp matters here in a way it doesn't
// for picker jumps (which always land on page 0): a bookmark stores a page
// index from a previous pagination, and font-size or orientation changes since
// then can shrink the chapter's page count.
void BibleReaderActivity::goTo(const std::string& bookName, const int chapter, const int page) {
  for (size_t i = 0; i < books.size(); ++i) {
    if (books[i].name == bookName) {
      currentBookIndex = static_cast<int>(i);
      break;
    }
  }
  currentChapter = std::clamp(chapter, 1, books[currentBookIndex].chapterCount);
  currentPageIndex = std::max(0, page);
  loadCurrentChapter();
  if (currentPageIndex >= static_cast<int>(pages.size())) {
    currentPageIndex = std::max(0, static_cast<int>(pages.size()) - 1);
    persistPosition();
    requestUpdate();
  }
}

// Crosses book boundaries at the ends of a chapter so paging forward/back
// through the whole Bible reads like one continuous book, matching how the
// EPUB/TXT readers page across chapters. isAtEndOfBook() always returns
// false (see header), so forward paging at Revelation's last page and
// backward paging at Genesis 1's first page both just stop rather than
// wrapping or opening an end-of-book menu.
bool BibleReaderActivity::pageTurn(const bool isForward) {
  if (isForward) {
    if (currentPageIndex + 1 < static_cast<int>(pages.size())) {
      ++currentPageIndex;
      persistPosition();
      return true;
    }
    if (currentChapter < books[currentBookIndex].chapterCount) {
      ++currentChapter;
    } else if (currentBookIndex + 1 < static_cast<int>(books.size())) {
      ++currentBookIndex;
      currentChapter = 1;
    } else {
      return false;
    }
    loadCurrentChapter();
    currentPageIndex = 0;
    return true;
  }

  if (currentPageIndex > 0) {
    --currentPageIndex;
    persistPosition();
    return true;
  }
  if (currentChapter > 1) {
    --currentChapter;
  } else if (currentBookIndex > 0) {
    --currentBookIndex;
    currentChapter = books[currentBookIndex].chapterCount;
  } else {
    return false;
  }
  loadCurrentChapter();
  currentPageIndex = std::max(0, static_cast<int>(pages.size()) - 1);
  return true;
}

// Flattens the chapter's verses into word-wrapped lines (via the shared
// wrappedText helper -- see PLAN.md's "not yet used" note, now used) and
// slices them into screen-sized pages. Runs once per chapter load, not per
// render.
void BibleReaderActivity::buildPages() {
  pages.clear();
  versePages.clear();
  if (!chapterLoaded || verses.empty()) return;

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int x = metrics.contentSidePadding;
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);

  int viewTop, viewRight, viewBottomInset, viewLeft;
  renderer.getOrientedViewableTRBL(&viewTop, &viewRight, &viewBottomInset, &viewLeft);
  // Must match renderBook()'s actual first-content-line y exactly: title row
  // (topPadding + headerHeight + lineH) plus the two-line gap render leaves
  // before content. A mismatch here caused a real bottom-edge overflow bug
  // (caught by GfxRenderer's "Outside range" log during simulator testing,
  // not by compiling) -- off by one lineH is enough for the last line of a
  // full page to bleed past the viewable area.
  const int contentTop = metrics.topPadding + metrics.headerHeight + lineH * 3;
  const int viewableBottomY = renderer.getScreenHeight() - viewBottomInset;
  const int maxLineWidth = pageWidth - x - viewRight;
  const int linesPerPage = std::max(1, (viewableBottomY - contentTop) / lineH);

  std::vector<std::string> flatLines;
  versePages.reserve(verses.size());
  for (const auto& verse : verses) {
    // Recorded before the verse's lines are appended, so this is the page the
    // verse *starts* on. Pages are fixed-size slices of flatLines, so the page
    // is just integer division -- no need to keep the slicing and this in sync.
    versePages.push_back(static_cast<int>(flatLines.size()) / linesPerPage);
    const std::string prefixed = std::to_string(verse.number) + "  " + verse.text;
    for (auto& line : renderer.wrappedText(UI_10_FONT_ID, prefixed.c_str(), maxLineWidth, MAX_WRAP_LINES_PER_VERSE)) {
      flatLines.push_back(std::move(line));
    }
  }

  for (size_t i = 0; i < flatLines.size(); i += linesPerPage) {
    const size_t end = std::min(i + static_cast<size_t>(linesPerPage), flatLines.size());
    pages.emplace_back(flatLines.begin() + static_cast<long>(i), flatLines.begin() + static_cast<long>(end));
  }
  if (pages.empty()) pages.emplace_back();
}

void BibleReaderActivity::renderBook() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int x = metrics.contentSidePadding;
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BIBLE));

  int y = metrics.topPadding + metrics.headerHeight + lineH;

  if (!chapterLoaded) {
    renderer.drawText(UI_10_FONT_ID, x, y, tr(STR_BIBLE_NOT_FOUND), true, EpdFontFamily::BOLD);
    y += lineH * 2;
    renderer.drawText(UI_10_FONT_ID, x, y, KJV_PATH, true);
  } else {
    std::string title = books[currentBookIndex].name + " " + std::to_string(currentChapter) + " (KJV)";
    if (pages.size() > 1) {
      title += "  " + std::to_string(currentPageIndex + 1) + "/" + std::to_string(pages.size());
    }
    // ASCII marker rather than a star glyph or icon bitmap: this has to render
    // identically under every bundled font and on the simulator, since it's the
    // only on-screen confirmation that a toggle took effect.
    if (BIBLE_BOOKMARKS.indexOf(books[currentBookIndex].name, currentChapter, currentPageIndex) >= 0) {
      title += "  *";
    }
    renderer.drawText(UI_10_FONT_ID, x, y, title.c_str(), true, EpdFontFamily::BOLD);
    y += lineH * 2;

    if (!pages.empty()) {
      for (const auto& line : pages[currentPageIndex]) {
        renderer.drawText(UI_10_FONT_ID, x, y, line.c_str(), true);
        y += lineH;
      }
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
