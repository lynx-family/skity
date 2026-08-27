// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gl/gpu_driver_info_gl.hpp"

#include <gtest/gtest.h>

namespace skity {
namespace {

TEST(GLDriverInfoTest, DetectsAffectedPowerVRRenderers) {
  constexpr const char* kRenderers[] = {
      "PowerVR GE8320",
      "PowerVR GE8100",
      "PowerVR GM9446",
      "PowerVR 7XTP-MT4",
  };

  for (const auto* renderer : kRenderers) {
    auto info = GLDriverInfo::FromStrings("Imagination Technologies", renderer,
                                          "OpenGL ES 3.2");
    EXPECT_EQ(info.vendor, GLVendor::kPowerVR);
    EXPECT_TRUE(ResolveGLDriverWorkarounds(info).use_draw_for_clear);
  }
}

TEST(GLDriverInfoTest, DoesNotMatchOtherRenderers) {
  auto mali = GLDriverInfo::FromStrings("ARM", "Mali-G78", "OpenGL ES 3.2");
  auto angle = GLDriverInfo::FromStrings(
      "Google Inc.", "ANGLE (PowerVR GE8320)", "OpenGL ES 3.0");

  EXPECT_EQ(mali.vendor, GLVendor::kUnknown);
  EXPECT_EQ(angle.vendor, GLVendor::kUnknown);
  EXPECT_FALSE(ResolveGLDriverWorkarounds(mali).use_draw_for_clear);
  EXPECT_FALSE(ResolveGLDriverWorkarounds(angle).use_draw_for_clear);
}

}  // namespace
}  // namespace skity
