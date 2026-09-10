#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "bible/BibleChapterLoader.h"

// CrossLight's first native screen: a Bible reader. Reads a getBible-format
// translation file straight off the SD card (no download required -- see
// PLAN.md's SD-first decision) and renders one chapter. Book/chapter is
// chosen via Confirm -> BibleBookSelectionActivity -> BibleChapterSelectionActivity
// (see openBookPicker/openChapterPicker); translation selection (beyond the
// hardcoded KJV path) comes later.
class BibleActivity final : public Activity {
 public:
  explicit BibleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Bible", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void goBack() { finish(); }
  void openBookPicker();
  void openChapterPicker();
  void loadCurrentChapter();

  std::string currentBook = "Genesis";
  int currentChapter = 1;
  int currentBookChapterCount = 50;  // Genesis's count; refined once a book is actually picked

  std::vector<BibleVerse> verses;
  bool loaded = false;
};
