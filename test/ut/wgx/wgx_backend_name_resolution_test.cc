// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <wgsl_cross.h>

#include <algorithm>

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

TEST(WgxBackendNameResolutionTest, LowersTextureLoadForGlslAndMsl) {
  auto program = wgx::Program::Parse(R"(
@group(0) @binding(0) var tex: texture_2d<f32>;

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let size: vec2<u32> = textureDimensions(tex);
  return textureLoad(tex, vec2<i32>(i32(size.x) - 1, 2), 0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::GlslOptions glsl_options;
  glsl_options.standard = wgx::GlslOptions::Standard::kDesktop;
  glsl_options.major_version = 3;
  glsl_options.minor_version = 3;
  auto glsl_result = program->WriteToGlsl("fs_main", glsl_options);
  ASSERT_TRUE(glsl_result.success);
  EXPECT_NE(glsl_result.content.find("texelFetch("), std::string::npos);
  EXPECT_NE(glsl_result.content.find("uvec2(textureSize("), std::string::npos);

  wgx::GlslOptions gles_options;
  gles_options.standard = wgx::GlslOptions::Standard::kES;
  gles_options.major_version = 3;
  gles_options.minor_version = 0;
  auto gles_result = program->WriteToGlsl("fs_main", gles_options);
  ASSERT_TRUE(gles_result.success);
  EXPECT_NE(gles_result.content.find("#version 300 es"), std::string::npos);
  EXPECT_NE(gles_result.content.find("precision highp float;"),
            std::string::npos);
  EXPECT_NE(gles_result.content.find("texelFetch("), std::string::npos);

  wgx::MslOptions msl_options;
  auto msl_result = program->WriteToMsl("fs_main", msl_options);
  ASSERT_TRUE(msl_result.success);
  EXPECT_NE(msl_result.content.find(".read(uint2("), std::string::npos);
}

TEST(WgxBackendNameResolutionTest, RejectsInvalidTextureBuiltinArity) {
  auto program = wgx::Program::Parse(R"(
@group(0) @binding(0) var tex: texture_2d<f32>;

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  return textureLoad(tex, vec2<i32>(0, 0));
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::GlslOptions glsl_options;
  EXPECT_FALSE(program->WriteToGlsl("fs_main", glsl_options).success);

  wgx::MslOptions msl_options;
  EXPECT_FALSE(program->WriteToMsl("fs_main", msl_options).success);
}

TEST(WgxBackendNameResolutionTest, LowersUnsignedIntegerTextureLoad) {
  auto program = wgx::Program::Parse(R"(
@group(0) @binding(0) var tex: texture_2d<u32>;

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let encoded: vec4<u32> = textureLoad(tex, vec2<i32>(0, 0), 0);
  return vec4<f32>(f32(encoded.x), f32(encoded.y),
                   f32(encoded.z), f32(encoded.w));
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::GlslOptions glsl_options;
  glsl_options.standard = wgx::GlslOptions::Standard::kDesktop;
  glsl_options.major_version = 3;
  glsl_options.minor_version = 3;
  auto glsl_result = program->WriteToGlsl("fs_main", glsl_options);
  ASSERT_TRUE(glsl_result.success);
  EXPECT_NE(glsl_result.content.find("uniform usampler2D"), std::string::npos);

  wgx::GlslOptions gles_options;
  gles_options.standard = wgx::GlslOptions::Standard::kES;
  gles_options.major_version = 3;
  auto gles_result = program->WriteToGlsl("fs_main", gles_options);
  ASSERT_TRUE(gles_result.success);
  EXPECT_NE(gles_result.content.find("highp usampler2D"), std::string::npos);

  wgx::MslOptions msl_options;
  auto msl_result = program->WriteToMsl("fs_main", msl_options);
  ASSERT_TRUE(msl_result.success);
  EXPECT_NE(msl_result.content.find("texture2d<uint>"), std::string::npos);
}

TEST(WgxBackendNameResolutionTest, LowersVectorComparisonsAndSelectForGlsl) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let f2_a: vec2<f32> = vec2<f32>(0.0, 1.0);
  let f2_b: vec2<f32> = vec2<f32>(1.0, 0.0);
  let f2_less: vec2<bool> = f2_a < f2_b;
  let f2_less_equal: vec2<bool> = f2_a <= f2_b;
  let f2_selected: vec2<f32> = select(f2_a, f2_b, f2_a < f2_b);
  let scalar_less: bool = f2_a.x < 17.0;
  let scalar_selected: f32 = select(0.0, 1.0, scalar_less);

  let i3_a: vec3<i32> = vec3<i32>(0, 1, 2);
  let i3_b: vec3<i32> = vec3<i32>(2, 1, 0);
  let i3_greater: vec3<bool> = i3_a > i3_b;
  let i3_greater_equal: vec3<bool> = i3_a >= i3_b;

  let u4_a: vec4<u32> =
      vec4<u32>(u32(0), u32(1), u32(2), u32(3));
  let u4_b: vec4<u32> =
      vec4<u32>(u32(3), u32(2), u32(1), u32(0));
  let u4_equal: vec4<bool> = u4_a == u4_b;
  let u4_not_equal: vec4<bool> = u4_a != u4_b;
  let selected: vec4<u32> = select(u4_a, u4_b, u4_equal);

  return vec4<f32>(f32(selected.x), scalar_selected, f2_selected.x, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::GlslOptions desktop_options;
  desktop_options.standard = wgx::GlslOptions::Standard::kDesktop;
  desktop_options.major_version = 3;
  desktop_options.minor_version = 3;
  auto desktop_result = program->WriteToGlsl("fs_main", desktop_options);
  ASSERT_TRUE(desktop_result.success);
  EXPECT_NE(desktop_result.content.find("lessThan("), std::string::npos);
  EXPECT_NE(desktop_result.content.find("lessThanEqual("), std::string::npos);
  EXPECT_NE(desktop_result.content.find("greaterThan("), std::string::npos);
  EXPECT_NE(desktop_result.content.find("greaterThanEqual("),
            std::string::npos);
  EXPECT_NE(desktop_result.content.find("equal("), std::string::npos);
  EXPECT_NE(desktop_result.content.find("notEqual("), std::string::npos);
  EXPECT_NE(desktop_result.content.find(" < 17.000000"), std::string::npos);
  EXPECT_NE(desktop_result.content.find(" ? 1.000000 : 0.000000"),
            std::string::npos);
  EXPECT_NE(desktop_result.content.find("uvec4 wgx_select("),
            std::string::npos);
  EXPECT_NE(desktop_result.content.find("vec2 wgx_select("), std::string::npos);
  EXPECT_EQ(desktop_result.content.find("ivec3 wgx_select("),
            std::string::npos);
  EXPECT_EQ(desktop_result.content.find("bool wgx_select("), std::string::npos);

  wgx::GlslOptions es_options;
  es_options.standard = wgx::GlslOptions::Standard::kES;
  es_options.major_version = 3;
  es_options.minor_version = 0;
  auto es_result = program->WriteToGlsl("fs_main", es_options);
  ASSERT_TRUE(es_result.success);
  EXPECT_NE(es_result.content.find("#version 300 es"), std::string::npos);
  EXPECT_NE(es_result.content.find("uvec4 wgx_select("), std::string::npos);
}

TEST(WgxBackendNameResolutionTest, LowersAny) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let flags: vec4<bool> = vec4<bool>(false, true, false, false);
  let value: f32 = select(0.0, 1.0, any(flags));
  return vec4<f32>(value);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::GlslOptions glsl_options;
  glsl_options.standard = wgx::GlslOptions::Standard::kDesktop;
  glsl_options.major_version = 3;
  glsl_options.minor_version = 3;
  auto glsl_result = program->WriteToGlsl("fs_main", glsl_options);
  ASSERT_TRUE(glsl_result.success);
  EXPECT_NE(glsl_result.content.find("any("), std::string::npos);

  wgx::MslOptions msl_options;
  auto msl_result = program->WriteToMsl("fs_main", msl_options);
  ASSERT_TRUE(msl_result.success);
  EXPECT_NE(msl_result.content.find("any("), std::string::npos);
}

TEST(WgxBackendNameResolutionTest, RejectsBooleanVectorComparison) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let lhs: vec2<bool> = vec2<bool>(true, false);
  let rhs: vec2<bool> = vec2<bool>(false, true);
  let compared: vec2<bool> = lhs == rhs;
  return vec4<f32>(0.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::GlslOptions glsl_options;
  EXPECT_FALSE(program->WriteToGlsl("fs_main", glsl_options).success);

  wgx::SpirvOptions spirv_options;
  EXPECT_FALSE(program->WriteToSpirv("fs_main", spirv_options).success);
}

TEST(WgxBackendNameResolutionTest,
     UsesKnownRhsVectorConstructorForIndirectLhs) {
  auto program = wgx::Program::Parse(R"(
struct Input {
  value: vec4<f32>,
}

fn compare(input: Input) -> vec4<bool> {
  return input.value < vec4<f32>(1.0);
}

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  var input: Input;
  input.value = vec4<f32>(0.0);
  let condition: vec4<bool> = compare(input);
  return select(vec4<f32>(0.0), vec4<f32>(1.0), condition);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::GlslOptions options;
  options.standard = wgx::GlslOptions::Standard::kDesktop;
  options.major_version = 3;
  options.minor_version = 3;
  auto result = program->WriteToGlsl("fs_main", options);
  ASSERT_TRUE(result.success);
  EXPECT_NE(result.content.find("lessThan("), std::string::npos);
  EXPECT_NE(result.content.find("vec4 wgx_select("), std::string::npos);
}

TEST(WgxBackendNameResolutionTest,
     PreservesScalarComparisonForUnknownFunctionReturnType) {
  auto program = wgx::Program::Parse(R"(
fn value() -> f32 {
  return 0.0;
}

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let compared: bool = value() < value();
  return vec4<f32>(0.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::GlslOptions options;
  options.standard = wgx::GlslOptions::Standard::kDesktop;
  options.major_version = 3;
  options.minor_version = 3;
  auto result = program->WriteToGlsl("fs_main", options);
  ASSERT_TRUE(result.success);
  EXPECT_NE(result.content.find(" < "), std::string::npos);
  EXPECT_EQ(result.content.find("lessThan("), std::string::npos);
}

TEST(WgxBackendNameResolutionTest,
     PreservesScalarSelectForUnknownFunctionReturnType) {
  auto program = wgx::Program::Parse(R"(
fn mask() -> bool {
  return true;
}

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let selected: f32 = select(0.0, 1.0, mask());
  return vec4<f32>(selected);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::GlslOptions options;
  options.standard = wgx::GlslOptions::Standard::kDesktop;
  options.major_version = 3;
  options.minor_version = 3;
  auto result = program->WriteToGlsl("fs_main", options);
  ASSERT_TRUE(result.success);
  EXPECT_NE(result.content.find(" ? 1.000000 : 0.000000"), std::string::npos);
  EXPECT_EQ(result.content.find("wgx_select("), std::string::npos);
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
  EXPECT_NE(msl_first.content.find("[[vertex]]"), std::string::npos);
  EXPECT_NE(msl_second.content.find("[[vertex]]"), std::string::npos);
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

TEST(WgxBackendNameResolutionTest, LowersFlatIntegerVaryings) {
  const char* source = R"(
struct VSInput {
  @location(0) unsigned_value: vec2<u32>,
  @location(1) signed_value: vec2<i32>,
}

struct StageOutput {
  @builtin(position) position: vec4<f32>,
  @location(0) @interpolate(flat) unsigned_value: vec2<u32>,
  @location(1) @interpolate(flat) signed_value: vec2<i32>,
}

@vertex
fn vs_main(input: VSInput) -> StageOutput {
  var output: StageOutput;
  output.position = vec4<f32>(0.0, 0.0, 0.0, 1.0);
  output.unsigned_value = input.unsigned_value;
  output.signed_value = input.signed_value;
  return output;
}

@fragment
fn fs_main(input: StageOutput) -> @location(0) vec4<f32> {
  return vec4<f32>(f32(input.unsigned_value.x),
                   f32(input.signed_value.x), 0.0, 1.0);
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
  auto fs_glsl = program->WriteToGlsl("fs_main", glsl_options);
  ASSERT_TRUE(vs_glsl.success);
  ASSERT_TRUE(fs_glsl.success);
  EXPECT_NE(vs_glsl.content.find("flat out uvec2"), std::string::npos);
  EXPECT_NE(vs_glsl.content.find("flat out ivec2"), std::string::npos);
  EXPECT_NE(fs_glsl.content.find("flat in uvec2"), std::string::npos);
  EXPECT_NE(fs_glsl.content.find("flat in ivec2"), std::string::npos);

  wgx::GlslOptions gles_options;
  gles_options.standard = wgx::GlslOptions::Standard::kES;
  gles_options.major_version = 3;
  gles_options.minor_version = 0;
  EXPECT_TRUE(program->WriteToGlsl("vs_main", gles_options).success);
  EXPECT_TRUE(program->WriteToGlsl("fs_main", gles_options).success);

  wgx::MslOptions msl_options;
  auto vs_msl = program->WriteToMsl("vs_main", msl_options);
  auto fs_msl = program->WriteToMsl("fs_main", msl_options);
  ASSERT_TRUE(vs_msl.success);
  ASSERT_TRUE(fs_msl.success);
  EXPECT_NE(vs_msl.content.find("uint2"), std::string::npos);
  EXPECT_NE(vs_msl.content.find("int2"), std::string::npos);
  EXPECT_NE(vs_msl.content.find("flat"), std::string::npos);
  EXPECT_NE(fs_msl.content.find("flat"), std::string::npos);
}

TEST(WgxBackendNameResolutionTest, UsesTextureSlotBindingInGlsl420) {
  auto program = wgx::Program::Parse(R"(
@group(0) @binding(1) var tex: texture_2d<f32>;
@group(0) @binding(2) var samp: sampler;

@fragment
fn fs_main(@location(0) uv: vec2<f32>) -> @location(0) vec4<f32> {
  return textureSample(tex, samp, uv);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::GlslOptions glsl_options;
  glsl_options.standard = wgx::GlslOptions::Standard::kDesktop;
  glsl_options.major_version = 4;
  glsl_options.minor_version = 2;

  auto glsl_result = program->WriteToGlsl("fs_main", glsl_options);
  ASSERT_TRUE(glsl_result.success);
  EXPECT_NE(glsl_result.content.find("layout ( binding = 0) uniform sampler2D"),
            std::string::npos);

  ASSERT_EQ(glsl_result.bind_groups.size(), 1u);
  auto* group = &glsl_result.bind_groups[0];
  auto* texture_entry = group->GetEntry(1);
  auto* sampler_entry = group->GetEntry(2);
  ASSERT_NE(texture_entry, nullptr);
  ASSERT_NE(sampler_entry, nullptr);
  EXPECT_EQ(texture_entry->index, 0u);
  ASSERT_TRUE(sampler_entry->units.has_value());
  ASSERT_EQ(sampler_entry->units->size(), 1u);
  EXPECT_EQ((*sampler_entry->units)[0], 0u);
}

TEST(WgxBackendNameResolutionTest,
     CollectsSamplerUnitsWithoutTextureSlotBinding) {
  auto program = wgx::Program::Parse(R"(
@group(0) @binding(1) var tex_a: texture_2d<f32>;
@group(0) @binding(3) var tex_b: texture_2d<f32>;
@group(0) @binding(2) var samp: sampler;

@fragment
fn fs_main(@location(0) uv: vec2<f32>) -> @location(0) vec4<f32> {
  return textureSample(tex_a, samp, uv) + textureSample(tex_b, samp, uv);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::GlslOptions glsl_options;
  glsl_options.standard = wgx::GlslOptions::Standard::kDesktop;
  glsl_options.major_version = 3;
  glsl_options.minor_version = 3;

  auto glsl_result = program->WriteToGlsl("fs_main", glsl_options);
  ASSERT_TRUE(glsl_result.success);
  EXPECT_EQ(glsl_result.content.find("layout ( binding = "), std::string::npos);

  ASSERT_EQ(glsl_result.bind_groups.size(), 1u);
  auto* group = &glsl_result.bind_groups[0];
  auto* tex_a_entry = group->GetEntry(1);
  auto* tex_b_entry = group->GetEntry(3);
  auto* sampler_entry = group->GetEntry(2);
  ASSERT_NE(tex_a_entry, nullptr);
  ASSERT_NE(tex_b_entry, nullptr);
  ASSERT_NE(sampler_entry, nullptr);
  EXPECT_EQ(tex_a_entry->index, 0u);
  EXPECT_EQ(tex_b_entry->index, 1u);

  ASSERT_TRUE(sampler_entry->units.has_value());
  const auto& units = *sampler_entry->units;
  EXPECT_EQ(units.size(), 2u);
  EXPECT_NE(std::find(units.begin(), units.end(), 0u), units.end());
  EXPECT_NE(std::find(units.begin(), units.end(), 1u), units.end());
}

}  // namespace
