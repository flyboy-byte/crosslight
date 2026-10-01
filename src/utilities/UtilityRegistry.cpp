#include "UtilityRegistry.h"

#include "activities/utilities/CalculatorActivity.h"
#include "activities/utilities/CameraScanActivity.h"
#include "activities/utilities/WifiCaptureActivity.h"
#include "activities/utilities/WifiScanActivity.h"
#include "activities/utilities/WifiThreatActivity.h"

namespace utilities {

namespace {
// Adding a utility: include its header above and add one line here. Keep the
// list in the order you want it to appear.
const std::vector<Utility> kUtilities = {
    {StrId::STR_CALCULATOR, Blocks,
     [](GfxRenderer& renderer, MappedInputManager& mappedInput) -> std::unique_ptr<Activity> {
       return std::make_unique<CalculatorActivity>(renderer, mappedInput);
     }},
    {StrId::STR_CAMERA_SCAN, Wifi,
     [](GfxRenderer& renderer, MappedInputManager& mappedInput) -> std::unique_ptr<Activity> {
       return std::make_unique<CameraScanActivity>(renderer, mappedInput);
     }},
    {StrId::STR_WIFI_SCAN, Wifi,
     [](GfxRenderer& renderer, MappedInputManager& mappedInput) -> std::unique_ptr<Activity> {
       return std::make_unique<WifiScanActivity>(renderer, mappedInput);
     }},
    {StrId::STR_WIFI_THREAT, Wifi,
     [](GfxRenderer& renderer, MappedInputManager& mappedInput) -> std::unique_ptr<Activity> {
       return std::make_unique<WifiThreatActivity>(renderer, mappedInput);
     }},
    {StrId::STR_WIFI_CAPTURE, Wifi,
     [](GfxRenderer& renderer, MappedInputManager& mappedInput) -> std::unique_ptr<Activity> {
       return std::make_unique<WifiCaptureActivity>(renderer, mappedInput);
     }},
};
}  // namespace

const std::vector<Utility>& all() { return kUtilities; }

}  // namespace utilities
