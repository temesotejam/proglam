#include "boat_protocol.h"

namespace boat {
uint32_t crc32(const uint8_t* data, size_t n) {
  uint32_t c = 0xffffffff;
  while (n--) {
    c ^= *data++;
    for (uint8_t i = 0; i < 8; ++i) c = (c >> 1) ^ ((c & 1) ? 0xedb88320 : 0);
  }
  return ~c;
}

static size_t cobs(const uint8_t* in, size_t n, uint8_t* out) {
  size_t r = 0, codePos = 0;
  uint8_t code = 1;
  out[r++] = 0;
  for (size_t i = 0; i < n; ++i) {
    if (in[i] == 0) {
      out[codePos] = code;
      code = 1;
      codePos = r++;
    } else {
      out[r++] = in[i];
      if (++code == 0xff) {
        out[codePos] = code;
        code = 1;
        codePos = r++;
      }
    }
  }
  out[codePos] = code;
  return r;
}

size_t encode(const Header& h, const uint8_t* payload, uint8_t* out, size_t cap) {
  if (h.length > kMaxPayload || cap < kMaxEncoded) return 0;
  uint8_t raw[kMaxRaw];
  memcpy(raw, &h, sizeof(h));
  if (h.length) memcpy(raw + sizeof(h), payload, h.length);
  const uint32_t crc = crc32(raw, sizeof(h) + h.length);
  memcpy(raw + sizeof(h) + h.length, &crc, sizeof(crc));
  const size_t n = cobs(raw, sizeof(h) + h.length + sizeof(crc), out);
  out[n] = 0;
  return n + 1;
}

bool Decoder::feed(uint8_t byte, Frame& frame) {
  if (byte) {
    if (n_ >= sizeof(encoded_)) {
      n_ = 0;
      ++lengthErrors;
    } else {
      encoded_[n_++] = byte;
    }
    return false;
  }
  if (!n_) return false;
  uint8_t raw[kMaxRaw];
  size_t r = 0;
  for (size_t i = 0; i < n_;) {
    const uint8_t code = encoded_[i++];
    if (!code || i + code - 1 > n_ + 1) {
      ++cobsErrors;
      n_ = 0;
      return false;
    }
    for (uint8_t j = 1; j < code && i < n_; ++j) raw[r++] = encoded_[i++];
    if (code < 0xff && i < n_) raw[r++] = 0;
  }
  n_ = 0;
  if (r < sizeof(Header) + sizeof(uint32_t)) {
    ++lengthErrors;
    return false;
  }
  memcpy(&frame.header, raw, sizeof(Header));
  if (frame.header.version != kVersion || frame.header.length > kMaxPayload ||
      r != sizeof(Header) + frame.header.length + sizeof(uint32_t)) {
    ++lengthErrors;
    return false;
  }
  uint32_t received = 0;
  memcpy(&received, raw + sizeof(Header) + frame.header.length, sizeof(received));
  if (received != crc32(raw, sizeof(Header) + frame.header.length)) {
    ++crcErrors;
    return false;
  }
  if (frame.header.length) memcpy(frame.payload, raw + sizeof(Header), frame.header.length);
  return true;
}
}  // namespace boat
