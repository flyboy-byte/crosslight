#include "ClaudeHistoryStore.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>
#include <time.h>

#include <cstdio>
#include <cstring>

namespace claude {

namespace {
constexpr char kHistoryPath[] = "/claude/ask-history.jsonl";
// Drop-oldest wholesale once the log passes this size -- same convention as
// the plugin event outbox (src/util/PluginEvents.cpp), chosen for the same
// reason: a log nobody ever prunes must not grow forever, and a history this
// old is rarely worth digging for anyway.
constexpr size_t MAX_HISTORY_BYTES = 16 * 1024;
constexpr size_t MAX_LINE_BYTES = 4 * 1024;  // one Q + one ~500-token A, generously
}  // namespace

void appendHistory(const std::string& question, const std::string& answer) {
  JsonDocument doc;
  doc["q"] = question;
  doc["a"] = answer;
  doc["ts"] = static_cast<long long>(time(nullptr));  // 0 if the clock was never set
  std::string line;
  serializeJson(doc, line);
  line += '\n';
  if (line.size() > MAX_LINE_BYTES) {
    LOG_ERR("CLAUDE", "history line too large (%u); not saved", static_cast<unsigned>(line.size()));
    return;
  }

  Storage.ensureDirectoryExists("/claude");
  HalFile file = Storage.open(kHistoryPath, O_WRONLY | O_CREAT | O_APPEND);
  if (!file || !file.isOpen()) {
    LOG_ERR("CLAUDE", "history: open failed");
    return;
  }
  if (file.fileSize() > MAX_HISTORY_BYTES) {
    file.close();
    Storage.remove(kHistoryPath);
    file = Storage.open(kHistoryPath, O_WRONLY | O_CREAT | O_APPEND);
    if (!file || !file.isOpen()) return;
  }
  const uint64_t before = file.fileSize64();
  if (file.write(reinterpret_cast<const uint8_t*>(line.data()), line.size()) != line.size()) {
    // A torn line would glue onto the next append and corrupt both; cut back
    // to the last complete line instead.
    LOG_ERR("CLAUDE", "history: short append; entry dropped");
    file.truncate(before);
  }
  file.flush();
}

std::vector<HistoryEntry> loadHistory() {
  std::vector<HistoryEntry> out;
  std::string raw;
  if (!Storage.readFileToString("CLAUDE", kHistoryPath, MAX_HISTORY_BYTES + MAX_LINE_BYTES, raw)) return out;

  size_t pos = 0;
  while (pos < raw.size()) {
    const size_t nl = raw.find('\n', pos);
    const std::string line = raw.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
    pos = (nl == std::string::npos) ? raw.size() : nl + 1;
    if (line.empty()) continue;

    JsonDocument doc;
    if (deserializeJson(doc, line) != DeserializationError::Ok) continue;
    HistoryEntry entry;
    entry.question = doc["q"].as<const char*>() ? doc["q"].as<const char*>() : "";
    entry.answer = doc["a"].as<const char*>() ? doc["a"].as<const char*>() : "";
    entry.ts = doc["ts"].as<long long>();
    out.push_back(std::move(entry));
  }
  return out;
}

}  // namespace claude
