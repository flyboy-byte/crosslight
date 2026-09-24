#include "BibleMemoryWorkActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include "MappedInputManager.h"
#include "activities/bible/BibleMemoryLessonActivity.h"
#include "bible/BibleTranslations.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

BibleMemoryWorkActivity::BibleMemoryWorkActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("BibleMemoryWork", renderer, mappedInput) {}

void BibleMemoryWorkActivity::onEnter() {
  UiListActivity::onEnter();
  translationPath = BibleTranslations::pathFor(BibleTranslations::current());
  coursePaths = bible_memory::availableCourses();

  // One course is the common case (the booklet you were handed), so skip
  // straight to its lessons rather than making you pick from a list of one.
  if (coursePaths.size() == 1 && bible_memory::loadCourse(coursePaths.front().c_str(), course)) {
    buildLessonRows();
    return;
  }
  showingCourses = true;
  buildCourseRows();
}

void BibleMemoryWorkActivity::buildCourseRows() {
  header = tr(STR_MEMORY_WORK);
  labels.clear();
  subtitles.clear();
  rowItems.clear();
  labels.reserve(coursePaths.size());
  subtitles.reserve(coursePaths.size());
  rowItems.reserve(coursePaths.size());

  for (const auto& path : coursePaths) {
    bible_memory::Course peek;
    if (!bible_memory::loadCourse(path.c_str(), peek)) continue;
    // ListItem holds bare const char* into these, so they must not reallocate.
    labels.push_back(peek.title);
    subtitles.push_back(peek.subtitle);
    fui::ListItem item;
    item.label = labels.back().c_str();
    if (!subtitles.back().empty()) item.subtitle = subtitles.back().c_str();
    item.actionValue = static_cast<int16_t>(rowItems.size());
    rowItems.push_back(item);
  }
}

void BibleMemoryWorkActivity::buildLessonRows() {
  showingCourses = false;
  header = course.title;
  labels.clear();
  subtitles.clear();
  rowItems.clear();
  labels.reserve(course.lessons.size());
  subtitles.reserve(course.lessons.size());
  rowItems.reserve(course.lessons.size());

  for (const auto& lesson : course.lessons) {
    labels.push_back(lesson.title);
    const int verses = lesson.verseCount();
    subtitles.push_back(std::to_string(verses) + " " + (verses == 1 ? tr(STR_VERSE) : tr(STR_VERSES)));
    fui::ListItem item;
    item.label = labels.back().c_str();
    item.subtitle = subtitles.back().c_str();
    item.actionValue = static_cast<int16_t>(rowItems.size());
    rowItems.push_back(item);
  }
  nav.selected = 0;
  nav.top = 0;
}

void BibleMemoryWorkActivity::openCourse(const std::string& path) {
  if (!bible_memory::loadCourse(path.c_str(), course)) return;
  RenderLock lock(*this);
  buildLessonRows();
  requestUpdate();
}

void BibleMemoryWorkActivity::activateIndex(const int index) {
  if (index < 0 || index >= static_cast<int>(rowItems.size())) return;
  app.clearTapFlash();
  nav.selected = index;

  if (showingCourses) {
    openCourse(coursePaths[index]);
    return;
  }
  startActivityForResult(std::make_unique<BibleMemoryLessonActivity>(renderer, mappedInput, course.lessons[index],
                                                                     translationPath),
                         [](const ActivityResult&) {});
}

// Back steps course list <- lesson list before leaving, so a multi-course card
// can get back to the picker.
void BibleMemoryWorkActivity::onBackButton() {
  if (!showingCourses && coursePaths.size() > 1) {
    RenderLock lock(*this);
    showingCourses = true;
    buildCourseRows();
    nav.selected = 0;
    nav.top = 0;
    requestUpdate();
    return;
  }
  UiListActivity::onBackButton();
}

const char* BibleMemoryWorkActivity::headerTitle() const { return header.c_str(); }

void BibleMemoryWorkActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (rowItems.empty()) {
    screen.centeredText(tr(STR_NO_MEMORY_WORK), screen.theme().bodyText);
    return;
  }

  fui::ListProps props;
  props.items = rowItems.data();
  props.count = static_cast<uint16_t>(rowItems.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // physical buttons stay in loop()
  syncListViewport(screen, props);
  screen.list(props);
}
