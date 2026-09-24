#pragma once

#include <string>
#include <vector>

#include "activities/UiListActivity.h"
#include "bible/BibleMemoryWork.h"

// Lesson list for a memory-work course: one row per lesson, with its verse
// count. Opening a row reads that lesson's verses.
//
// With several courses on the card this lists the courses first, then that
// course's lessons, so a second booklet is a file drop and nothing else.
class BibleMemoryWorkActivity final : public UiListActivity {
 public:
  BibleMemoryWorkActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;

 private:
  int listCount() const override { return static_cast<int>(rowItems.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void onBackButton() override;

  // Course list -> lesson list. Only reached when more than one course exists.
  void openCourse(const std::string& path);
  void buildCourseRows();
  void buildLessonRows();

  std::vector<std::string> coursePaths;
  bible_memory::Course course;
  bool showingCourses = false;
  std::string translationPath;
  std::string header;
  std::vector<std::string> labels;
  std::vector<std::string> subtitles;
  std::vector<freeink::ui::ListItem> rowItems;
};
