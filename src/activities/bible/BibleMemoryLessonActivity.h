#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "bible/BibleMemoryWork.h"

// One lesson's memory work as a paged reading page: each verse's reference
// followed by its text, pulled from the installed translation.
//
// Deliberately not a UiListActivity -- list rows are single-line, and the point
// of this screen is reading the passages themselves, so it lays the lesson out
// as wrapped body text and pages through it like the reader does.
class BibleMemoryLessonActivity final : public Activity {
 public:
  BibleMemoryLessonActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bible_memory::Lesson lesson,
                            std::string translationPath);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&& lock) override;

 private:
  // A laid-out line of the page. Reference lines and headings are bold; blank
  // lines are the spacing between entries.
  struct Line {
    std::string text;
    bool bold = false;
  };

  // Pulls each verse's text from the translation and word-wraps the lesson into
  // `lines`. Runs once, on entry.
  void buildLines();
  int pageCount() const;
  void turnPage(int delta);

  bible_memory::Lesson lesson;
  std::string translationPath;
  std::vector<Line> lines;
  int linesPerPage = 1;
  int page = 0;
  bool ready = false;
};
