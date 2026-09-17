// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gl/gpu_driver_info_gl.hpp"

#include <gtest/gtest.h>

namespace skity {
namespace {

TEST(GLDriverInfoTest, PreservesStringsAndIdentifiesMobileDriver) {
  const auto info =
      GLDriverInfo::FromStrings("Qualcomm", "Adreno (TM) 530", "OpenGL ES 3.2");
  EXPECT_EQ(info.vendor_name, "Qualcomm");
  EXPECT_EQ(info.renderer, "Adreno (TM) 530");
  EXPECT_EQ(info.version, "OpenGL ES 3.2");
  EXPECT_EQ(info.vendor, GLVendor::kQualcomm);
  EXPECT_EQ(info.renderer_family, GLRendererFamily::kAdreno);
  EXPECT_EQ(info.renderer_model, 530);

  const auto unknown = GLDriverInfo::FromStrings("", "", "");
  EXPECT_EQ(unknown.vendor, GLVendor::kUnknown);
  EXPECT_EQ(unknown.renderer_family, GLRendererFamily::kUnknown);
  EXPECT_EQ(unknown.renderer_model, -1);
}

TEST(GLDriverInfoTest, AppliesMobileDriverRules) {
  const struct {
    const char* vendor;
    const char* renderer;
    bool draw_clear;
    bool disable_fetch;
    bool disable_native;
  } cases[] = {
      {"Imagination Technologies", "PowerVR GE8320", true, false, false},
      {"Imagination Technologies", "PowerVR GE8100", true, false, false},
      {"Imagination Technologies", "PowerVR GM9446", true, false, false},
      {"Imagination Technologies", "PowerVR 7XTP-MT4", true, false, false},
      {"Imagination Technologies", "Unknown", false, false, false},
      {"Unknown", "PowerVR GE8320", true, false, false},
      {"ARM", "Mali-G76", false, false, true},
      {"ARM", "Mali-G77", false, false, true},
      {"arm", "mali-g78", false, false, false},
      {"Qualcomm", "Adreno (TM) 399", false, false, false},
      {"Qualcomm", "Adreno (TM) 400", false, false, true},
      {"Qualcomm", "Adreno (TM) 499", false, false, true},
      {"Qualcomm", "Adreno (TM) 500", false, true, true},
      {"Qualcomm", "Adreno (TM) 530", false, true, true},
      {"Qualcomm", "Adreno (TM) 599", false, true, true},
      {"Qualcomm", "Adreno (TM) 600", false, false, false},
      {"Qualcomm", "Adreno (TM) 640", false, false, false},
      {"freedreno", "FD530", false, true, true},
      {"freedreno", "FD640", false, false, false},
      {"QUALCOMM", "ADRENO (TM) 530", false, false, false},
      {"Unknown", "Unknown", false, false, false},
      {"", "", false, false, false},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.vendor);
    SCOPED_TRACE(test.renderer);
    auto workarounds = ResolveGLDriverWorkarounds(
        GLDriverInfo::FromStrings(test.vendor, test.renderer, ""));
    EXPECT_EQ(workarounds.use_draw_for_clear, test.draw_clear);
    EXPECT_EQ(workarounds.disable_framebuffer_fetch, test.disable_fetch);
    EXPECT_EQ(workarounds.disable_native_advanced_blend, test.disable_native);

    for (bool coherent : {false, true}) {
      GPUCaps caps;
      caps.supports_framebuffer_fetch = true;
      caps.supports_native_advanced_blend = true;
      caps.supports_native_advanced_blend_coherent = coherent;
      caps.native_blend_shader_variant = true;
      caps.supports_dual_source_blending = true;
      ApplyGLDriverWorkarounds(workarounds, caps);
      EXPECT_EQ(caps.supports_framebuffer_fetch, !test.disable_fetch);
      EXPECT_EQ(caps.supports_native_advanced_blend, !test.disable_native);
      EXPECT_EQ(caps.supports_native_advanced_blend_coherent,
                coherent && !test.disable_native);
      EXPECT_EQ(caps.native_blend_shader_variant, !test.disable_native);
      EXPECT_TRUE(caps.supports_dual_source_blending);
    }

    GPUCaps unsupported_caps;
    ApplyGLDriverWorkarounds(workarounds, unsupported_caps);
    EXPECT_FALSE(unsupported_caps.supports_framebuffer_fetch);
    EXPECT_FALSE(unsupported_caps.supports_native_advanced_blend);
    EXPECT_FALSE(unsupported_caps.supports_dual_source_blending);
  }
}

TEST(GLDriverInfoTest, RejectsMissingAndInvalidModels) {
  for (const char* renderer :
       {"", "FD", "Adreno (TM) ", "Adreno (TM) -530", "Adreno (TM) +530",
        "FD99999999999999999999", "Other GPU 530", "Other (Adreno 530)"}) {
    SCOPED_TRACE(renderer);
    auto workarounds = ResolveGLDriverWorkarounds(
        GLDriverInfo::FromStrings("Qualcomm", renderer, ""));
    EXPECT_FALSE(workarounds.use_draw_for_clear);
    EXPECT_FALSE(workarounds.disable_framebuffer_fetch);
    EXPECT_FALSE(workarounds.disable_native_advanced_blend);
  }
}

}  // namespace
}  // namespace skity
