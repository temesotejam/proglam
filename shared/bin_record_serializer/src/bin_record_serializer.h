#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <boat_protocol.h>

namespace boat_bin {
constexpr uint32_t kMagic = 0x424C4F47UL;
constexpr size_t kPrefixBytes = sizeof(uint32_t) + sizeof(uint64_t) + sizeof(boat::Header);
constexpr size_t kMaxRecordBytes = kPrefixBytes + boat::kMaxPayload;
inline void putLe16(uint8_t* dst, uint16_t value) { dst[0] = static_cast<uint8_t>(value); dst[1] = static_cast<uint8_t>(value >> 8); }
inline void putLe32(uint8_t* dst, uint32_t value) { for (uint8_t i = 0; i < 4; ++i) dst[i] = static_cast<uint8_t>(value >> (8 * i)); }
inline void putLe64(uint8_t* dst, uint64_t value) { for (uint8_t i = 0; i < 8; ++i) dst[i] = static_cast<uint8_t>(value >> (8 * i)); }
// Canonical BIN record: [magic][received/control timestamp][protocol Header][payload].
// Storage-agnostic: callers provide a fixed output buffer and select the sink.
inline bool serializeRecord(const boat::Header& header, uint64_t receivedUs, const uint8_t* payload,
                            uint16_t payloadLength, uint8_t* output, size_t capacity, size_t& written) {
  written = 0;
  if (!output || (!payload && payloadLength) || payloadLength != header.length || payloadLength > boat::kMaxPayload || capacity < kPrefixBytes + payloadLength) return false;
  size_t offset = 0;
  putLe32(output + offset, kMagic); offset += sizeof(uint32_t);
  putLe64(output + offset, receivedUs); offset += sizeof(uint64_t);
  output[offset++] = header.version; output[offset++] = header.type;
  putLe16(output + offset, header.length); offset += sizeof(uint16_t);
  putLe32(output + offset, header.sequence); offset += sizeof(uint32_t);
  putLe32(output + offset, header.bootId); offset += sizeof(uint32_t);
  putLe64(output + offset, header.sourceUs); offset += sizeof(uint64_t);
  putLe16(output + offset, header.flags); offset += sizeof(uint16_t);
  if (payloadLength) memcpy(output + offset, payload, payloadLength);
  offset += payloadLength; written = offset; return true;
}
}  // namespace boat_bin
