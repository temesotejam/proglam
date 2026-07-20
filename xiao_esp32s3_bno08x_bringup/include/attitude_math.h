#pragma once

namespace attitude_math {

struct Quaternion {
  float x;
  float y;
  float z;
  float w;
};

struct EulerDegrees {
  float roll_deg;
  float pitch_deg;
  float yaw_deg;
};

// Intrinsic Z-Y-X (yaw-pitch-roll) Euler angles from an x,y,z,w quaternion.
// Returns false when the quaternion norm is zero or non-finite.
bool quaternionToEulerDegrees(const Quaternion& quaternion, EulerDegrees& euler);

}  // namespace attitude_math
