// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <wgsl_cross.h>

namespace {

TEST(WgxBackendNameResolutionTest, RewritesGlslConflictingVariableNames) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  let input: f32 = 1.0;
  return vec4<f32>(input, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::GlslOptions options;
  options.standard = wgx::GlslOptions::Standard::kDesktop;
  options.major_version = 3;
  options.minor_version = 3;

  auto result = program->WriteToGlsl("vs_main", options);
  ASSERT_TRUE(result.success);
  EXPECT_NE(result.content.find("wgx_symbol_"), std::string::npos);
  EXPECT_EQ(result.content.find("input_1"), std::string::npos);
}

TEST(WgxBackendNameResolutionTest, RewritesMslConflictingVariableNames) {
  auto program = wgx::Program::Parse(R"(
fn helper(vertex: f32) -> f32 {
  return vertex;
}

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  let value: f32 = helper(1.0);
  return vec4<f32>(value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::MslOptions options;
  auto result = program->WriteToMsl("vs_main", options);
  ASSERT_TRUE(result.success);
  EXPECT_NE(result.content.find("wgx_symbol_"), std::string::npos);
  EXPECT_EQ(result.content.find("vertex_1"), std::string::npos);
  EXPECT_NE(result.content.find("vs_main("), std::string::npos);
}

TEST(WgxBackendNameResolutionTest,
     RewritesCustomTypeNamesInFunctionParameters) {
  auto program = wgx::Program::Parse(R"(
struct VertexInput {
  @location(0) pos: vec2<f32>,
}

fn helper(input: VertexInput) -> f32 {
  return input.pos.x;
}

@vertex
fn vs_main(input: VertexInput) -> @builtin(position) vec4<f32> {
  let x: f32 = helper(input);
  return vec4<f32>(x, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::GlslOptions glsl_options;
  glsl_options.standard = wgx::GlslOptions::Standard::kDesktop;
  glsl_options.major_version = 3;
  glsl_options.minor_version = 3;

  auto glsl_result = program->WriteToGlsl("vs_main", glsl_options);
  ASSERT_TRUE(glsl_result.success);
  EXPECT_NE(glsl_result.content.find("wgx_symbol_"), std::string::npos);
  EXPECT_EQ(glsl_result.content.find("helper(VertexInput"), std::string::npos);
  EXPECT_EQ(glsl_result.content.find("vs_main(VertexInput"), std::string::npos);

  wgx::MslOptions msl_options;
  auto msl_result = program->WriteToMsl("vs_main", msl_options);
  ASSERT_TRUE(msl_result.success);
  EXPECT_NE(msl_result.content.find("wgx_symbol_"), std::string::npos);
  EXPECT_EQ(msl_result.content.find("helper(VertexInput"), std::string::npos);
  EXPECT_NE(msl_result.content.find("vs_main("), std::string::npos);
}

TEST(WgxBackendNameResolutionTest, ProducesDeterministicBackendOutput) {
  auto program = wgx::Program::Parse(R"(
struct Payload {
  value: f32,
}

fn helper(value: f32) -> f32 {
  let input: f32 = value;
  return input;
}

@vertex
fn vs_main(input: Payload) -> @builtin(position) vec4<f32> {
  let value: f32 = helper(input.value);
  return vec4<f32>(value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::GlslOptions glsl_options;
  glsl_options.standard = wgx::GlslOptions::Standard::kDesktop;
  glsl_options.major_version = 3;
  glsl_options.minor_version = 3;

  auto glsl_first = program->WriteToGlsl("vs_main", glsl_options);
  auto glsl_second = program->WriteToGlsl("vs_main", glsl_options);
  ASSERT_TRUE(glsl_first.success);
  ASSERT_TRUE(glsl_second.success);
  EXPECT_EQ(glsl_first.content, glsl_second.content);

  wgx::MslOptions msl_options;
  auto msl_first = program->WriteToMsl("vs_main", msl_options);
  auto msl_second = program->WriteToMsl("vs_main", msl_options);
  ASSERT_TRUE(msl_first.success);
  ASSERT_TRUE(msl_second.success);
  EXPECT_EQ(msl_first.content, msl_second.content);
}

TEST(WgxBackendNameResolutionTest,
     KeepsStructMemberAccessStableWhenNamesShadow) {
  auto program = wgx::Program::Parse(R"(
struct Data {
  value: f32,
}

fn helper(data: Data) -> f32 {
  let value: f32 = data.value;
  return value;
}

@vertex
fn vs_main(data: Data) -> @builtin(position) vec4<f32> {
  let value: f32 = helper(data);
  return vec4<f32>(value, data.value, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::GlslOptions glsl_options;
  glsl_options.standard = wgx::GlslOptions::Standard::kDesktop;
  glsl_options.major_version = 3;
  glsl_options.minor_version = 3;

  auto glsl_result = program->WriteToGlsl("vs_main", glsl_options);
  ASSERT_TRUE(glsl_result.success);
  EXPECT_NE(glsl_result.content.find(".value"), std::string::npos);
  EXPECT_NE(glsl_result.content.find("wgx_symbol_"), std::string::npos);

  wgx::MslOptions msl_options;
  auto msl_result = program->WriteToMsl("vs_main", msl_options);
  ASSERT_TRUE(msl_result.success);
  EXPECT_NE(msl_result.content.find(".value"), std::string::npos);
  EXPECT_NE(msl_result.content.find("wgx_symbol_"), std::string::npos);
}

TEST(WgxBackendNameResolutionTest,
     AvoidsInterfaceVariableCollisionWithLocalStructMemberNameInGlsl) {
  auto program = wgx::Program::Parse(R"(
struct VSIn {
  @location(0) input: vec2<f32>,
}

@vertex
fn vs_main(input: VSIn) -> @builtin(position) vec4<f32> {
  let input_2: vec2<f32> = input.input;
  return vec4<f32>(input_2, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::GlslOptions glsl_options;
  glsl_options.standard = wgx::GlslOptions::Standard::kDesktop;
  glsl_options.major_version = 3;
  glsl_options.minor_version = 3;

  auto glsl_result = program->WriteToGlsl("vs_main", glsl_options);
  ASSERT_TRUE(glsl_result.success);
  EXPECT_NE(glsl_result.content.find("in vec2 wgx_in_"), std::string::npos);
  EXPECT_EQ(glsl_result.content.find("in vec2 wgx_varying_0;"),
            std::string::npos);
  EXPECT_EQ(glsl_result.content.find("in vec2 input;"), std::string::npos);
  EXPECT_NE(glsl_result.content.find("wgx_symbol_"), std::string::npos);
}

TEST(WgxBackendNameResolutionTest,
     UsesConsistentVaryingNamesAcrossStagesInGlsl) {
  const char* source = R"(
struct VSOut {
  @builtin(position) position: vec4<f32>,
  @location(0) value: vec2<f32>,
}

@vertex
fn vs_main() -> VSOut {
  var out_data: VSOut;
  out_data.position = vec4<f32>(0.0, 0.0, 0.0, 1.0);
  out_data.value = vec2<f32>(1.0, 2.0);
  return out_data;
}

@fragment
fn fs_main(@location(0) value: vec2<f32>) -> @location(0) vec4<f32> {
  return vec4<f32>(value, 0.0, 1.0);
}
)";

  auto program = wgx::Program::Parse(source);
  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::GlslOptions glsl_options;
  glsl_options.standard = wgx::GlslOptions::Standard::kDesktop;
  glsl_options.major_version = 3;
  glsl_options.minor_version = 3;

  auto vs_glsl = program->WriteToGlsl("vs_main", glsl_options);
  ASSERT_TRUE(vs_glsl.success);
  EXPECT_NE(vs_glsl.content.find("out vec2 wgx_varying_0;"), std::string::npos);

  auto fs_glsl = program->WriteToGlsl("fs_main", glsl_options);
  ASSERT_TRUE(fs_glsl.success);
  EXPECT_NE(fs_glsl.content.find("in vec2 wgx_varying_0;"), std::string::npos);
}

}  // namespace
