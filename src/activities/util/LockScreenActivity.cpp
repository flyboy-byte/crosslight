#include "LockScreenActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <utility>

#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/DeviceLock.h"

LockScreenActivity::LockScreenActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                       std::function<void()> onUnlocked)
    : Activity("LockScreen", renderer, mappedInput), onUnlocked(std::move(onUnlocked)) {}

void LockScreenActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
  prompt();
}

void LockScreenActivity::prompt() {
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_ENTER_PASSPHRASE), "",
                                              DeviceLock::MAX_LENGTH, InputType::Password),
      [this](const ActivityResult& result) {
        // Cancelling is not a way out: ask again. Anything else would make Back
        // the passphrase.
        if (result.isCancelled) {
          prompt();
          return;
        }
        if (!DEVICE_LOCK.verify(std::get<KeyboardResult>(result.data).text)) {
          {
            RenderLock lock(*this);
            wrong = true;
          }
          requestUpdate();
          prompt();
          return;
        }
        // Hand control to whatever boot would have done. finish() first so this
        // screen is gone before the next one paints.
        const auto unlocked = onUnlocked;
        finish();
        if (unlocked) unlocked();
      });
}

void LockScreenActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int lineH = renderer.getLineHeight(UI_12_FONT_ID);

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_LOCKED));
  renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2 - lineH, tr(STR_ENTER_PASSPHRASE), true,
                            EpdFontFamily::BOLD);
  if (wrong) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + lineH, tr(STR_WRONG_PASSPHRASE));
  }
  renderer.displayBuffer();
}
