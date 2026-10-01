#include "util/VendorDb.h"

#include <Logging.h>

bool VendorDb::open(const char* path) {
  close();
  if (!Storage.exists(path)) return false;
  file = Storage.open(path);
  if (!file.isOpen()) return false;

  uint8_t header[vendordb::HEADER_SIZE];
  if (file.read(header, vendordb::HEADER_SIZE) != vendordb::HEADER_SIZE || !vendordb::parseHeader(header, count)) {
    LOG_ERR("VENDORDB", "%s is not a valid vendor db", path);
    close();
    return false;
  }
  LOG_INF("VENDORDB", "Opened %s: %u entries", path, static_cast<unsigned>(count));
  return true;
}

void VendorDb::close() {
  if (file.isOpen()) file.close();
  count = 0;
}

std::string VendorDb::lookup(const uint32_t key) {
  if (!file.isOpen() || count == 0) return "";

  int lo = 0;
  int hi = static_cast<int>(count) - 1;
  uint8_t record[vendordb::RECORD_SIZE];
  while (lo <= hi) {
    const int mid = lo + (hi - lo) / 2;
    const size_t offset = vendordb::HEADER_SIZE + static_cast<size_t>(mid) * vendordb::RECORD_SIZE;
    if (!file.seek(offset) || file.read(record, vendordb::RECORD_SIZE) != vendordb::RECORD_SIZE) return "";
    const uint32_t k = vendordb::keyOf(record);
    if (k == key) return vendordb::nameOf(record);
    if (k < key)
      lo = mid + 1;
    else
      hi = mid - 1;
  }
  return "";
}
