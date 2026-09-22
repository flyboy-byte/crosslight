#pragma once

#include <string>

// Sleep-screen wallpaper actions for an image picked in the file browser. Both switch
// the Sleep Screen setting to Custom so the result shows on the next sleep.
namespace Wallpaper {

// Copies the image to /sleep.bmp, which the sleep screen always prefers.
bool setAsSleepScreen(const std::string& path);

// Copies the image into /.sleep/, the random-rotation folder. /sleep.bmp would keep
// overriding the rotation, so an existing one is moved into the folder, not deleted.
bool addToRotation(const std::string& path);

}  // namespace Wallpaper
