#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <boat_protocol.h>
namespace boat_test {
constexpr uint32_t kBinMagic = 0x424C4F47UL;
inline void writeRecord(std::ofstream& out, boat::Type type, uint32_t& sequence, uint64_t timestamp, const void* payload, uint16_t length) {
  boat::Header header{boat::kVersion, static_cast<uint8_t>(type), length, ++sequence, 7, timestamp, 0};
  uint8_t encoded[boat::kMaxEncoded]{};
  const size_t encodedLength = boat::encode(header, static_cast<const uint8_t*>(payload), encoded, sizeof(encoded));
  assert(encodedLength > 0);
  boat::Decoder decoder; boat::Frame decoded{}; bool got = false;
  for (size_t i = 0; i < encodedLength; ++i) if (decoder.feed(encoded[i], decoded)) got = true;
  assert(got && decoder.crcErrors == 0 && decoder.cobsErrors == 0 && decoder.lengthErrors == 0);
  assert(decoded.header.version == header.version && decoded.header.type == header.type && decoded.header.length == header.length && decoded.header.sequence == header.sequence && decoded.header.bootId == header.bootId && decoded.header.sourceUs == header.sourceUs);
  assert(std::memcmp(decoded.payload, payload, length) == 0);
  out.write(reinterpret_cast<const char*>(&kBinMagic), sizeof(kBinMagic));
  out.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));
  out.write(reinterpret_cast<const char*>(&decoded.header), sizeof(decoded.header));
  out.write(reinterpret_cast<const char*>(decoded.payload), length);
  assert(out.good());
}
}
