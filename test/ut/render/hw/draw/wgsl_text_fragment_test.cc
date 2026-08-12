// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/fragment/wgsl_text_fragment.hpp"

#include <gtest/gtest.h>
#include <wgsl_cross.h>

#include "skity/effect/shader.hpp"
#include "skity/graphic/color.hpp"
#include "src/render/hw/draw/geometry/wgsl_text_geometry.hpp"
#include "src/render/hw/draw/hw_wgsl_shader_writer.hpp"
#include "src/render/hw/draw/wgx_programmable_blending.hpp"

namespace skity {

std::string GenerateGradientTextFragmentWGSL(
    WGSLGradientTextFragment* fragment) {
  WGSLTextGradientGeometry geometry({}, {}, {}, {});
  return HWWGSLShaderWriter(&geometry, fragment).GenFSSourceWGSL();
}

TEST(WGSLTextFragmentTest, LinearGradient) {
  Color4f colors[3] = {Colors::kYellow, Colors::kRed, Colors::kBlue};
  float positions[3] = {0.0f, 0.5f, 1.0f};
  Point points[2] = {
      {80.0f, 100.0f, 0.0f, 1.0f},
      {320.0f, 100.0f, 0.0f, 1.0f},
  };
  auto shader = Shader::MakeLinear(points, colors, positions, 3);
  Shader::GradientInfo info;
  auto type = shader->AsGradient(&info);
  WGSLGradientTextFragment fragment({}, {}, info, type, 1.0f);

  auto program =
      wgx::Program::Parse(GenerateGradientTextFragmentWGSL(&fragment));
  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::MslOptions msl_options;
  EXPECT_TRUE(program->WriteToMsl("fs_main", msl_options).success);

  wgx::GlslOptions glsl_options;
  glsl_options.standard = wgx::GlslOptions::Standard::kDesktop;
  EXPECT_TRUE(program->WriteToGlsl("fs_main", glsl_options).success);
}

TEST(WGSLTextFragmentTest, RadialGradient) {
  Color4f colors[3] = {Colors::kYellow, Colors::kRed, Colors::kBlue};
  float positions[3] = {0.0f, 0.5f, 1.0f};
  auto shader = Shader::MakeRadial({400.0f, 250.0f, 0.0f, 1.0f}, 320.0f, colors,
                                   positions, 3);
  Shader::GradientInfo info;
  auto type = shader->AsGradient(&info);
  WGSLGradientTextFragment fragment({}, {}, info, type, 1.0f);

  auto program =
      wgx::Program::Parse(GenerateGradientTextFragmentWGSL(&fragment));
  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::MslOptions msl_options;
  EXPECT_TRUE(program->WriteToMsl("fs_main", msl_options).success);

  wgx::GlslOptions glsl_options;
  glsl_options.standard = wgx::GlslOptions::Standard::kDesktop;
  EXPECT_TRUE(program->WriteToGlsl("fs_main", glsl_options).success);
}

TEST(WGSLTextFragmentTest, ConicalGradient) {
  Color4f colors[3] = {Colors::kYellow, Colors::kRed, Colors::kBlue};
  float positions[3] = {0.0f, 0.5f, 1.0f};
  auto shader = Shader::MakeTwoPointConical({140.0f, 100.0f, 0.0f, 1.0f}, 0.0f,
                                            {220.0f, 100.0f, 0.0f, 1.0f},
                                            240.0f, colors, positions, 3);
  Shader::GradientInfo info;
  auto type = shader->AsGradient(&info);
  WGSLGradientTextFragment fragment({}, {}, info, type, 1.0f);

  auto program =
      wgx::Program::Parse(GenerateGradientTextFragmentWGSL(&fragment));
  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::MslOptions msl_options;
  EXPECT_TRUE(program->WriteToMsl("fs_main", msl_options).success);

  wgx::GlslOptions glsl_options;
  glsl_options.standard = wgx::GlslOptions::Standard::kDesktop;
  EXPECT_TRUE(program->WriteToGlsl("fs_main", glsl_options).success);
}

TEST(WGSLTextFragmentTest, SweepGradient) {
  Color4f colors[3] = {Colors::kYellow, Colors::kRed, Colors::kBlue};
  float positions[3] = {0.0f, 0.5f, 1.0f};
  auto shader =
      Shader::MakeSweep(400.0f, 250.0f, 0.0f, 360.0f, colors, positions, 3);
  Shader::GradientInfo info;
  auto type = shader->AsGradient(&info);
  WGSLGradientTextFragment fragment({}, {}, info, type, 1.0f);

  auto program =
      wgx::Program::Parse(GenerateGradientTextFragmentWGSL(&fragment));
  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::MslOptions msl_options;
  EXPECT_TRUE(program->WriteToMsl("fs_main", msl_options).success);

  wgx::GlslOptions glsl_options;
  glsl_options.standard = wgx::GlslOptions::Standard::kDesktop;
  EXPECT_TRUE(program->WriteToGlsl("fs_main", glsl_options).success);
}

TEST(WGSLTextFragmentTest, SeparatesColorTextCoverageForProgrammableBlend) {
  WGSLTextSolidColorGeometry geometry({}, {}, Paint{});
  WGSLColorTextFragment fragment({}, {});
  fragment.SetProgrammableBlending(WGXProgrammableBlending::Make(
      BlendMode::kSrc, DstReadStrategy::kTextureCopy));
  HWWGSLShaderWriter writer(&geometry, &fragment);
  auto source = writer.GenFSSourceWGSL();

  EXPECT_NE(source.find("color = vec4<f32>(input.v_color.rgb"),
            std::string::npos);
  EXPECT_NE(source.find("coverage = get_texture_color"), std::string::npos);
  EXPECT_NE(source.find("color = blending(color, dst_color, coverage)"),
            std::string::npos);

  auto program = wgx::Program::Parse(source);
  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());
  EXPECT_TRUE(program->WriteToMsl("fs_main", {}).success);
  EXPECT_TRUE(program->WriteToGlsl("fs_main", {}).success);
  EXPECT_TRUE(program->WriteToSpirv("fs_main", {}).success);
}

TEST(WGSLTextFragmentTest, SdfUsesDistanceAsCoverage) {
  WGSLTextSolidColorGeometry geometry({}, {}, Paint{});
  WGSLSdfColorTextFragment fragment({}, {}, Colors::kRed);
  HWWGSLShaderWriter writer(&geometry, &fragment);
  auto source = writer.GenFSSourceWGSL();

  EXPECT_NE(source.find("color = vec4<f32>(uColor.rgb"), std::string::npos);
  EXPECT_NE(source.find("coverage = smoothstep"), std::string::npos);

  auto program = wgx::Program::Parse(source);
  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());
  EXPECT_TRUE(program->WriteToMsl("fs_main", {}).success);
  EXPECT_TRUE(program->WriteToGlsl("fs_main", {}).success);
  EXPECT_TRUE(program->WriteToSpirv("fs_main", {}).success);
}

TEST(WGSLTextFragmentTest, EmojiColorHasNoFragmentMask) {
  WGSLTextSolidColorGeometry geometry({}, {}, Paint{});
  WGSLColorEmojiFragment fragment({}, {}, false, 1.f);
  HWWGSLShaderWriter writer(&geometry, &fragment);
  auto source = writer.GenFSSourceWGSL();

  EXPECT_EQ(source.find("var coverage"), std::string::npos);
  EXPECT_NE(source.find("color = get_texture_color"), std::string::npos);

  auto program = wgx::Program::Parse(source);
  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());
  EXPECT_TRUE(program->WriteToMsl("fs_main", {}).success);
  EXPECT_TRUE(program->WriteToGlsl("fs_main", {}).success);
  EXPECT_TRUE(program->WriteToSpirv("fs_main", {}).success);
}

}  // namespace skity
