#include "ClaudeAnswerPager.h"

#include <GfxRenderer.h>

#include <algorithm>

namespace claude {

namespace {
constexpr int MAX_WRAPPED_LINES_PER_PARAGRAPH = 60;  // generous; a 500-token answer won't exceed this per paragraph
}  // namespace

void AnswerPager::setText(const GfxRenderer& renderer, const int fontId, const std::string& text,
                          const int textWidth, const int maxLinesPerPage) {
  lines.clear();
  pageIndex = 0;
  linesPerPage = std::max(1, maxLinesPerPage);

  // Split on blank lines (paragraph breaks); a lone "\n" inside a paragraph
  // collapses to a space before wrapping.
  size_t pos = 0;
  while (pos <= text.size()) {
    size_t breakPos = text.find("\n\n", pos);
    std::string paragraph = text.substr(pos, breakPos == std::string::npos ? std::string::npos : breakPos - pos);
    std::replace(paragraph.begin(), paragraph.end(), '\n', ' ');
    if (!paragraph.empty()) {
      for (auto& wrapped :
           renderer.wrappedText(fontId, paragraph.c_str(), textWidth, MAX_WRAPPED_LINES_PER_PARAGRAPH)) {
        lines.push_back(std::move(wrapped));
      }
      lines.push_back("");  // paragraph gap
    }
    if (breakPos == std::string::npos) break;
    pos = breakPos + 2;
  }
  if (!lines.empty() && lines.back().empty()) lines.pop_back();  // no trailing gap
}

void AnswerPager::clear() {
  lines.clear();
  pageIndex = 0;
}

int AnswerPager::pageCount() const {
  if (lines.empty()) return 1;
  return (static_cast<int>(lines.size()) + linesPerPage - 1) / linesPerPage;
}

bool AnswerPager::turnPage(const int delta) {
  const int next = std::clamp(pageIndex + delta, 0, pageCount() - 1);
  if (next == pageIndex) return false;
  pageIndex = next;
  return true;
}

std::pair<int, int> AnswerPager::currentPageRange() const {
  const int start = pageIndex * linesPerPage;
  const int end = std::min(static_cast<int>(lines.size()), start + linesPerPage);
  return {start, end};
}

}  // namespace claude
