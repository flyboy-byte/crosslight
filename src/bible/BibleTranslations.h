#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Which Bible translations exist on the SD card, which one is selected, and the
// English presets that can be downloaded from getBible.
//
// A translation `abbr` (getBible's lowercase abbreviation, e.g. "kjv") lives at
// /Bible/<ABBR>/<abbr>.json — the layout the KJV file already used.
struct BiblePreset {
  const char* abbr;
  const char* name;
  const char* license;
};

namespace BibleTranslations {

// The English entries of getBible's catalog (api.getbible.net/v2/translations.json,
// read 2026-09-21). Compiled in so the device never has to fetch and parse the 183KB
// catalog; all share the loader's JSON layout (verified against web.json/weymouth.json).
const BiblePreset* presets(size_t& count);
const BiblePreset* findPreset(const std::string& abbr);

std::string pathFor(const std::string& abbr);
std::string downloadUrl(const std::string& abbr);
bool isInstalled(const std::string& abbr);

// Abbreviations with a translation file on the card, from /Bible/<DIR>/<dir>.json.
std::vector<std::string> installed();

// The saved choice if it is still on the card, else KJV if present, else the first
// installed translation, else "".
std::string current();
void setCurrent(const std::string& abbr);

// Preset name, or the abbreviation upper-cased for a translation not in the list.
std::string displayName(const std::string& abbr);
std::string shortLabel(const std::string& abbr);  // "KJV"

}  // namespace BibleTranslations
