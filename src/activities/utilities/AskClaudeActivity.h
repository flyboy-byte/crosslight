#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "claude/ClaudeAnswerPager.h"
#include "claude/ClaudeHistoryStore.h"

struct Rect;

// Standalone "Ask Claude" utility: a free-prompt tile, independent of any
// open book -- the generic quick-ask wishlist item. One-shot requests only
// (decided with Logan, to keep the request body and RAM use small); past
// Q&A pairs are logged to SD for browsing (see claude::ClaudeHistoryStore)
// but that log is never fed back into a new request. Shares ClaudeClient and
// the AnswerPager display with Bible Passage Q&A. Plan:
// docs/crosslight/claude-features.md.
class AskClaudeActivity final : public Activity {
 public:
  AskClaudeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool preventAutoSleep() override { return true; }

 private:
  enum class State { Menu, Asking, Answer, HistoryList, HistoryEntry };

  void openNewQuestion();
  void sendQuestion(const std::string& question);
  void doAsk();
  void openHistory();
  void moveHistorySelection(int delta);
  void openHistoryEntry(int index);
  int visibleHistoryRows() const;

  Rect menuRowRect(int row) const;
  Rect historyRowRect(int row) const;
  int contentTop() const;

  State state = State::Menu;
  int menuSelRow = 0;
  claude::AnswerPager answerPager;
  std::string pendingQuestion;
  std::string statusLine;

  std::vector<claude::HistoryEntry> historyEntries;  // most-recent-first for display
  int historySelRow = 0;
  int historyScrollTop = 0;

  static constexpr int kMenuRowCount = 2;
};
