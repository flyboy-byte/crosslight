#include "LibreHistory.h"

#include <cstdlib>
#include <cstring>

namespace libre {

namespace {
constexpr char kValueKey[] = "\"value\":";
constexpr char kAgeKey[] = "\"age_s\":";
constexpr size_t kKeepTail = 16;     // enough to catch a key split across two chunks
constexpr size_t kMaxPending = 2048;  // a single reading object is ~250 bytes
}  // namespace

// Parses the number (or null) starting at `pos`. Returns false when the token
// might continue in the next chunk (so the caller waits for more data).
bool HistoryScanner::takeInt(size_t pos, long& value, bool& isNull, size_t& end) const {
  while (pos < buf.size() && buf[pos] == ' ') ++pos;
  if (pos >= buf.size()) return false;
  if (buf.compare(pos, 4, "null") == 0) {
    isNull = true;
    end = pos + 4;
    return true;
  }
  if (buf.size() - pos < 4 && buf[pos] == 'n') return false;
  size_t i = pos;
  if (i < buf.size() && buf[i] == '-') ++i;
  while (i < buf.size() && buf[i] >= '0' && buf[i] <= '9') ++i;
  if (i >= buf.size()) return false;  // digits may continue
  if (i == pos) {
    isNull = true;  // not a number; treat as absent and move on
    end = pos + 1;
    return true;
  }
  isNull = false;
  value = std::strtol(buf.c_str() + pos, nullptr, 10);
  end = i;
  return true;
}

void HistoryScanner::feed(const char* data, size_t len) {
  buf.append(data, len);
  for (;;) {
    const size_t v = buf.find(kValueKey);
    if (v == std::string::npos) {
      if (buf.size() > kKeepTail) buf.erase(0, buf.size() - kKeepTail);
      return;
    }
    long value = 0;
    bool valueNull = false;
    size_t valueEnd = 0;
    if (!takeInt(v + sizeof(kValueKey) - 1, value, valueNull, valueEnd)) {
      buf.erase(0, v);
      return;
    }
    const size_t a = buf.find(kAgeKey, valueEnd);
    if (a == std::string::npos) {
      buf.erase(0, v);
      if (buf.size() > kMaxPending) buf.clear();  // malformed; resync on the next value key
      return;
    }
    long age = 0;
    bool ageNull = false;
    size_t ageEnd = 0;
    if (!takeInt(a + sizeof(kAgeKey) - 1, age, ageNull, ageEnd)) {
      buf.erase(0, v);
      return;
    }
    if (!valueNull && !ageNull && out.size() < maxPoints) {
      HistoryPoint p;
      p.value = static_cast<int16_t>(value);
      p.ageS = static_cast<int32_t>(age);
      out.push_back(p);
    }
    buf.erase(0, ageEnd);
  }
}

}  // namespace libre
