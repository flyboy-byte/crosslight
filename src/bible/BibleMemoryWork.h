#pragma once

#include <string>
#include <vector>

// Memory-work courses: a printed assignment booklet (lesson N -> the verses to
// memorize) turned into a browsable part of the Bible app.
//
// The booklets list *references*, not text, so that is all we store: a course
// file is a few KB on SD and the words are pulled from whatever translation is
// installed, through the same chapter cache the reader uses. Nothing is baked
// into flash, and a new course is a file drop rather than a firmware change.
//
// Courses live at /Bible/memory/<name>.json:
//   { "title": "...", "subtitle": "...",
//     "lessons": [ { "title": "Lesson 1", "items": [
//        { "heading": "Conversion" },                       // section label
//        { "book": "John", "chapter": 3, "verse": 3 },      // single verse
//        { "book": "Acts", "chapter": 2, "verse": 38, "end": 39 },  // range
//        { "note": "Memorize the books of the Bible." } ] } ] }    // free text
namespace bible_memory {

// One line of a lesson. Exactly one of the three shapes is populated, matching
// the three item kinds above.
struct Item {
  enum class Kind { Verse, Heading, Note };

  Kind kind = Kind::Verse;
  std::string text;  // heading or note text; empty for a verse
  std::string book;
  int chapter = 0;
  int verse = 0;
  int end = 0;  // last verse of the range; equals `verse` for a single verse

  // "John 3:16" / "Acts 2:38-39".
  std::string reference() const;
};

struct Lesson {
  std::string title;
  std::vector<Item> items;

  // Verse items only -- headings and notes are not counted.
  int verseCount() const;
};

struct Course {
  std::string path;
  std::string title;
  std::string subtitle;
  std::vector<Lesson> lessons;
};

// Directory scanned for course files.
constexpr const char* COURSE_DIR = "/Bible/memory";

// Course files present on SD, by path, in directory order. Does not parse them.
std::vector<std::string> availableCourses();

// Parses one course file. False (leaving `out` untouched) if it can't be read
// or has no lessons.
bool loadCourse(const char* path, Course& out);

}  // namespace bible_memory
