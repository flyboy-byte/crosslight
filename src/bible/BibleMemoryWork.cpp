#include "BibleMemoryWork.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

namespace bible_memory {

namespace {
// Course files are reference lists, not text, so they stay small: the Prep
// Class booklet's 40 verses across 9 lessons is under 3KB. Cap the parse well
// above that but far below anything that could strain the heap.
constexpr size_t MAX_COURSE_BYTES = 64 * 1024;

bool endsWithJson(const std::string& name) {
  return name.size() > 5 && name.compare(name.size() - 5, 5, ".json") == 0;
}
}  // namespace

std::string Item::reference() const {
  if (kind != Kind::Verse) return text;
  std::string ref = book + " " + std::to_string(chapter) + ":" + std::to_string(verse);
  if (end > verse) ref += "-" + std::to_string(end);
  return ref;
}

int Lesson::verseCount() const {
  int n = 0;
  for (const auto& item : items) {
    if (item.kind == Item::Kind::Verse) n++;
  }
  return n;
}

std::vector<std::string> availableCourses() {
  std::vector<std::string> paths;
  if (!Storage.exists(COURSE_DIR)) return paths;
  for (const auto& entry : Storage.listFiles(COURSE_DIR)) {
    const std::string name(entry.c_str());
    if (endsWithJson(name)) paths.push_back(std::string(COURSE_DIR) + "/" + name);
  }
  return paths;
}

bool loadCourse(const char* path, Course& out) {
  const String raw = Storage.readFile(path);
  if (raw.isEmpty()) {
    LOG_ERR("BIBLEMEM", "Course %s is missing or empty", path);
    return false;
  }
  if (raw.length() > MAX_COURSE_BYTES) {
    LOG_ERR("BIBLEMEM", "Course %s is %u bytes, over the %u cap", path, static_cast<unsigned>(raw.length()),
            static_cast<unsigned>(MAX_COURSE_BYTES));
    return false;
  }

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, raw.c_str());
  if (error) {
    LOG_ERR("BIBLEMEM", "Course %s failed to parse: %s", path, error.c_str());
    return false;
  }

  Course course;
  course.path = path;
  course.title = doc["title"] | "";
  course.subtitle = doc["subtitle"] | "";

  for (JsonObjectConst lessonJson : doc["lessons"].as<JsonArrayConst>()) {
    Lesson lesson;
    lesson.title = lessonJson["title"] | "";
    for (JsonObjectConst itemJson : lessonJson["items"].as<JsonArrayConst>()) {
      Item item;
      if (itemJson["heading"].is<const char*>()) {
        item.kind = Item::Kind::Heading;
        item.text = itemJson["heading"].as<const char*>();
      } else if (itemJson["note"].is<const char*>()) {
        item.kind = Item::Kind::Note;
        item.text = itemJson["note"].as<const char*>();
      } else {
        item.kind = Item::Kind::Verse;
        item.book = itemJson["book"] | "";
        item.chapter = itemJson["chapter"] | 0;
        item.verse = itemJson["verse"] | 0;
        // A single-verse item omits "end"; normalize so callers can always read
        // [verse, end] as the range.
        item.end = itemJson["end"] | item.verse;
        if (item.end < item.verse) item.end = item.verse;
        if (item.book.empty() || item.chapter < 1 || item.verse < 1) {
          LOG_ERR("BIBLEMEM", "Skipping malformed item in %s", path);
          continue;
        }
      }
      lesson.items.push_back(std::move(item));
    }
    if (!lesson.items.empty()) course.lessons.push_back(std::move(lesson));
  }

  if (course.lessons.empty()) {
    LOG_ERR("BIBLEMEM", "Course %s has no lessons", path);
    return false;
  }
  if (course.title.empty()) course.title = "Memory Work";
  out = std::move(course);
  return true;
}

}  // namespace bible_memory
