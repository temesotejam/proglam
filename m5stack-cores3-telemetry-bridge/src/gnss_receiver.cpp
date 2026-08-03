#include "gnss_receiver.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace gnss {
namespace {
bool equal(const char* a, const char* b) { return strcmp(a, b) == 0; }
bool nonEmpty(const char* text) { return text && *text; }
float knotsToMps(double knots) { return static_cast<float>(knots * 0.514444); }
}

void Receiver::begin(HardwareSerial& serial) {
  serial.begin(app_config::kGnssBaud, SERIAL_8N1, app_config::kGnssRxPin,
               app_config::kGnssTxPin);
}

void Receiver::count(uint32_t Counts::*field) {
  total_.*field += 1;
  window_.*field += 1;
}

bool Receiver::checksum(const char* raw) {
  if (!raw || raw[0] != '$') return false;
  const char* star = strchr(raw, '*');
  if (!star || !isxdigit(star[1]) || !isxdigit(star[2])) return false;
  uint8_t value = 0;
  for (const char* p = raw + 1; p < star; ++p) value ^= static_cast<uint8_t>(*p);
  char given[3] = {star[1], star[2], 0};
  return value == static_cast<uint8_t>(strtoul(given, nullptr, 16));
}

bool Receiver::number(const char* text, double& value) {
  if (!nonEmpty(text)) return false;
  char* end = nullptr;
  value = strtod(text, &end);
  return end && *end == '\0';
}

bool Receiver::coord(const char* text, const char* hemisphere, bool latitude,
                     double& value) {
  double raw = 0;
  if (!number(text, raw) || !nonEmpty(hemisphere)) return false;
  const double degrees = floor(raw / 100.0);
  value = degrees + (raw - degrees * 100.0) / 60.0;
  const char h = toupper(hemisphere[0]);
  if ((latitude && h == 'S') || (!latitude && h == 'W')) value = -value;
  return (latitude && (h == 'N' || h == 'S')) ||
         (!latitude && (h == 'E' || h == 'W'));
}

void Receiver::typeOf(const char* raw, char* type) {
  type[0] = 0;
  if (!raw || raw[0] != '$') return;
  const char* p = raw + 1;
  const size_t n = strlen(p);
  if (n < 5) return;
  strncpy(type, p + 2, 5);
  type[5] = 0;
}

bool Receiver::parse(char* raw, uint64_t parseUs, Sentence& sentence) {
  char* star = strchr(raw, '*');
  if (star) *star = 0;
  char* fields[24]{};
  uint8_t fieldCount = 0;
  for (char* token = strtok(raw + 1, ","); token && fieldCount < 24;
       token = strtok(nullptr, ",")) {
    fields[fieldCount++] = token;
  }
  if (!fieldCount) return false;
  char kind[7]{};
  const size_t idLength = strlen(fields[0]);
  if (idLength < 5) return false;
  strncpy(kind, fields[0] + idLength - 3, 3);
  kind[3] = 0;
  bool changed = false;
  double value = 0;

  if (equal(kind, "RMC") && fieldCount >= 10) {
    if (nonEmpty(fields[1])) { strncpy(latest_.utcTime, fields[1], sizeof(latest_.utcTime)-1); latest_.flags |= UtcTimeValid; changed = true; }
    if (nonEmpty(fields[9])) { strncpy(latest_.utcDate, fields[9], sizeof(latest_.utcDate)-1); latest_.flags |= UtcDateValid; changed = true; }
    const bool valid = equal(fields[2], "A");
    if (valid) latest_.flags |= FixValid; else latest_.flags &= ~FixValid;
    double coordinate = 0;
    if (coord(fields[3], fields[4], true, coordinate)) { latest_.latitude = coordinate; latest_.flags |= LatitudeValid; changed = true; }
    if (coord(fields[5], fields[6], false, coordinate)) { latest_.longitude = coordinate; latest_.flags |= LongitudeValid; changed = true; }
    if (number(fields[7], value)) { latest_.speedMps = knotsToMps(value); latest_.flags |= SpeedValid; changed = true; }
    if (number(fields[8], value)) { latest_.courseDeg = static_cast<float>(value); latest_.flags |= CourseValid; changed = true; }
    if (valid && (latest_.flags & LatitudeValid) && (latest_.flags & LongitudeValid)) latest_.lastValidFixUs = parseUs;
  } else if (equal(kind, "GGA") && fieldCount >= 10) {
    if (nonEmpty(fields[1])) { strncpy(latest_.utcTime, fields[1], sizeof(latest_.utcTime)-1); latest_.flags |= UtcTimeValid; changed = true; }
    if (number(fields[2], value)) { /* Coordinate is parsed below. */ }
    double coordinate = 0;
    if (coord(fields[2], fields[3], true, coordinate)) { latest_.latitude = coordinate; latest_.flags |= LatitudeValid; changed = true; }
    if (coord(fields[4], fields[5], false, coordinate)) { latest_.longitude = coordinate; latest_.flags |= LongitudeValid; changed = true; }
    if (number(fields[6], value)) { latest_.fixType = static_cast<uint8_t>(value); latest_.flags |= FixTypeValid; if (value > 0) latest_.flags |= FixValid; changed = true; }
    if (number(fields[7], value)) { latest_.satellites = static_cast<uint8_t>(value); latest_.flags |= SatellitesValid; changed = true; }
    if (number(fields[8], value)) { latest_.hdop = static_cast<float>(value); latest_.flags |= HdopValid; changed = true; }
    if (number(fields[9], value)) { latest_.altitudeM = static_cast<float>(value); latest_.flags |= AltitudeValid; changed = true; }
    if ((latest_.flags & FixValid) && (latest_.flags & LatitudeValid) && (latest_.flags & LongitudeValid)) latest_.lastValidFixUs = parseUs;
  } else if (equal(kind, "VTG") && fieldCount >= 8) {
    if (number(fields[1], value)) { latest_.courseDeg = static_cast<float>(value); latest_.flags |= CourseValid; changed = true; }
    if (number(fields[5], value)) { latest_.speedMps = knotsToMps(value); latest_.flags |= SpeedValid; changed = true; }
  } else if (equal(kind, "GSA") && fieldCount >= 3) {
    if (number(fields[2], value)) { latest_.fixType = static_cast<uint8_t>(value); latest_.flags |= FixTypeValid; changed = true; }
    if (fieldCount >= 17 && number(fields[16], value)) { latest_.hdop = static_cast<float>(value); latest_.flags |= HdopValid; changed = true; }
  } else if (equal(kind, "ZDA") && fieldCount >= 5) {
    if (nonEmpty(fields[1])) { strncpy(latest_.utcTime, fields[1], sizeof(latest_.utcTime)-1); latest_.flags |= UtcTimeValid; changed = true; }
    if (nonEmpty(fields[2]) && nonEmpty(fields[3]) && nonEmpty(fields[4])) { snprintf(latest_.utcDate, sizeof(latest_.utcDate), "%02u%02u%02u", atoi(fields[2]), atoi(fields[3]), atoi(fields[4]) % 100); latest_.flags |= UtcDateValid; changed = true; }
  }
  latest_.lastParseUs = parseUs;
  return changed;
}

bool Receiver::finish(uint64_t endUs, Sentence& sentence) {
  if (!collecting_) return false;
  collecting_ = false;
  if (overlong_) { count(&Counts::overlong); return false; }
  line_[len_] = 0;
  if (!len_) return false;
  count(&Counts::sentences);
  sentence = Sentence{};
  strncpy(sentence.raw, line_, sizeof(sentence.raw)-1);
  sentence.rawLength = static_cast<uint8_t>(min<size_t>(len_, 255));
  typeOf(line_, sentence.type);
  sentence.endUs = endUs;
  sentence.checksumValid = checksum(line_);
  if (!sentence.checksumValid) { count(&Counts::checksumErrors); return true; }
  char copy[app_config::kGnssInputLineChars + 1]{};
  strncpy(copy, line_, sizeof(copy)-1);
  sentence.parseUs = endUs;
  sentence.parsed = parse(copy, endUs, sentence);
  if (sentence.parsed) count(&Counts::validSentences);
  else count(&Counts::parseErrors);
  strncpy(latest_.lastType, sentence.type, sizeof(latest_.lastType)-1);
  latest_.lastSentenceEndUs = endUs;
  return true;
}

bool Receiver::feed(char c, uint64_t timestampUs, Sentence& sentence) {
  count(&Counts::bytes);
  if (c == '$') { collecting_ = true; overlong_ = false; len_ = 0; startedUs_ = timestampUs; line_[len_++] = c; return false; }
  if (!collecting_) return false;
  if (c == '\r' || c == '\n') return finish(timestampUs, sentence);
  if (len_ >= app_config::kGnssInputLineChars) { overlong_ = true; return false; }
  line_[len_++] = c;
  return false;
}

void Receiver::expire(uint64_t timestampUs) {
  if (collecting_ && timestampUs - startedUs_ > app_config::kGnssSentenceTimeoutMs * 1000ULL) {
    collecting_ = false; len_ = 0; count(&Counts::unfinished);
  }
}

Counts Receiver::takeWindow() { Counts snapshot = window_; window_ = Counts{}; return snapshot; }
void Receiver::noteLogDrop() { count(&Counts::logDrops); }
bool Receiver::receiving(uint64_t timestampUs) const {
  return latest_.lastSentenceEndUs && timestampUs - latest_.lastSentenceEndUs < app_config::kGnssNoDataTimeoutMs * 1000ULL;
}
}  // namespace gnss
