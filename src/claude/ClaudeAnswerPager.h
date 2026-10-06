#pragma once

#include <string>
#include <utility>
#include <vector>

class GfxRenderer;

// Shared paginated-text display for Claude answers (Bible Passage Q&A, Ask
// Claude): wraps arbitrary-length response text into lines and pages through
// them with the same page-turn-button handling Compare Translations uses --
// deliberately NOT a "fit the screen" prompt instruction, since a model's
// output length doesn't reliably match a given font's character width. The
// response-length ceiling is ClaudeClient's max_tokens cap; this just renders
// however long that turns out to be.
namespace claude {

class AnswerPager {
 public:
  // Splits on blank lines (paragraph breaks) and wraps each paragraph at
  // textWidth; a lone "\n" inside a paragraph is treated as a space (prose
  // from the API doesn't rely on single line breaks for meaning).
  void setText(const GfxRenderer& renderer, int fontId, const std::string& text, int textWidth, int maxLinesPerPage);

  void clear();
  bool empty() const { return lines.empty(); }
  int pageCount() const;
  int page() const { return pageIndex; }
  // Returns false (no-op) at the first/last page -- callers checking for a
  // page-turn no-op (e.g. to fall through to another action) can rely on that.
  bool turnPage(int delta);

  // [start, end) line indices for the current page, into allLines().
  std::pair<int, int> currentPageRange() const;
  const std::vector<std::string>& allLines() const { return lines; }

 private:
  std::vector<std::string> lines;
  int linesPerPage = 1;
  int pageIndex = 0;
};

}  // namespace claude
