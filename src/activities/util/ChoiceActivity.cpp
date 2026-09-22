#include "ChoiceActivity.h"

#include <utility>

#include "HalDisplay.h"

ChoiceActivity::ChoiceActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string heading,
                               std::string body, std::vector<std::string> options)
    : Activity("Choice", renderer, mappedInput),
      heading(std::move(heading)),
      body(std::move(body)),
      options(std::move(options)) {}

void ChoiceActivity::onEnter() {
  Activity::onEnter();

  std::vector<const char*> labels;
  labels.reserve(options.size());
  for (const auto& option : options) labels.push_back(option.c_str());
  popup.show(heading.c_str(), body.c_str(), labels.data(), static_cast<int>(labels.size()), 0, [this](int idx) {
    ActivityResult res{MenuResult{idx}};
    res.isCancelled = false;
    setResult(std::move(res));
    finish();
  });

  requestUpdate(true);
}

void ChoiceActivity::render(RenderLock&&) {
  renderer.clearScreen();
  if (popup.processRender(renderer, mappedInput)) return;
  renderer.displayBuffer(HalDisplay::RefreshMode::FAST_REFRESH);
}

void ChoiceActivity::loop() {
  if (popup.handleInput(mappedInput, [this] { requestUpdate(); })) return;

  ActivityResult res;
  res.isCancelled = true;
  setResult(std::move(res));
  finish();
}
