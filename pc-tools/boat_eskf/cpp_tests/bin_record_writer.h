#pragma once
#include <cassert>
#include <cstdint>
#include <fstream>
#include <boat_protocol.h>
#include <bin_record_serializer.h>

namespace boat_test {
struct TransportDiagnostics { uint64_t decodedFrames=0, crcErrors=0, cobsErrors=0, lengthErrors=0; };
inline void writeBinRecord(std::ofstream& out, TransportDiagnostics& diagnostics, boat::Type type, uint32_t& sequence, uint64_t timestamp, const void* payload, uint16_t length) {
  boat::Header header{boat::kVersion, static_cast<uint8_t>(type), length, ++sequence, 7, timestamp, 0};
  uint8_t encoded[boat::kMaxEncoded]{};
  const size_t encodedLength=boat::encode(header, static_cast<const uint8_t*>(payload), encoded, sizeof(encoded));
  assert(encodedLength>0);
  boat::Decoder decoder; boat::Frame decoded{}; bool accepted=false;
  for(size_t i=0;i<encodedLength;++i) if(decoder.feed(encoded[i],decoded)) accepted=true;
  diagnostics.crcErrors+=decoder.crcErrors; diagnostics.cobsErrors+=decoder.cobsErrors; diagnostics.lengthErrors+=decoder.lengthErrors;
  assert(accepted && decoder.crcErrors==0 && decoder.cobsErrors==0 && decoder.lengthErrors==0);
  ++diagnostics.decodedFrames;
  uint8_t bytes[boat_bin::kMaxRecordBytes]{}; size_t written=0;
  assert(boat_bin::serializeRecord(decoded.header, timestamp, decoded.payload, decoded.header.length, bytes, sizeof(bytes), written));
  out.write(reinterpret_cast<const char*>(bytes), static_cast<std::streamsize>(written));
  assert(out.good());
}
}
