#include "BibleReference.h"

#include <cctype>

namespace {

// Lowercase, keeping only [a-z0-9:] so spacing and punctuation stop mattering.
std::string normalize(const std::string& raw) {
  std::string out;
  out.reserve(raw.size());
  for (const unsigned char c : raw) {
    if (std::isalnum(c)) {
      out.push_back(static_cast<char>(std::tolower(c)));
    } else if (c == ':') {
      out.push_back(':');
    }
  }
  return out;
}

bool parseInt(const std::string& s, int& out) {
  if (s.empty() || s.size() > 3) return false;
  int value = 0;
  for (const char c : s) {
    if (c < '0' || c > '9') return false;
    value = value * 10 + (c - '0');
  }
  out = value;
  return true;
}

}  // namespace

bool parseBibleReference(const std::string& input, const std::vector<BibleBookInfo>& books,
                         BibleReferenceQuery& out) {
  const std::string text = normalize(input);
  if (text.empty()) return false;

  // Walk back over the trailing [0-9:] run; what remains in front is the book.
  size_t split = text.size();
  while (split > 0) {
    const char c = text[split - 1];
    if ((c >= '0' && c <= '9') || c == ':') {
      --split;
    } else {
      break;
    }
  }

  const std::string bookPart = text.substr(0, split);
  const std::string numberPart = text.substr(split);
  if (bookPart.empty()) return false;

  int chapter = 1;
  int verse = 0;
  if (!numberPart.empty()) {
    const size_t colon = numberPart.find(':');
    if (colon == std::string::npos) {
      if (!parseInt(numberPart, chapter)) return false;
    } else {
      if (numberPart.find(':', colon + 1) != std::string::npos) return false;
      if (!parseInt(numberPart.substr(0, colon), chapter)) return false;
      if (!parseInt(numberPart.substr(colon + 1), verse)) return false;
    }
  }

  const BibleBookInfo* match = nullptr;
  for (const auto& book : books) {
    const std::string candidate = normalize(book.name);
    if (candidate == bookPart) {
      match = &book;
      break;
    }
    // First canonical prefix hit, kept only if no exact match turns up later.
    if (match == nullptr && candidate.rfind(bookPart, 0) == 0) {
      match = &book;
    }
  }
  if (match == nullptr) return false;
  if (chapter < 1 || chapter > match->chapterCount) return false;

  out.book = match->name;
  out.chapter = chapter;
  out.verse = verse;
  return true;
}
