#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "bible/BibleChapterLoader.h"
#include "claude/ClaudeAnswerPager.h"

// "Ask Claude" for the Bible reader: reached from the reader's Confirm menu
// (BibleMenuActivity), not Bible Hub -- Compare Translations' friction is that
// Hub-based entry only seeds book+chapter, so you always leave the reader and
// re-pick verse; this instead starts from exactly the verses already on
// screen (the caller passes the whole loaded chapter plus the current page's
// verse-index range) and never needs its own book/chapter picker.
//
// Three screens in one activity, so asking a follow-up question about the
// same passage costs no new navigation and (once Wi-Fi is up) no reconnect:
//   Range -> Questions -> Answer, with Back walking one step back each time.
// Page-turn buttons (PageBack/PageForward) page the passage/answer text
// in those screens, matching the Compare Translations fix -- a long press
// (same buttons) widens the verse range instead of jumping chapters.
struct Rect;

class BiblePassageQaActivity final : public Activity {
 public:
  BiblePassageQaActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string book, int chapter,
                        std::string translationAbbr, std::vector<BibleVerse> verses, int startIdx, int endIdx);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool preventAutoSleep() override { return true; }

 private:
  enum class State { Range, Questions, Asking, Answer };

  void buildPassagePager();
  void widenRange(int startDelta, int endDelta);
  void moveQuestionSelection(int delta);
  void activateQuestion(int index);
  void openCustomQuestion();
  void sendQuestion(const std::string& instruction);
  void doAsk();

  std::string verseRangeLabel() const;
  std::string buildPrompt(const std::string& instruction) const;

  Rect questionRowRect(int row) const;
  int contentTop() const;

  std::string book;
  int chapter;
  std::string translationAbbr;
  std::vector<BibleVerse> verses;
  int startIdx;
  int endIdx;

  State state = State::Range;
  claude::AnswerPager passagePager;
  claude::AnswerPager answerPager;
  int questionSelRow = 0;
  std::string pendingInstruction;
  std::string statusLine;

  static constexpr int kQuestionCount = 4;
};
