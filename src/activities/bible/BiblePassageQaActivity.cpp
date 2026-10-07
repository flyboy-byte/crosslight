#include "BiblePassageQaActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include <algorithm>
#include <utility>

#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "claude/ClaudeClient.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr unsigned long LONG_PRESS_MS = 1000;
}  // namespace

BiblePassageQaActivity::BiblePassageQaActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                              std::string book, const int chapter, std::string translationAbbr,
                                              std::vector<BibleVerse> verses, const int startIdx, const int endIdx)
    : Activity("BiblePassageQa", renderer, mappedInput),
      book(std::move(book)),
      chapter(chapter),
      translationAbbr(std::move(translationAbbr)),
      verses(std::move(verses)),
      startIdx(std::clamp(startIdx, 0, static_cast<int>(this->verses.size()) - 1)),
      endIdx(std::clamp(endIdx, 0, static_cast<int>(this->verses.size()) - 1)) {}

void BiblePassageQaActivity::onEnter() {
  Activity::onEnter();
  buildPassagePager();
  requestUpdate();
}

void BiblePassageQaActivity::onExit() {
  Activity::onExit();
  // Only torn down if a request actually brought Wi-Fi up (lazy-connect on
  // the first question, not on entry) -- same TLS-heap-fragmentation teardown
  // BibleDownloadActivity/ClaudePanelActivity use.
  if (WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

int BiblePassageQaActivity::contentTop() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  return metrics.topPadding + metrics.headerHeight + renderer.getLineHeight(UI_12_FONT_ID) +
         metrics.verticalSpacing;
}

std::string BiblePassageQaActivity::verseRangeLabel() const {
  if (startIdx < 0 || endIdx >= static_cast<int>(verses.size())) return "";
  if (startIdx == endIdx) return std::to_string(verses[startIdx].number);
  return std::to_string(verses[startIdx].number) + "-" + std::to_string(verses[endIdx].number);
}

void BiblePassageQaActivity::buildPassagePager() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int textWidth = renderer.getScreenWidth() - metrics.contentSidePadding * 2;
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int avail = safe.y + safe.height - contentTop() - lineH;  // room for the page footer

  std::string text;
  for (int i = startIdx; i <= endIdx && i < static_cast<int>(verses.size()); ++i) {
    text += std::to_string(verses[i].number) + "  " + verses[i].text;
    if (i < endIdx) text += "\n\n";
  }
  passagePager.setText(renderer, UI_10_FONT_ID, text, textWidth, std::max(1, avail / lineH));
}

void BiblePassageQaActivity::widenRange(const int startDelta, const int endDelta) {
  const int newStart = std::clamp(startIdx - startDelta, 0, endIdx);
  const int newEnd = std::clamp(endIdx + endDelta, startIdx, static_cast<int>(verses.size()) - 1);
  if (newStart == startIdx && newEnd == endIdx) return;
  startIdx = newStart;
  endIdx = newEnd;
  buildPassagePager();
  requestUpdate();
}

void BiblePassageQaActivity::moveQuestionSelection(const int delta) {
  questionSelRow = std::clamp(questionSelRow + delta, 0, kQuestionCount - 1);
  requestUpdate();
}

void BiblePassageQaActivity::activateQuestion(const int index) {
  switch (index) {
    case 0:
      sendQuestion(tr(STR_BIBLE_ASK_EXPLAIN));
      break;
    case 1:
      sendQuestion(tr(STR_BIBLE_ASK_HISTORY));
      break;
    case 2:
      sendQuestion(tr(STR_BIBLE_ASK_XREF));
      break;
    case 3:
      openCustomQuestion();
      break;
    default:
      break;
  }
}

void BiblePassageQaActivity::openCustomQuestion() {
  startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_BIBLE_ASK_CLAUDE)),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) return;
                           const auto& text = std::get<KeyboardResult>(result.data).text;
                           if (!text.empty()) sendQuestion(text);
                         });
}

std::string BiblePassageQaActivity::buildPrompt(const std::string& instruction) const {
  std::string prompt = translationAbbr + " " + book + " " + std::to_string(chapter) + ":" + verseRangeLabel() + "\n\n";
  for (int i = startIdx; i <= endIdx && i < static_cast<int>(verses.size()); ++i) {
    prompt += std::to_string(verses[i].number) + " " + verses[i].text + "\n";
  }
  prompt += "\n" + instruction;
  return prompt;
}

void BiblePassageQaActivity::sendQuestion(const std::string& instruction) {
  pendingInstruction = instruction;
  state = State::Asking;
  statusLine = tr(STR_BIBLE_ASK_ASKING);
  requestUpdate();

  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    doAsk();
    return;
  }
  WiFi.mode(WIFI_STA);
  startActivityForResult(
      std::make_unique<WifiSelectionActivity>(renderer, mappedInput, /*autoConnect=*/true, /*quiet=*/true),
      [this](const ActivityResult& result) {
        if (result.isCancelled) {
          RenderLock lock(*this);
          state = State::Questions;
          statusLine = tr(STR_WIFI_CONN_FAILED);
          requestUpdate();
          return;
        }
        doAsk();
      });
}

void BiblePassageQaActivity::doAsk() {
  requestUpdateAndWait();
  const claude::AskResult r = claude::ask(buildPrompt(pendingInstruction));
  LOG_INF("CLAUDE", "passage ask: error=%d http=%d", static_cast<int>(r.error), r.httpCode);
  RenderLock lock(*this);
  switch (r.error) {
    case claude::Error::Ok: {
      const auto& metrics = UITheme::getInstance().getMetrics();
      const int textWidth = renderer.getScreenWidth() - metrics.contentSidePadding * 2;
      const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
      const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
      const int avail = safe.y + safe.height - contentTop() - lineH;
      answerPager.setText(renderer, UI_10_FONT_ID, r.text, textWidth, std::max(1, avail / lineH));
      state = State::Answer;
      statusLine.clear();
      break;
    }
    case claude::Error::NoToken:
      statusLine = tr(STR_CLAUDE_NO_TOKEN);
      state = State::Questions;
      break;
    case claude::Error::Unauthorized:
      statusLine = tr(STR_CLAUDE_TOKEN_REJECTED);
      state = State::Questions;
      break;
    case claude::Error::LowMemory:
      statusLine = tr(STR_CLAUDE_LOW_MEMORY);
      state = State::Questions;
      break;
    default:
      statusLine = tr(STR_CLAUDE_REQUEST_FAILED);
      state = State::Questions;
      break;
  }
  requestUpdate();
}

Rect BiblePassageQaActivity::questionRowRect(const int row) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int rowH = renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing;
  return Rect{0, contentTop() + row * rowH, renderer.getScreenWidth(), rowH};
}

void BiblePassageQaActivity::loop() {
  if (state == State::Asking) return;  // blocking request in flight; nothing to route

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    switch (state) {
      case State::Range:
        finish();
        return;
      case State::Questions:
        state = State::Range;
        statusLine.clear();
        requestUpdate();
        return;
      case State::Answer:
        state = State::Questions;
        requestUpdate();
        return;
      default:
        return;
    }
  }

  if (state == State::Range) {
    if (mappedInput.wasLongPressed(MappedInputManager::Button::PageBack, LONG_PRESS_MS)) {
      widenRange(1, 0);
      return;
    }
    if (mappedInput.wasLongPressed(MappedInputManager::Button::PageForward, LONG_PRESS_MS)) {
      widenRange(0, 1);
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::PageBack)) {
      RenderLock lock(*this);
      passagePager.turnPage(-1);
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::PageForward)) {
      RenderLock lock(*this);
      passagePager.turnPage(1);
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      state = State::Questions;
      questionSelRow = 0;
      requestUpdate();
      return;
    }
    int x = 0, y = 0;
    if (mappedInput.wasScreenTapped(x, y) && y >= contentTop()) {
      state = State::Questions;
      questionSelRow = 0;
      requestUpdate();
    }
    return;
  }

  if (state == State::Questions) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      moveQuestionSelection(-1);
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      moveQuestionSelection(1);
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      activateQuestion(questionSelRow);
      return;
    }
    int x = 0, y = 0;
    if (mappedInput.wasScreenTapped(x, y)) {
      for (int row = 0; row < kQuestionCount; ++row) {
        const Rect r = questionRowRect(row);
        if (y >= r.y && y < r.y + r.height) {
          questionSelRow = row;
          activateQuestion(row);
          return;
        }
      }
    }
    return;
  }

  if (state == State::Answer) {
    if (mappedInput.wasReleased(MappedInputManager::Button::PageBack)) {
      RenderLock lock(*this);
      answerPager.turnPage(-1);
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::PageForward)) {
      RenderLock lock(*this);
      answerPager.turnPage(1);
      requestUpdate();
      return;
    }
  }
}

namespace {
void drawPagedLines(const GfxRenderer& renderer, const claude::AnswerPager& pager, const int top, const int pad) {
  const auto [start, end] = pager.currentPageRange();
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  int y = top;
  for (int i = start; i < end; ++i) {
    if (!pager.allLines()[i].empty()) renderer.drawText(UI_10_FONT_ID, pad, y, pager.allLines()[i].c_str());
    y += lineH;
  }
  if (pager.pageCount() > 1) {
    char foot[24];
    snprintf(foot, sizeof(foot), "%d / %d", pager.page() + 1, pager.pageCount());
    renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight() - lineH * 2, foot);
  }
}
}  // namespace

void BiblePassageQaActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pad = metrics.contentSidePadding;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BIBLE_ASK_CLAUDE));

  const int titleY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing / 2;
  std::string titleLine = book + " " + std::to_string(chapter) + ":" + verseRangeLabel();
  renderer.drawText(UI_12_FONT_ID, pad, titleY, titleLine.c_str(), true, EpdFontFamily::BOLD);

  MappedInputManager::Labels labels{tr(STR_BACK), "", "", ""};

  if (state == State::Range) {
    drawPagedLines(renderer, passagePager, contentTop(), pad);
    labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_BIBLE_ASK_WIDEN_HINT), tr(STR_BIBLE_ASK_CONTINUE_HINT), "");
  } else if (state == State::Questions) {
    const char* rowLabels[kQuestionCount] = {tr(STR_BIBLE_ASK_EXPLAIN), tr(STR_BIBLE_ASK_HISTORY),
                                             tr(STR_BIBLE_ASK_XREF), tr(STR_BIBLE_ASK_CUSTOM)};
    for (int row = 0; row < kQuestionCount; ++row) {
      const Rect r = questionRowRect(row);
      renderer.drawLine(0, r.y, pageWidth, r.y);
      const int ty = r.y + (r.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
      renderer.drawText(UI_12_FONT_ID, pad, ty, rowLabels[row], true,
                        row == questionSelRow ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
    }
    if (!statusLine.empty()) {
      renderer.drawText(UI_10_FONT_ID, pad, questionRowRect(kQuestionCount).y + metrics.verticalSpacing,
                        statusLine.c_str());
    }
  } else if (state == State::Asking) {
    const int y = (renderer.getScreenHeight()) / 2;
    const int w = renderer.getTextWidth(UI_10_FONT_ID, statusLine.c_str());
    renderer.drawText(UI_10_FONT_ID, (pageWidth - w) / 2, y, statusLine.c_str());
  } else if (state == State::Answer) {
    drawPagedLines(renderer, answerPager, contentTop(), pad);
  }

  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
