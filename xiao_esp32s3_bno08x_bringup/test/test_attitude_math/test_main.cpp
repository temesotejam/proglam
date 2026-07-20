#include <cmath>

#include <unity.h>

#include "attitude_math.h"

namespace {
constexpr float kToleranceDeg = 0.001F;
constexpr float kSqrtHalf = 0.70710678118F;

void assertNear(float expected, float actual) {
  TEST_ASSERT_FLOAT_WITHIN(kToleranceDeg, expected, actual);
}

void test_identity_is_zero() {
  attitude_math::EulerDegrees euler{};
  TEST_ASSERT_TRUE(attitude_math::quaternionToEulerDegrees({0.0F, 0.0F, 0.0F, 1.0F}, euler));
  assertNear(0.0F, euler.roll_deg);
  assertNear(0.0F, euler.pitch_deg);
  assertNear(0.0F, euler.yaw_deg);
}

void test_positive_90_degree_roll() {
  attitude_math::EulerDegrees euler{};
  TEST_ASSERT_TRUE(attitude_math::quaternionToEulerDegrees({kSqrtHalf, 0.0F, 0.0F, kSqrtHalf}, euler));
  assertNear(90.0F, euler.roll_deg);
}

void test_positive_90_degree_pitch() {
  attitude_math::EulerDegrees euler{};
  TEST_ASSERT_TRUE(attitude_math::quaternionToEulerDegrees({0.0F, kSqrtHalf, 0.0F, kSqrtHalf}, euler));
  assertNear(90.0F, euler.pitch_deg);
}

void test_positive_90_degree_yaw() {
  attitude_math::EulerDegrees euler{};
  TEST_ASSERT_TRUE(attitude_math::quaternionToEulerDegrees({0.0F, 0.0F, kSqrtHalf, kSqrtHalf}, euler));
  assertNear(90.0F, euler.yaw_deg);
}

void test_non_normalized_quaternion_is_normalized() {
  attitude_math::EulerDegrees euler{};
  TEST_ASSERT_TRUE(attitude_math::quaternionToEulerDegrees({0.0F, 0.0F, 7.0F * kSqrtHalf, 7.0F * kSqrtHalf}, euler));
  assertNear(90.0F, euler.yaw_deg);
}

void test_asin_input_is_clamped_and_finite() {
  attitude_math::EulerDegrees euler{};
  TEST_ASSERT_TRUE(attitude_math::quaternionToEulerDegrees(
      {0.0F, 0.70710684F, 0.0F, 0.70710684F}, euler));
  TEST_ASSERT_TRUE(std::isfinite(euler.pitch_deg));
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 90.0F, euler.pitch_deg);
}
}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_identity_is_zero);
  RUN_TEST(test_positive_90_degree_roll);
  RUN_TEST(test_positive_90_degree_pitch);
  RUN_TEST(test_positive_90_degree_yaw);
  RUN_TEST(test_non_normalized_quaternion_is_normalized);
  RUN_TEST(test_asin_input_is_clamped_and_finite);
  return UNITY_END();
}
