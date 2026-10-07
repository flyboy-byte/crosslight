#include "AskClaudeActivity.h"

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

AskClaudeActivity::AskClaudeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("AskClaude", renderer, mappedInput) {}

void AskClaudeActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void AskClaudeActivity::onExit() {
  Activity::onExit();
  // Only torn down if a request actually brought Wi-Fi up (lazy-connect on
  // the first question) -- same TLS-heap-fragmentation teardown Claude Panel
  // and Bible Passage Q&A use.
  if (WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

int AskClaudeActivity::contentTop() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  return metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
}

Rect AskClaudeActivity::menuRowRect(const int row) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int rowH = renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing;
  return Rect{0, contentTop() + row * rowH, renderer.getScreenWidth(), rowH};
}

Rect AskClaudeActivity::historyRowRect(const int row) const { return menuRowRect(row); }

int AskClaudeActivity::visibleHistoryRows() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int rowH = renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing;
  return std::max(1, (safe.y + safe.height - contentTop()) / rowH);
}

void AskClaudeActivity::openNewQuestion() {
  startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_ASK_CLAUDE)),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) return;
                           const auto& text = std::get<KeyboardResult>(result.data).text;
                           if (!text.empty()) sendQuestion(text);
                         });
}

void AskClaudeActivity::sendQuestion(const std::string& question) {
  pendingQuestion = question;
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
          state = State::Menu;
          statusLine = tr(STR_WIFI_CONN_FAILED);
          requestUpdate();
          return;
        }
        doAsk();
      });
}

void AskClaudeActivity::doAsk() {
  requestUpdateAndWait();
  const claude::AskResult r = claude::ask(pendingQuestion);
  LOG_INF("CLAUDE", "ask: error=%d http=%d", static_cast<int>(r.error), r.httpCode);
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
      claude::appendHistory(pendingQuestion, r.text);
      break;
    }
    case claude::Error::NoToken:
      statusLine = tr(STR_CLAUDE_NO_TOKEN);
      state = State::Menu;
      break;
    case claude::Error::Unauthorized:
      statusLine = tr(STR_CLAUDE_TOKEN_REJECTED);
      state = State::Menu;
      break;
    case claude::Error::LowMemory:
      statusLine = tr(STR_CLAUDE_LOW_MEMORY);
      state = State::Menu;
      break;
    default:
      statusLine = tr(STR_CLAUDE_REQUEST_FAILED);
      state = State::Menu;
      break;
  }
  requestUpdate();
}

void AskClaudeActivity::openHistory() {
  const auto loaded = claude::loadHistory();  // file order: oldest-first
  historyEntries.assign(loaded.rbegin(), loaded.rend());  // most-recent-first for display
  historySelRow = 0;
  historyScrollTop = 0;
  state = State::HistoryList;
  requestUpdate();
}

void AskClaudeActivity::moveHistorySelection(const int delta) {
  if (historyEntries.empty()) return;
  historySelRow = std::clamp(historySelRow + delta, 0, static_cast<int>(historyEntries.size()) - 1);
  if (historySelRow < historyScrollTop) historyScrollTop = historySelRow;
  const int visible = visibleHistoryRows();
  if (historySelRow >= historyScrollTop + visible) historyScrollTop = historySelRow - visible + 1;
  requestUpdate();
}

void AskClaudeActivity::openHistoryEntry(const int index) {
  if (index < 0 || index >= static_cast<int>(historyEntries.size())) return;
  const auto& entry = historyEntries[index];
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int textWidth = renderer.getScreenWidth() - metrics.contentSidePadding * 2;
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int avail = safe.y + safe.height - contentTop() - lineH;
  // Show the question too -- a replay with no context of what was asked
  // isn't useful days later.
  answerPager.setText(renderer, UI_10_FONT_ID, entry.question + "\n\n" + entry.answer, textWidth,
                      std::max(1, avail / lineH));
  state = State::HistoryEntry;
  requestUpdate();
}

void AskClaudeActivity::loop() {
  if (state == State::Asking) return;  // blocking request in flight

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    switch (state) {
      case State::Menu:
        finish();
        return;
      case State::Answer:
        state = State::Menu;
        statusLine.clear();
        requestUpdate();
        return;
      case State::HistoryList:
        state = State::Menu;
        requestUpdate();
        return;
      case State::HistoryEntry:
        state = State::HistoryList;
        requestUpdate();
        return;
      default:
        return;
    }
  }

  if (state == State::Menu) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      menuSelRow = std::clamp(menuSelRow - 1, 0, kMenuRowCount - 1);
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      menuSelRow = std::clamp(menuSelRow + 1, 0, kMenuRowCount - 1);
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (menuSelRow == 0) openNewQuestion();
      else openHistory();
      return;
    }
    int x = 0, y = 0;
    if (mappedInput.wasScreenTapped(x, y)) {
      for (int row = 0; row < kMenuRowCount; ++row) {
        const Rect r = menuRowRect(row);
        if (y >= r.y && y < r.y + r.height) {
          menuSelRow = row;
          if (row == 0) openNewQuestion();
          else openHistory();
          return;
        }
      }
    }
    return;
  }

  if (state == State::Answer || state == State::HistoryEntry) {
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
    return;
  }

  if (state == State::HistoryList) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      moveHistorySelection(-1);
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      moveHistorySelection(1);
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      openHistoryEntry(historySelRow);
      return;
    }
    int x = 0, y = 0;
    if (mappedInput.wasScreenTapped(x, y)) {
      const int visible = visibleHistoryRows();
      for (int row = 0; row < visible; ++row) {
        const int index = historyScrollTop + row;
        if (index >= static_cast<int>(historyEntries.size())) break;
        const Rect r = historyRowRect(row);
        if (y >= r.y && y < r.y + r.height) {
          historySelRow = index;
          openHistoryEntry(index);
          return;
        }
      }
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

void AskClaudeActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pad = metrics.contentSidePadding;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_ASK_CLAUDE));

  MappedInputManager::Labels labels{tr(STR_BACK), "", "", ""};

  if (state == State::Menu) {
    const char* rowLabels[kMenuRowCount] = {tr(STR_ASK_CLAUDE_NEW), tr(STR_ASK_CLAUDE_HISTORY)};
    for (int row = 0; row < kMenuRowCount; ++row) {
      const Rect r = menuRowRect(row);
      renderer.drawLine(0, r.y, pageWidth, r.y);
      const int ty = r.y + (r.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
      renderer.drawText(UI_12_FONT_ID, pad, ty, rowLabels[row], true,
                        row == menuSelRow ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
    }
    if (!statusLine.empty()) {
      renderer.drawText(UI_10_FONT_ID, pad, menuRowRect(kMenuRowCount).y + metrics.verticalSpacing,
                        statusLine.c_str());
    }
  } else if (state == State::Asking) {
    const int y = renderer.getScreenHeight() / 2;
    const int w = renderer.getTextWidth(UI_10_FONT_ID, statusLine.c_str());
    renderer.drawText(UI_10_FONT_ID, (pageWidth - w) / 2, y, statusLine.c_str());
  } else if (state == State::Answer || state == State::HistoryEntry) {
    drawPagedLines(renderer, answerPager, contentTop(), pad);
    labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  } else if (state == State::HistoryList) {
    if (historyEntries.empty()) {
      renderer.drawText(UI_10_FONT_ID, pad, contentTop(), tr(STR_ASK_CLAUDE_HISTORY_EMPTY));
    } else {
      const int visible = visibleHistoryRows();
      const int rowWidth = pageWidth - pad * 2;
      for (int row = 0; row < visible; ++row) {
        const int index = historyScrollTop + row;
        if (index >= static_cast<int>(historyEntries.size())) break;
        const Rect r = historyRowRect(row);
        renderer.drawLine(0, r.y, pageWidth, r.y);
        const int ty = r.y + (r.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
        const std::string truncated =
            renderer.truncatedText(UI_12_FONT_ID, historyEntries[index].question.c_str(), rowWidth);
        renderer.drawText(UI_12_FONT_ID, pad, ty, truncated.c_str(), true,
                          index == historySelRow ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
      }
    }
  }

  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
