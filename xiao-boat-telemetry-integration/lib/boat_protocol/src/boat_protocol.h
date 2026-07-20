#pragma once

#include <Arduino.h>

namespace boat {
constexpr uint8_t kVersion = 1;
constexpr size_t kMaxPayload = 768, kMaxRaw = 800, kMaxEncoded = 820;
enum class Type : uint8_t {
  Hello = 1, BnoAccel = 2, BnoGyro = 3, BnoQuaternion = 4, TofFrame = 5,
  InaSample = 6, VescStatus = 7, ActuatorState = 8, SystemHealth = 9,
  Event = 10, TimeSyncReply = 11, GnssRaw = 12, GnssFix = 13, GnssStatus = 14,
  Heartbeat = 32, Arm = 33, Disarm = 34, StartTest = 35, Stop = 36,
  Estop = 37, ClearEstop = 38,
};
struct __attribute__((packed)) Header {
  uint8_t version, type;
  uint16_t length;
  uint32_t sequence, bootId;
  uint64_t sourceUs;
  uint16_t flags;
};
struct Frame {
  Header header{};
  uint8_t payload[kMaxPayload]{};
};
uint32_t crc32(const uint8_t*, size_t);
size_t encode(const Header&, const uint8_t*, uint8_t*, size_t);
class Decoder {
 public:
  bool feed(uint8_t, Frame&);
  uint32_t crcErrors = 0, cobsErrors = 0, lengthErrors = 0;

 private:
  uint8_t encoded_[kMaxEncoded]{};
  size_t n_ = 0;
};
}  // namespace boat
