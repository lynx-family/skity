// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/geometry/matrix.hpp>
#include <skity/geometry/quaternion.hpp>

#include "gtest/gtest.h"
#include "test/ut/common.hpp"

TEST(Quaternion, EulerToMatrix) {
  float alpha = glm::radians(0.f);
  float beta = glm::radians(275.f);
  float gamma = glm::radians(0.f);
  skity::Matrix m1 = skity::Quaternion::EulerToMatrix(alpha, beta, gamma);

  skity::Quaternion q = skity::Quaternion::FromEuler(alpha, beta, gamma);
  skity::Matrix m2 = q.ToMatrix();
  EXPECT_MATRIX_EQ(m1, m2);
}

TEST(Quaternion, Op) {
  {
    float alpha1 = glm::radians(345.f);
    float beta1 = glm::radians(32.f);
    float gamma1 = glm::radians(0.f);
    skity::Quaternion q1 = skity::Quaternion::FromEuler(alpha1, beta1, gamma1);
    const auto axis_angle = q1.ToAxisAngle();
    EXPECT_GE(axis_angle.second, glm::radians(180.f));

    float alpha2 = glm::radians(305.f);
    float beta2 = glm::radians(345.f);
    float gamma2 = glm::radians(0.f);
    skity::Quaternion q2 = skity::Quaternion::FromEuler(alpha2, beta2, gamma2);

    skity::Quaternion q1_to_q2 = q2 * q1.Reciprocal();
    const auto axis_angle2 = q1_to_q2.ToAxisAngle();
    EXPECT_GE(axis_angle2.second, glm::radians(180.f));
  }

  {
    skity::Vec3 axis{1, 1, 1};
    skity::Quaternion q1 =
        skity::Quaternion::FromAxisAngle(axis, glm::radians(20.f));
    skity::Quaternion q2 =
        skity::Quaternion::FromAxisAngle(axis, glm::radians(230.f));

    const auto angle = q1.IncludeAngle(q2);
    EXPECT_GE(angle, glm::radians(90.f));

    skity::Quaternion negative_q1 = q1.Negative();
    const auto angle2 = negative_q1.IncludeAngle(q2);
    EXPECT_LE(angle2, glm::radians(90.f));

    skity::Quaternion negative_q2 = q2.Negative();
    const auto angle3 = q1.IncludeAngle(negative_q2);
    EXPECT_LE(angle3, glm::radians(90.f));
  }

  {
    float alpha1 = glm::radians(45.f);
    float beta1 = glm::radians(0.f);
    float gamma1 = glm::radians(0.f);
    skity::Quaternion q1 = skity::Quaternion::FromEuler(alpha1, beta1, gamma1);

    float alpha2 = glm::radians(345.f);
    float beta2 = glm::radians(0.f);
    float gamma2 = glm::radians(0.f);
    skity::Quaternion q2 = skity::Quaternion::FromEuler(alpha2, beta2, gamma2);

    skity::Quaternion q1_to_q2 = q2 * q1.Reciprocal();
    const auto axis_angle2 = q1_to_q2.ToAxisAngle();
    EXPECT_TRUE(skity::FloatNearlyZero(glm::degrees(axis_angle2.second) - 300));

    skity::Quaternion q1_to_N_q2 = skity::Quaternion::FromAxisAngle(
        axis_angle2.first, axis_angle2.second - glm::radians(360.f));
    skity::Quaternion n_q2 = q1_to_N_q2 * q1;
    const auto axis_angle3 = q1_to_N_q2.ToAxisAngle();
    EXPECT_TRUE(skity::FloatNearlyZero(glm::degrees(axis_angle3.second) - 60));

    const auto axis_angle4 = n_q2.ToAxisAngle();
    EXPECT_TRUE(skity::FloatNearlyZero(glm::degrees(axis_angle4.second) - 15));

    float progress = 0.5f;
    skity::Quaternion p = n_q2.Slerp(q1, 1 - progress);
    auto progess_aixs_angle = p.ToAxisAngle();
    EXPECT_TRUE(
        skity::FloatNearlyZero(glm::degrees(progess_aixs_angle.second) - 15));
  }
}
