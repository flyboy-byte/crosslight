#include "BibleNumbers.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>

namespace bible_numbers {

namespace {
// Number files are a paragraph plus a handful of cited claims, so they stay
// small -- well under a kilobyte per number in practice. Cap the parse well
// above that but far below anything that could strain the heap.
constexpr size_t MAX_STUDY_BYTES = 32 * 1024;

bool endsWithJson(const std::string& name) {
  return name.size() > 5 && name.compare(name.size() - 5, 5, ".json") == 0;
}
}  // namespace

std::vector<std::string> availableStudies() {
  std::vector<std::string> paths;
  if (!Storage.exists(NUMBERS_DIR)) return paths;
  std::vector<std::pair<int, std::string>> numbered;
  for (const auto& entry : Storage.listFiles(NUMBERS_DIR)) {
    const std::string name(entry.c_str());
    if (!endsWithJson(name)) continue;
    // Filenames are "<n>.json" by convention; sort numerically so 6, 7, 12, 40
    // list in study order rather than lexically (which would put "12" before
    // "40" but also before "6").
    const int n = std::atoi(name.c_str());
    numbered.emplace_back(n, std::string(NUMBERS_DIR) + "/" + name);
  }
  std::sort(numbered.begin(), numbered.end());
  for (auto& pair : numbered) paths.push_back(std::move(pair.second));
  return paths;
}

bool loadStudy(const char* path, NumberStudy& out) {
  const String raw = Storage.readFile(path);
  if (raw.isEmpty()) {
    LOG_ERR("BIBLENUM", "Number study %s is missing or empty", path);
    return false;
  }
  if (raw.length() > MAX_STUDY_BYTES) {
    LOG_ERR("BIBLENUM", "Number study %s is %u bytes, over the %u cap", path, static_cast<unsigned>(raw.length()),
            static_cast<unsigned>(MAX_STUDY_BYTES));
    return false;
  }

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, raw.c_str());
  if (error) {
    LOG_ERR("BIBLENUM", "Number study %s failed to parse: %s", path, error.c_str());
    return false;
  }

  NumberStudy study;
  study.path = path;
  study.number = doc["number"] | 0;
  study.name = doc["name"] | "";
  study.summary = doc["summary"] | "";

  for (JsonVariantConst term : doc["terms"].as<JsonArrayConst>()) {
    if (term.is<const char*>()) study.terms.emplace_back(term.as<const char*>());
  }
  for (JsonVariantConst source : doc["sources"].as<JsonArrayConst>()) {
    if (source.is<const char*>()) study.sources.emplace_back(source.as<const char*>());
  }

  for (JsonObjectConst claimJson : doc["claims"].as<JsonArrayConst>()) {
    Claim claim;
    claim.classification = classificationFromString(claimJson["class"] | "");
    claim.text = claimJson["text"] | "";
    if (claim.text.empty()) {
      LOG_ERR("BIBLENUM", "Skipping claim with no text in %s", path);
      continue;
    }
    for (JsonObjectConst refJson : claimJson["refs"].as<JsonArrayConst>()) {
      Ref ref;
      ref.book = refJson["book"] | "";
      ref.chapter = refJson["chapter"] | 0;
      ref.verse = refJson["verse"] | 0;
      // A single-verse ref omits "end"; normalize so callers can always read
      // [verse, end] as the range.
      ref.end = refJson["end"] | ref.verse;
      if (ref.end < ref.verse) ref.end = ref.verse;
      if (ref.book.empty() || ref.chapter < 1 || ref.verse < 1) {
        LOG_ERR("BIBLENUM", "Skipping malformed ref in %s", path);
        continue;
      }
      claim.refs.push_back(std::move(ref));
    }
    study.claims.push_back(std::move(claim));
  }

  if (study.claims.empty()) {
    LOG_ERR("BIBLENUM", "Number study %s has no usable claims", path);
    return false;
  }
  if (study.name.empty()) study.name = std::to_string(study.number);
  out = std::move(study);
  return true;
}

}  // namespace bible_numbers
