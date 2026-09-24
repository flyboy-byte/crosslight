#include "StartupPasswordActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <utility>

#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "util/DeviceLock.h"

namespace fui = freeink::ui;

StartupPasswordActivity::StartupPasswordActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("StartupPassword", renderer, mappedInput) {}

void StartupPasswordActivity::onEnter() {
  UiListActivity::onEnter();
  DEVICE_LOCK.loadFromFile();
  buildRows();
}

void StartupPasswordActivity::buildRows() {
  const bool set = DEVICE_LOCK.hasPassphrase();
  rowItems.clear();
  rowItems.reserve(3);

  fui::ListItem setItem;
  setItem.label = set ? tr(STR_CHANGE_PASSPHRASE) : tr(STR_SET_PASSPHRASE);
  setItem.actionValue = SetPassphrase;
  rowItems.push_back(setItem);

  fui::ListItem wakeItem;
  wakeItem.label = tr(STR_LOCK_ON_WAKE);
  wakeItem.toggle = true;
  wakeItem.toggleChecked = DEVICE_LOCK.lockOnWake();
  wakeItem.enabled = set;
  wakeItem.actionValue = LockOnWake;
  rowItems.push_back(wakeItem);

  fui::ListItem removeItem;
  removeItem.label = tr(STR_REMOVE_PASSPHRASE);
  removeItem.enabled = set;
  removeItem.actionValue = Remove;
  rowItems.push_back(removeItem);
}

// Asking for the current passphrase before changing or removing it is the
// whole point: otherwise the lock is only as strong as reaching this screen.
void StartupPasswordActivity::withCurrentPassphrase(std::function<void()> next) {
  if (!DEVICE_LOCK.hasPassphrase()) {
    next();
    return;
  }
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_CURRENT_PASSPHRASE), "",
                                              DeviceLock::MAX_LENGTH, InputType::Password),
      [this, next = std::move(next)](const ActivityResult& result) {
        if (result.isCancelled) return;
        if (!DEVICE_LOCK.verify(std::get<KeyboardResult>(result.data).text)) {
          RenderLock lock(*this);
          GUI.drawPopup(renderer, tr(STR_WRONG_PASSPHRASE));
          delay(900);
          requestUpdate(true);
          return;
        }
        next();
      });
}

void StartupPasswordActivity::promptNewPassphrase() {
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_NEW_PASSPHRASE), "",
                                              DeviceLock::MAX_LENGTH, InputType::Password),
      [this](const ActivityResult& result) {
        if (result.isCancelled) return;
        const std::string entered = std::get<KeyboardResult>(result.data).text;
        if (!DEVICE_LOCK.setPassphrase(entered)) {
          RenderLock lock(*this);
          GUI.drawPopup(renderer, tr(STR_PASSPHRASE_TOO_SHORT));
          delay(1200);
        } else {
          RenderLock lock(*this);
          GUI.drawPopup(renderer, tr(STR_DONE));
          delay(700);
        }
        RenderLock lock(*this);
        buildRows();
        requestUpdate(true);
      });
}

void StartupPasswordActivity::activateIndex(const int index) {
  if (index < 0 || index >= static_cast<int>(rowItems.size()) || !rowItems[index].enabled) return;
  app.clearTapFlash();
  nav.selected = index;

  switch (static_cast<Row>(rowItems[index].actionValue)) {
    case SetPassphrase:
      withCurrentPassphrase([this] { promptNewPassphrase(); });
      return;
    case LockOnWake: {
      RenderLock lock(*this);
      DEVICE_LOCK.setLockOnWake(!DEVICE_LOCK.lockOnWake());
      buildRows();
      requestUpdate();
      return;
    }
    case Remove:
      withCurrentPassphrase([this] {
        {
          RenderLock lock(*this);
          DEVICE_LOCK.clear();
          buildRows();
        }
        requestUpdate(true);
      });
      return;
  }
}

const char* StartupPasswordActivity::headerTitle() const { return tr(STR_STARTUP_PASSWORD); }

void StartupPasswordActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  fui::ListProps props;
  props.items = rowItems.data();
  props.count = static_cast<uint16_t>(rowItems.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // physical buttons stay in loop()
  syncListViewport(screen, props);
  screen.list(props);
}
