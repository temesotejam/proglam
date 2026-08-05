#pragma once
#include <cassert>
#include <cstdint>
#include <fstream>
#include <boat_protocol.h>
#include <bin_record_serializer.h>
namespace boat_test {
inline void writeBinRecord(std::ofstream& out, boat::Type type, uint32_t& sequence, uint64_t timestamp, const void* payload, uint16_t length) {
  boat::Header header{boat::kVersion, static_cast<uint8_t>(type), length, ++sequence, 7, timestamp, 0};
  uint8_t bytes[boat_bin::kMaxRecordBytes]{}; size_t written = 0;
  assert(boat_bin::serializeRecord(header, timestamp, static_cast<const uint8_t*>(payload), length, bytes, sizeof(bytes), written));
  out.write(reinterpret_cast<const char*>(bytes), static_cast<std::streamsize>(written));
  assert(out.good());
}
}