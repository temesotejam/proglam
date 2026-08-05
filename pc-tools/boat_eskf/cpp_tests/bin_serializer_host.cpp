#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <boat_protocol.h>
#include <bin_record_serializer.h>

int main() {
  const uint8_t payload[] = {0x10, 0x20, 0x30, 0x40};
  boat::Header header{boat::kVersion, static_cast<uint8_t>(boat::Type::WaypointAck), 4, 9, 7, 1234, 0x55AA};
  uint8_t bytes[boat_bin::kMaxRecordBytes]{}; size_t written = 0;
  assert(boat_bin::serializeRecord(header, 5678, payload, sizeof(payload), bytes, sizeof(bytes), written));
  assert(written == boat_bin::kPrefixBytes + sizeof(payload));
  assert(bytes[0] == 'G' && bytes[1] == 'O' && bytes[2] == 'L' && bytes[3] == 'B');
  uint64_t received = 0; memcpy(&received, bytes + 4, sizeof(received)); assert(received == 5678);
  assert(bytes[12] == 1 && bytes[13] == 67 && bytes[14] == 4 && bytes[15] == 0);
  assert(memcmp(bytes + boat_bin::kPrefixBytes, payload, sizeof(payload)) == 0);
  size_t rejected = 0; assert(!boat_bin::serializeRecord(header, 5678, payload, sizeof(payload), bytes, boat_bin::kPrefixBytes, rejected));
  std::cout << "BIN_SERIALIZER_HOST_PASS golden=ok little_endian=ok bounds=ok bytes=" << written << "\n";
}
