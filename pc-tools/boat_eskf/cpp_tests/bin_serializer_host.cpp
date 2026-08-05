#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <boat_protocol.h>
#include <bin_record_serializer.h>

int main() {
  const uint8_t payload[] = {0x10, 0x20, 0x30, 0x40};
  const uint8_t expected[] = {
    0x47,0x4F,0x4C,0x42,0x2E,0x16,0x00,0x00,0x00,0x00,0x00,0x00,
    0x01,0x43,0x04,0x00,0x09,0x00,0x00,0x00,0x07,0x00,0x00,0x00,
    0xD2,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0xAA,0x55,0x10,0x20,0x30,0x40};
  boat::Header header{boat::kVersion, static_cast<uint8_t>(boat::Type::WaypointAck), 4, 9, 7, 1234, 0x55AA};
  uint8_t bytes[boat_bin::kMaxRecordBytes]{}; size_t written = 0;
  assert(boat_bin::serializeRecord(header, 5678, payload, sizeof(payload), bytes, sizeof(bytes), written));
  assert(written == sizeof(expected));
  assert(std::memcmp(bytes, expected, sizeof(expected)) == 0);
  size_t rejected = 99;
  assert(!boat_bin::serializeRecord(header, 5678, payload, sizeof(payload), bytes, sizeof(expected)-1, rejected) && rejected == 0);
  boat::Header badLength = header; badLength.length = 3; rejected = 99;
  assert(!boat_bin::serializeRecord(badLength, 5678, payload, sizeof(payload), bytes, sizeof(bytes), rejected) && rejected == 0);
  uint8_t tooLong[boat::kMaxPayload + 1]{}; badLength = header; badLength.length = boat::kMaxPayload + 1; rejected = 99;
  assert(!boat_bin::serializeRecord(badLength, 5678, tooLong, sizeof(tooLong), bytes, sizeof(bytes), rejected) && rejected == 0);
  std::cout << "BIN_SERIALIZER_HOST_PASS golden=ok full_bytes=38 bounds=ok length=ok payload_limit=ok\n";
}
