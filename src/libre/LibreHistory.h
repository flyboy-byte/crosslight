#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace libre {

// One point of the bridge's history (GET /api/v1/glucose), oldest first.
// ageS is seconds before the request, taken from the reading's own age_s so
// no trusted clock is needed to place it on the graph.
struct HistoryPoint {
  int16_t value = 0;
  int32_t ageS = 0;
};

// Incremental scanner for the history body. The body is up to ~40 KB of JSON
// (3 h at one reading a minute) and the heap is tight while TLS is up, so it is
// never held whole: chunks are fed as they arrive and only the "value" and
// "age_s" fields are pulled out. Readings with a null value (HIGH/LOW/
// UNAVAILABLE) are skipped -- they have no number to plot.
class HistoryScanner {
 public:
  explicit HistoryScanner(std::vector<HistoryPoint>& out, size_t maxPoints = 400) : out(out), maxPoints(maxPoints) {}

  void feed(const char* data, size_t len);

 private:
  bool takeInt(size_t pos, long& value, bool& isNull, size_t& end) const;

  std::vector<HistoryPoint>& out;
  size_t maxPoints;
  std::string buf;
};

}  // namespace libre
