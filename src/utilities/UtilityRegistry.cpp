#include "UtilityRegistry.h"

#include "activities/utilities/CalculatorActivity.h"

namespace utilities {

namespace {
// Adding a utility: include its header above and add one line here. Keep the
// list in the order you want it to appear.
const std::vector<Utility> kUtilities = {
    {StrId::STR_CALCULATOR, Blocks,
     [](GfxRenderer& renderer, MappedInputManager& mappedInput) -> std::unique_ptr<Activity> {
       return std::make_unique<CalculatorActivity>(renderer, mappedInput);
     }},
};
}  // namespace

const std::vector<Utility>& all() { return kUtilities; }

}  // namespace utilities
