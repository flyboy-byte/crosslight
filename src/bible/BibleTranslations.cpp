#include "BibleTranslations.h"

#include <HalStorage.h>

#include <algorithm>
#include <cctype>
#include <iterator>

#include "BibleReadingStateStore.h"

namespace {

constexpr BiblePreset PRESETS[] = {
    {"kjv", "King James Version", "GPL"},
    {"web", "World English Bible", "Public Domain"},
    {"asv", "American Standard Version", "Public Domain"},
    {"ylt", "Young's Literal Translation", "Public Domain"},
    {"basicenglish", "Basic English Bible", "Public Domain"},
    {"wb", "Webster's Bible", "Public Domain"},
    {"douayrheims", "Douay Rheims", "Public Domain"},
    {"akjv", "American King James Version", "Free non-commercial"},
    {"kjva", "King James Version with Strong's", "GPL"},
    {"weymouth", "Weymouth New Testament", "Public Domain"},
    {"tyndale", "Tyndale Bible (1525/1530)", "Public Domain"},
    {"wycliffe", "Wycliffe Bible (c.1395)", "CC BY-SA 4.0"},
};

constexpr char BIBLE_ROOT[] = "/Bible";
constexpr char DEFAULT_ABBR[] = "kjv";

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

std::string upper(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return s;
}

}  // namespace

namespace BibleTranslations {

const BiblePreset* presets(size_t& count) {
  count = std::size(PRESETS);
  return PRESETS;
}

const BiblePreset* findPreset(const std::string& abbr) {
  for (const auto& preset : PRESETS) {
    if (abbr == preset.abbr) return &preset;
  }
  return nullptr;
}

std::string pathFor(const std::string& abbr) {
  return std::string(BIBLE_ROOT) + "/" + upper(abbr) + "/" + lower(abbr) + ".json";
}

std::string downloadUrl(const std::string& abbr) { return "https://api.getbible.net/v2/" + lower(abbr) + ".json"; }

bool isInstalled(const std::string& abbr) { return !abbr.empty() && Storage.exists(pathFor(abbr).c_str()); }

std::vector<std::string> installed() {
  std::vector<std::string> out;
  HalFile root = Storage.open(BIBLE_ROOT);
  if (!root || !root.isDirectory()) return out;
  char name[64];
  while (true) {
    HalFile entry = root.openNextFile();
    if (!entry) break;
    const bool isDir = entry.isDirectory();
    entry.getName(name, sizeof(name));
    entry.close();
    if (!isDir || name[0] == '.') continue;
    const std::string abbr = lower(name);
    if (isInstalled(abbr)) out.push_back(abbr);
  }
  std::sort(out.begin(), out.end());
  return out;
}

std::string current() {
  const std::string& saved = BIBLE_READING_STATE.translation;
  if (isInstalled(saved)) return saved;
  if (isInstalled(DEFAULT_ABBR)) return DEFAULT_ABBR;
  const auto all = installed();
  return all.empty() ? "" : all.front();
}

void setCurrent(const std::string& abbr) {
  BIBLE_READING_STATE.translation = abbr;
  BIBLE_READING_STATE.saveToFile();
}

std::string displayName(const std::string& abbr) {
  if (const auto* preset = findPreset(abbr)) return preset->name;
  return upper(abbr);
}

std::string shortLabel(const std::string& abbr) { return upper(abbr); }

}  // namespace BibleTranslations
