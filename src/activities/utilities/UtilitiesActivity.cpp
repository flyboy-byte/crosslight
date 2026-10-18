#include "UtilitiesActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UiAppHelpers.h"
#include "components/UITheme.h"
#include "utilities/UtilityRegistry.h"

namespace fui = freeink::ui;

UtilitiesActivity::UtilitiesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("Utilities", renderer, mappedInput) {}

void UtilitiesActivity::onEnter() {
  UiListActivity::onEnter();
  const auto& registry = utilities::all();
  rowToUtility.clear();
  rowItems.clear();
  rowItems.reserve(registry.size() + 1);

  for (size_t i = 0; i < registry.size(); ++i) {
    const auto& utility = registry[i];
    if (utility.available && !utility.available()) continue;
    if (utility.title == StrId::STR_GLUCOSE_MONITOR) {
      // One tap flips the saved acquisition source without opening the app.
      // Switching inside Glucose Monitor itself used the Right button, the
      // same one used for paging everywhere else -- too easy to hit by
      // accident -- so this is the deliberate way to pick it ahead of time.
      // Kept in sync with GlucoseMonitorActivity::switchSource()'s own write.
      glucoseSourceSubtitle = SETTINGS.glucoseSource ? tr(STR_GLUCOSE_SOURCE_BLE) : tr(STR_GLUCOSE_SOURCE_WIFI);
      fui::ListItem toggleItem;
      toggleItem.label = tr(STR_GLUCOSE_SOURCE_TOGGLE);
      toggleItem.subtitle = glucoseSourceSubtitle.c_str();
      toggleItem.toggle = true;
      toggleItem.toggleChecked = SETTINGS.glucoseSource != 0;
      toggleItem.actionValue = static_cast<int16_t>(rowItems.size());
      rowItems.push_back(toggleItem);
      rowToUtility.push_back(kGlucoseSourceToggleRow);
    }
    fui::ListItem item;
    // Registry titles are StrIds, so the pointer is the interned translation
    // and outlives the row -- no local string to keep alive.
    item.label = I18n::getInstance().get(utility.title);
    item.icon = listIconFor(utility.icon);
    item.actionValue = static_cast<int16_t>(rowItems.size());
    rowItems.push_back(item);
    rowToUtility.push_back(static_cast<int>(i));
  }
}

void UtilitiesActivity::activateIndex(const int index) {
  if (index < 0 || index >= static_cast<int>(rowToUtility.size())) return;
  app.clearTapFlash();
  nav.selected = index;
  if (rowToUtility[index] == kGlucoseSourceToggleRow) {
    SETTINGS.glucoseSource = SETTINGS.glucoseSource ? 0 : 1;
    SETTINGS.saveToFile();
    glucoseSourceSubtitle = SETTINGS.glucoseSource ? tr(STR_GLUCOSE_SOURCE_BLE) : tr(STR_GLUCOSE_SOURCE_WIFI);
    rowItems[index].subtitle = glucoseSourceSubtitle.c_str();
    rowItems[index].toggleChecked = SETTINGS.glucoseSource != 0;
    requestUpdate();
    return;
  }
  const auto& utility = utilities::all()[rowToUtility[index]];
  if (!utility.create) return;
  startActivityForResult(utility.create(renderer, mappedInput), [](const ActivityResult&) {});
}

const char* UtilitiesActivity::headerTitle() const { return tr(STR_UTILITIES); }

void UtilitiesActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (rowItems.empty()) {
    screen.centeredText(tr(STR_NO_UTILITIES), screen.theme().bodyText);
    return;
  }

  fui::ListProps props;
  props.items = rowItems.data();
  props.count = static_cast<uint16_t>(rowItems.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // physical buttons stay in loop()
  syncListViewport(screen, props);
  screen.list(props);
}
