#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "components/OptionPopup.h"

// A modal list of options (ConfirmationActivity with N choices). Result: MenuResult with
// `action` = the chosen option's index; cancelled on Back or a tap outside the dialog.
class ChoiceActivity : public Activity {
  std::string heading;
  std::string body;
  std::vector<std::string> options;
  OptionPopup popup;

 public:
  ChoiceActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string heading, std::string body,
                 std::vector<std::string> options);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&& lock) override;
};
