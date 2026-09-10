#pragma once

#include <string>
#include <vector>

#include "activities/reader/ReaderActivity.h"
#include "bible/BibleChapterLoader.h"

// Bible reading surface, rebuilt on the shared ReaderActivity base (the same
// one EPUB/TXT/XTC subclass) instead of the raw Activity the first version
// used -- see PLAN.md. Inherits page-turn button/touch handling, the e-ink
// refresh-batching policy (pagesUntilFullRefresh), and Back/Home navigation
// for free; this class supplies the content model (verses -> wrapped lines ->
// pages) and the book/chapter picker hookup.
//
// Deliberately NOT wired into ReaderActivity's file-book plumbing:
// APP_STATE.openEpubPath / RecentBooksStore / EndOfBookOptions all assume a
// path that ReaderActivity::create() can re-dispatch by extension (EPUB/TXT/
// XTC); a getBible .json path would misdispatch to the EPUB branch. onEnter/
// onExit are overridden in full (not calling ReaderActivity::onEnter()) to
// skip that coupling. isAtEndOfBook() always returns false for the same
// reason -- Revelation's last page just stops advancing, no end-of-book menu.
//
// Reading position (book/chapter/page) is persisted via BibleReadingStateStore,
// and saved locations via BibleBookmarkStore -- both dedicated PersistableStores,
// deliberately NOT added to CrossPointState or the shared file-book bookmark
// plumbing, to keep avoiding the shared-app-state coupling described above.
//
// Confirm opens BibleMenuActivity (Select Book / Bookmarks / Toggle Bookmark)
// rather than the book picker directly: all four buttons are already bound
// (Back / Select / previous page / next page), so added features need a menu.
class BibleReaderActivity final : public ReaderActivity {
 public:
  explicit BibleReaderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;

 private:
  bool loadBook() override { return true; }  // no-op: onEnter() drives loading directly, see above
  std::string getBookTitle() const override { return "Bible"; }
  bool handleFormatInput() override;
  bool pageTurn(bool isForward) override;
  bool isAtEndOfBook() const override { return false; }
  void renderBook() override;

  void openMenu();
  void openBookPicker();
  void openChapterPicker();
  void openBookmarkList();
  void goTo(const std::string& bookName, int chapter, int page);
  void loadCurrentChapter();
  void buildPages();
  void persistPosition() const;

  // Canonical book order + chapter counts, scanned once per activity visit
  // (BibleChapterLoader::loadBookIndex) so chapter-boundary page turns can
  // cross into the next/previous book without re-scanning the file each time.
  std::vector<BibleBookInfo> books;
  int currentBookIndex = 0;
  int currentChapter = 1;

  std::vector<BibleVerse> verses;
  std::vector<std::vector<std::string>> pages;
  int currentPageIndex = 0;
  bool chapterLoaded = false;
};
