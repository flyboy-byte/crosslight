#include "PcapSink.h"

#include <Logging.h>

namespace wifiaudit {

bool PcapSink::open(const char* path) {
  close();
  if (!Storage.openFileForWrite("WIFIAUDIT", path, file)) {
    LOG_ERR("WIFIAUDIT", "Could not open %s for pcap capture", path);
    return false;
  }
  uint8_t header[PCAP_GLOBAL_HEADER_LEN];
  pcapWriteGlobalHeader(header, PCAP_LINKTYPE_IEEE802_11, PCAP_SNAPLEN);
  if (file.write(header, sizeof(header)) != sizeof(header)) {
    LOG_ERR("WIFIAUDIT", "Failed writing pcap header to %s", path);
    file.close();
    return false;
  }
  opened = true;
  frames = 0;
  bytes = sizeof(header);
  LOG_INF("WIFIAUDIT", "Capturing to %s", path);
  return true;
}

bool PcapSink::appendFrame(const uint8_t* data, const size_t len, const uint32_t origLen, const uint32_t tsSec,
                           const uint32_t tsUsec) {
  if (!opened || !data) return false;
  const uint32_t incl = len > PCAP_SNAPLEN ? PCAP_SNAPLEN : static_cast<uint32_t>(len);
  uint8_t record[PCAP_RECORD_HEADER_LEN];
  pcapWriteRecordHeader(record, tsSec, tsUsec, incl, origLen);
  if (file.write(record, sizeof(record)) != sizeof(record)) return false;
  if (file.write(data, incl) != incl) return false;
  frames++;
  bytes += sizeof(record) + incl;
  return true;
}

void PcapSink::close() {
  if (opened) {
    file.close();
    opened = false;
    LOG_INF("WIFIAUDIT", "Capture closed (%u frames, %u bytes)", static_cast<unsigned>(frames),
            static_cast<unsigned>(bytes));
  }
}

}  // namespace wifiaudit
