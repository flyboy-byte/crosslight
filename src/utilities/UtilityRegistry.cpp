#include "UtilityRegistry.h"

#include <HalStorage.h>

#include "activities/utilities/BleScanActivity.h"
#include "activities/utilities/CalculatorActivity.h"
#include "activities/utilities/FlashlightActivity.h"
#include "activities/utilities/UnitConverterActivity.h"
#include "activities/utilities/WifiScanActivity.h"
// Offensive / radio-audit tools live in src/offensive and are hidden by
// default. See src/offensive/README.md and docs/crosslight/offensive/.
#include "offensive/activities/BeaconFloodActivity.h"
#include "offensive/activities/BleSpoofActivity.h"
#include "offensive/activities/CameraScanActivity.h"
#include "offensive/activities/EvilTwinActivity.h"
#include "offensive/activities/PmkidHarvestActivity.h"
#include "offensive/activities/WifiCaptureActivity.h"
#include "offensive/activities/WifiThreatActivity.h"

namespace utilities {

namespace {

// General-purpose utilities. Always shown. Wi-Fi Analyzer and Bluetooth
// Scanner are plain receive-only radio tools (channel/signal/vendor info).
// Adding one: include its header above and add one line here.
const std::vector<Utility> kGeneral = {
    {StrId::STR_CALCULATOR, Calculator,
     [](GfxRenderer& renderer, MappedInputManager& mappedInput) -> std::unique_ptr<Activity> {
       return std::make_unique<CalculatorActivity>(renderer, mappedInput);
     }},
    {StrId::STR_FLASHLIGHT, Flashlight,
     [](GfxRenderer& renderer, MappedInputManager& mappedInput) -> std::unique_ptr<Activity> {
       return std::make_unique<FlashlightActivity>(renderer, mappedInput);
     }},
    {StrId::STR_UNIT_CONVERTER, Convert,
     [](GfxRenderer& renderer, MappedInputManager& mappedInput) -> std::unique_ptr<Activity> {
       return std::make_unique<UnitConverterActivity>(renderer, mappedInput);
     }},
    {StrId::STR_WIFI_SCAN, Wifi,
     [](GfxRenderer& renderer, MappedInputManager& mappedInput) -> std::unique_ptr<Activity> {
       return std::make_unique<WifiScanActivity>(renderer, mappedInput);
     }},
    {StrId::STR_BLE_SCAN, Wifi,
     [](GfxRenderer& renderer, MappedInputManager& mappedInput) -> std::unique_ptr<Activity> {
       return std::make_unique<BleScanActivity>(renderer, mappedInput);
     }},
};

// Offensive / active-audit tools. Compiled in, but hidden from the menu unless
// the SD card holds the flag file below. These transmit, impersonate, or
// capture credential material and are for the operator's own gear only. To
// reveal them: create an (empty) file /offensive/enabled on the SD card and
// reboot. Removing the file hides them again.
constexpr char kOffensiveFlag[] = "/offensive/enabled";

const std::vector<Utility> kOffensive = {
    {StrId::STR_CAMERA_SCAN, Wifi,
     [](GfxRenderer& renderer, MappedInputManager& mappedInput) -> std::unique_ptr<Activity> {
       return std::make_unique<CameraScanActivity>(renderer, mappedInput);
     }},
    {StrId::STR_WIFI_THREAT, Wifi,
     [](GfxRenderer& renderer, MappedInputManager& mappedInput) -> std::unique_ptr<Activity> {
       return std::make_unique<WifiThreatActivity>(renderer, mappedInput);
     }},
    {StrId::STR_WIFI_CAPTURE, Wifi,
     [](GfxRenderer& renderer, MappedInputManager& mappedInput) -> std::unique_ptr<Activity> {
       return std::make_unique<WifiCaptureActivity>(renderer, mappedInput);
     }},
    {StrId::STR_PMKID_HARVEST, Wifi,
     [](GfxRenderer& renderer, MappedInputManager& mappedInput) -> std::unique_ptr<Activity> {
       return std::make_unique<PmkidHarvestActivity>(renderer, mappedInput);
     }},
    {StrId::STR_BEACON_FLOOD, Wifi,
     [](GfxRenderer& renderer, MappedInputManager& mappedInput) -> std::unique_ptr<Activity> {
       return std::make_unique<BeaconFloodActivity>(renderer, mappedInput);
     }},
    {StrId::STR_EVIL_TWIN, Wifi,
     [](GfxRenderer& renderer, MappedInputManager& mappedInput) -> std::unique_ptr<Activity> {
       return std::make_unique<EvilTwinActivity>(renderer, mappedInput);
     }},
    {StrId::STR_BLE_SPOOF, Wifi,
     [](GfxRenderer& renderer, MappedInputManager& mappedInput) -> std::unique_ptr<Activity> {
       return std::make_unique<BleSpoofActivity>(renderer, mappedInput);
     }},
};

// Built once per boot: general tools, plus the offensive tools when the SD
// flag is present. Cached, so the flag is read once (drop the file, reboot).
const std::vector<Utility>& resolved() {
  static const std::vector<Utility> list = [] {
    std::vector<Utility> out = kGeneral;
    if (Storage.exists(kOffensiveFlag)) {
      out.insert(out.end(), kOffensive.begin(), kOffensive.end());
    }
    return out;
  }();
  return list;
}

}  // namespace

const std::vector<Utility>& all() { return resolved(); }

}  // namespace utilities
