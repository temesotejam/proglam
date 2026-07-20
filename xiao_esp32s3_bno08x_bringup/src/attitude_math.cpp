#include "attitude_math.h"

#include <cmath>

namespace {
constexpr float kRadToDeg = 57.29577951308232F;

float clampUnit(float value) {
  if (value > 1.0F) {
    return 1.0F;
  }
  if (value < -1.0F) {
    return -1.0F;
  }
  return value;
}
}  // namespace

namespace attitude_math {

bool quaternionToEulerDegrees(const Quaternion& quaternion, EulerDegrees& euler) {
  const float norm_sq = quaternion.x * quaternion.x + quaternion.y * quaternion.y +
                        quaternion.z * quaternion.z + quaternion.w * quaternion.w;
  if (!std::isfinite(norm_sq) || norm_sq <= 0.0F) {
    return false;
  }

  const float inverse_norm = 1.0F / std::sqrt(norm_sq);
  const float x = quaternion.x * inverse_norm;
  const float y = quaternion.y * inverse_norm;
  const float z = quaternion.z * inverse_norm;
  const float w = quaternion.w * inverse_norm;

  const float roll_rad = std::atan2(2.0F * (w * x + y * z),
                                    1.0F - 2.0F * (x * x + y * y));
  const float pitch_argument = clampUnit(2.0F * (w * y - z * x));
  const float pitch_rad = std::asin(pitch_argument);
  const float yaw_rad = std::atan2(2.0F * (w * z + x * y),
                                   1.0F - 2.0F * (y * y + z * z));

  euler.roll_deg = roll_rad * kRadToDeg;
  euler.pitch_deg = pitch_rad * kRadToDeg;
  euler.yaw_deg = yaw_rad * kRadToDeg;
  return true;
}

}  // namespace attitude_math
