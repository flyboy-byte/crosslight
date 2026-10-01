#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

// "Bible Numbers": a small, data-driven study tool. Each number is a file on SD
// at /Bible/numbers/<n>.json describing how that number appears in Scripture,
// with *every claim explicitly classified* so a plain textual fact is never
// blurred into a later interpretation. Verse text is pulled from whatever
// translation is installed (the same chapter cache the reader uses), so the
// files hold references, not Scripture -- nothing is baked into flash and a new
// number is a file drop rather than a firmware change.
//
//   { "number": 7, "name": "Seven", "summary": "...",
//     "terms": ["seven", "seventh"],                      // words this number uses
//     "claims": [
//        { "class": "FACT",                               // see Classification
//          "text": "...",
//          "refs": [ { "book":"Genesis","chapter":2,"verse":2,"end":3 } ] } ],
//     "sources": ["..."] }
//
// Design note worth keeping: the classification is the whole point. The firmware
// is a renderer, not an oracle -- it must never present "7 = perfection" or
// "6 = evil" as fact. The data says which layer a statement belongs to; the UI
// shows that label next to it.
namespace bible_numbers {

// How strongly a claim is grounded in the text, least to most interpretive.
// Rendered as a labelled band so the reader always sees which is which.
enum class Classification { Fact, Pattern, Tradition, Debate, Speculation };

// Parse a data file's "class" string (case-insensitive). Unknown or missing
// maps to Speculation -- the most cautious bucket, so a typo in a data file can
// only ever under-claim, never dress an opinion up as a fact.
//
// Inline and dependency-free on purpose (needs only <string>/<cctype>) so it is
// host-testable without pulling in ArduinoJson/HalStorage -- this is the one
// place a data-file typo could silently misclassify a claim, so it is pinned by
// test/bible_numbers/BibleNumbersTest.cpp.
inline Classification classificationFromString(const std::string& s) {
  std::string key = s;
  std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (key == "fact") return Classification::Fact;
  if (key == "pattern" || key == "literary_pattern" || key == "literary pattern") return Classification::Pattern;
  if (key == "tradition" || key == "traditional" || key == "traditional_interpretation" ||
      key == "interpretation")
    return Classification::Tradition;
  if (key == "debate" || key == "scholarly_debate" || key == "scholarly debate") return Classification::Debate;
  return Classification::Speculation;
}

// One cited passage. Exactly the shape BibleMemoryWork uses, so the detail
// screen loads verse text through the same chapter-cache path.
struct Ref {
  std::string book;
  int chapter = 0;
  int verse = 0;
  int end = 0;  // last verse of the range; equals `verse` for a single verse

  // "Genesis 2:2" / "Genesis 2:2-3". Inline for the same reason as
  // classificationFromString() above -- host-testable, no JSON dependency.
  std::string reference() const {
    std::string ref = book + " " + std::to_string(chapter) + ":" + std::to_string(verse);
    if (end > verse) ref += "-" + std::to_string(end);
    return ref;
  }
};

struct Claim {
  Classification classification = Classification::Speculation;
  std::string text;
  std::vector<Ref> refs;
};

struct NumberStudy {
  std::string path;
  int number = 0;
  std::string name;     // "Seven"
  std::string summary;  // one neutral paragraph of framing
  std::vector<std::string> terms;    // the words this number uses, for future counts
  std::vector<Claim> claims;
  std::vector<std::string> sources;  // free-text citations
};

// Directory scanned for number files.
constexpr const char* NUMBERS_DIR = "/Bible/numbers";

// Number files present on SD, by path, sorted by their numeric value (so 6, 7,
// 12, 40 list in order rather than lexically). Does not parse them.
std::vector<std::string> availableStudies();

// Parses one number file. False (leaving `out` untouched) if it can't be read
// or has no claims.
bool loadStudy(const char* path, NumberStudy& out);

}  // namespace bible_numbers
