#pragma once

#include <string>
#include <utility>
#include <vector>

#include "activities/Activity.h"
#include "bible/BibleChapterLoader.h"
#include "bible/BibleCompare.h"
#include "components/themes/BaseTheme.h"

// Cross-translation compare: pick a chapter + verse and read it in every
// translation installed on the card at once. Seeds from the current reading
// position (book + chapter) so "compare where I am" is the default. Book stays
// fixed to the reading position's book for this screen; chapter and verse are
// navigable. Reached from a Bible-hub row that is enabled only when 2+
// translations are installed.
//
// Loading reuses the reader's cached-chapter path (BibleChapterLoader cache,
// JSON fallback) per translation. The verse-selection logic is pure and
// host-tested in bible/BibleCompare.h; this activity does the SD loading, the
// chapter/verse navigation, and the paged rendering (modeled on
// BibleNumberDetailActivity).
class CompareTranslationsActivity final : public Activity {
 public:
  CompareTranslationsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool preventAutoSleep() override { return true; }

 private:
  enum class State { NeedTwo, Ready };

  struct Line {
    std::string text;
    bool bold = false;
  };

  // y-bands of the two selector rows (chapter / verse).
  Rect selectorRect(int row) const;
  int contentTop() const;

  void loadChapters();  // (re)load the current chapter from every translation
  void buildLines();    // wrap the current verse's rows into `lines`
  int pageCount() const;
  void changeChapter(int delta);
  void changeVerse(int delta);
  void turnPage(int delta);

  State state = State::NeedTwo;

  // Installed translations: short label + SD path. Parallel to `chapters`.
  std::vector<std::pair<std::string, std::string>> translations;

  std::string book;
  int chapter = 1;
  int chapterCount = 1;
  int verse = 1;

  // Per translation: label + the loaded verses of the current chapter.
  std::vector<bible_compare::LoadedChapter> chapters;

  std::vector<Line> lines;
  int linesPerPage = 1;
  int page = 0;
};
