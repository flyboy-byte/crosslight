#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "bible/BibleNumbers.h"

// One number's study material as a paged reading page: summary, then each
// claim with its classification label, text, and cited verses (text pulled
// from the installed translation), then sources.
//
// Modeled directly on BibleMemoryLessonActivity -- same reasons apply: this is
// reading, not a list, so it lays out as wrapped body text and pages through it
// like the reader does.
class BibleNumberDetailActivity final : public Activity {
 public:
  BibleNumberDetailActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string studyPath,
                            std::string translationPath);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&& lock) override;

 private:
  // A laid-out line of the page. Headings/classification labels are bold;
  // blank lines are the spacing between entries.
  struct Line {
    std::string text;
    bool bold = false;
  };

  // Pulls each cited verse's text from the translation and word-wraps the
  // whole study into `lines`. Runs once, on entry.
  void buildLines();
  int pageCount() const;
  void turnPage(int delta);

  std::string studyPath;
  std::string translationPath;
  bible_numbers::NumberStudy study;
  std::vector<Line> lines;
  int linesPerPage = 1;
  int page = 0;
  bool ready = false;
};
