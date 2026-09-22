#include "Wallpaper.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstdint>
#include <memory>
#include <new>

#include "CrossPointSettings.h"

namespace {

constexpr char SLEEP_BMP[] = "/sleep.bmp";
constexpr char ROTATION_DIR[] = "/.sleep";
constexpr size_t COPY_CHUNK = 4096;

bool copyFile(const std::string& from, const std::string& to) {
  std::unique_ptr<uint8_t[]> buf(new (std::nothrow) uint8_t[COPY_CHUNK]);
  if (!buf) {
    LOG_ERR("WALL", "OOM: copy buffer");
    return false;
  }
  HalFile in;
  HalFile out;
  if (!Storage.openFileForRead("WALL", from, in) || !Storage.openFileForWrite("WALL", to, out)) return false;
  int n;
  while ((n = in.read(buf.get(), COPY_CHUNK)) > 0) {
    if (out.write(buf.get(), static_cast<size_t>(n)) != static_cast<size_t>(n)) return false;
  }
  return n == 0 && out.close();
}

std::string fileName(const std::string& path) {
  const size_t slash = path.find_last_of('/');
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

// A path in the rotation folder that doesn't clobber an existing different image.
std::string rotationPathFor(const std::string& name) {
  std::string candidate = std::string(ROTATION_DIR) + "/" + name;
  const size_t dot = name.find_last_of('.');
  const std::string stem = name.substr(0, dot);
  const std::string ext = dot == std::string::npos ? "" : name.substr(dot);
  for (int i = 2; Storage.exists(candidate.c_str()) && i < 1000; ++i) {
    candidate = std::string(ROTATION_DIR) + "/" + stem + "_" + std::to_string(i) + ext;
  }
  return candidate;
}

void useCustomSleepScreen() {
  SETTINGS.sleepScreen = CrossPointSettings::SLEEP_SCREEN_MODE::CUSTOM;
  SETTINGS.saveToFile();
}

}  // namespace

namespace Wallpaper {

bool setAsSleepScreen(const std::string& path) {
  if (path != SLEEP_BMP && !copyFile(path, SLEEP_BMP)) {
    LOG_ERR("WALL", "Could not copy %s to %s", path.c_str(), SLEEP_BMP);
    return false;
  }
  useCustomSleepScreen();
  LOG_INF("WALL", "Sleep screen set to %s", path.c_str());
  return true;
}

bool addToRotation(const std::string& path) {
  if (!Storage.ensureDirectoryExists(ROTATION_DIR)) return false;
  const std::string dirPrefix = std::string(ROTATION_DIR) + "/";
  // /sleep.bmp itself is moved below, not copied (a copy would leave a duplicate).
  if (path != SLEEP_BMP && path.rfind(dirPrefix, 0) != 0 && !copyFile(path, rotationPathFor(fileName(path)))) {
    LOG_ERR("WALL", "Could not copy %s into %s", path.c_str(), ROTATION_DIR);
    return false;
  }
  if (Storage.exists(SLEEP_BMP) && !Storage.rename(SLEEP_BMP, rotationPathFor("sleep.bmp").c_str())) {
    LOG_ERR("WALL", "Could not move %s into %s", SLEEP_BMP, ROTATION_DIR);
    return false;
  }
  useCustomSleepScreen();
  LOG_INF("WALL", "Added %s to sleep rotation", path.c_str());
  return true;
}

}  // namespace Wallpaper
