#pragma once

#include <string>
#include <vector>

// Local, read-only-on-replay history log for the Ask Claude utility: every
// Q&A pair gets appended to an SD file so past questions are browsable later
// -- but the log is NEVER read back into a new request. Keeping every ask()
// call one-shot (decided with Logan) means no growing request body and no
// extra RAM to hold conversation state; this is just a local record, the
// same relationship CrossPoint's plugin event outbox has to its subscribers
// (see src/util/PluginEvents.cpp, whose drop-oldest-wholesale cap this
// mirrors). See docs/crosslight/claude-features.md Phase D.
namespace claude {

struct HistoryEntry {
  std::string question;
  std::string answer;
  long long ts = 0;  // unix seconds, 0 if the clock was never set
};

// Appends one entry as a JSON line. Drops the oldest entries (wholesale, by
// truncating the whole file) once the log exceeds its byte cap -- cheap, and
// this is a convenience log, not a record anyone needs kept forever.
void appendHistory(const std::string& question, const std::string& answer);

// Most-recent-last, matching file order. Empty if the file doesn't exist yet
// or every line failed to parse.
std::vector<HistoryEntry> loadHistory();

}  // namespace claude
