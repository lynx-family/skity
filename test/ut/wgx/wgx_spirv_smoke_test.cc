// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <wgsl_cross.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "spirv/unified1/spirv.h"

namespace {

#if defined(WGX_VULKAN)
bool ContainsInstruction(const std::vector<uint32_t>& words, SpvOp opcode) {
  size_t offset = 5u;
  while (offset < words.size()) {
    const uint32_t inst = words[offset];
    const uint16_t word_count =
        static_cast<uint16_t>(inst >> SpvWordCountShift);
    const auto current_opcode = static_cast<SpvOp>(inst & SpvOpCodeMask);

    if (word_count == 0u) {
      return false;
    }

    if (current_opcode == opcode) {
      return true;
    }

    offset += word_count;
  }

  return false;
}

bool ContainsExecutionMode(const std::vector<uint32_t>& words,
                           SpvExecutionMode mode) {
  size_t offset = 5u;
  while (offset < words.size()) {
    const uint32_t inst = words[offset];
    const uint16_t word_count =
        static_cast<uint16_t>(inst >> SpvWordCountShift);
    const auto opcode = static_cast<SpvOp>(inst & SpvOpCodeMask);

    if (word_count == 0u) {
      return false;
    }

    if (opcode == SpvOpExecutionMode && word_count >= 3u &&
        words[offset + 2u] == static_cast<uint32_t>(mode)) {
      return true;
    }

    offset += word_count;
  }

  return false;
}

bool ContainsBuiltInDecoration(const std::vector<uint32_t>& words,
                               SpvBuiltIn builtin) {
  size_t offset = 5u;
  while (offset < words.size()) {
    const uint32_t inst = words[offset];
    const uint16_t word_count =
        static_cast<uint16_t>(inst >> SpvWordCountShift);
    const auto opcode = static_cast<SpvOp>(inst & SpvOpCodeMask);

    if (word_count == 0u) {
      return false;
    }

    if (opcode == SpvOpDecorate && word_count >= 4u &&
        words[offset + 2u] == static_cast<uint32_t>(SpvDecorationBuiltIn) &&
        words[offset + 3u] == static_cast<uint32_t>(builtin)) {
      return true;
    }

    offset += word_count;
  }

  return false;
}

bool ContainsDecoration(const std::vector<uint32_t>& words,
                        SpvDecoration decoration) {
  size_t offset = 5u;
  while (offset < words.size()) {
    const uint32_t inst = words[offset];
    const uint16_t word_count =
        static_cast<uint16_t>(inst >> SpvWordCountShift);
    const auto opcode = static_cast<SpvOp>(inst & SpvOpCodeMask);

    if (word_count == 0u) {
      return false;
    }

    if (opcode == SpvOpDecorate && word_count >= 3u &&
        words[offset + 2u] == static_cast<uint32_t>(decoration)) {
      return true;
    }

    offset += word_count;
  }

  return false;
}

bool ContainsVariableWithStorageClass(const std::vector<uint32_t>& words,
                                      SpvStorageClass storage_class) {
  size_t offset = 5u;
  while (offset < words.size()) {
    const uint32_t inst = words[offset];
    const uint16_t word_count =
        static_cast<uint16_t>(inst >> SpvWordCountShift);
    const auto opcode = static_cast<SpvOp>(inst & SpvOpCodeMask);

    if (word_count == 0u) {
      return false;
    }

    if (opcode == SpvOpVariable && word_count >= 4u &&
        words[offset + 3u] == static_cast<uint32_t>(storage_class)) {
      return true;
    }

    offset += word_count;
  }

  return false;
}

bool ContainsCapability(const std::vector<uint32_t>& words,
                        SpvCapability capability) {
  size_t offset = 5u;
  while (offset < words.size()) {
    const uint32_t inst = words[offset];
    const uint16_t word_count =
        static_cast<uint16_t>(inst >> SpvWordCountShift);
    const auto opcode = static_cast<SpvOp>(inst & SpvOpCodeMask);

    if (word_count == 0u) {
      return false;
    }

    if (opcode == SpvOpCapability && word_count >= 2u &&
        words[offset + 1u] == static_cast<uint32_t>(capability)) {
      return true;
    }

    offset += word_count;
  }

  return false;
}

void DumpSpirvBinary(const std::string& filename,
                     const std::vector<uint32_t>& words) {
  if (words.empty()) {
    return;
  }

  namespace fs = std::filesystem;
  const char* env_dir = std::getenv("WGX_SPIRV_DUMP_DIR");
  fs::path dir;
  if (env_dir != nullptr && env_dir[0] != '\0') {
    dir = fs::path(env_dir);
  } else {
#if defined(WGX_SPIRV_DUMP_DEFAULT_DIR)
    dir = fs::path(WGX_SPIRV_DUMP_DEFAULT_DIR);
#else
    dir = fs::current_path() / "out" / "cmake_host_build";
#endif
  }
  std::error_code ec;
  fs::create_directories(dir, ec);

  const fs::path path = dir / filename;

  std::ofstream output(path, std::ios::binary);
  if (!output.is_open()) {
    return;
  }

  output.write(reinterpret_cast<const char*>(words.data()),
               static_cast<std::streamsize>(words.size() * sizeof(uint32_t)));
}

TEST(WgxSpirvSmokeTest, EmitsMinimalVertexSpirvBinaryForValidEntryPoint) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() {
  return;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);
  DumpSpirvBinary("wgx_vs_main_minimal.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_EQ(words[1], 0x00010300u);
  EXPECT_EQ(words[3], 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpEntryPoint));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFunction));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpReturn));
  EXPECT_FALSE(ContainsExecutionMode(words, SpvExecutionModeOriginUpperLeft));
}

TEST(WgxSpirvSmokeTest, EmitsFragmentExecutionModeForFragmentEntryPoint) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() {
  return;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_minimal.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpEntryPoint));
  EXPECT_TRUE(ContainsExecutionMode(words, SpvExecutionModeOriginUpperLeft));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForBuiltinPositionVec4Return) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_position.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpEntryPoint));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypeVector));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypePointer));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpVariable));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConstant));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConstantComposite));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForLocalVariableReturn) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var pos: vec4<f32> = vec4<f32>(0.0, 0.0, 0.0, 1.0);
  return pos;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_var_return.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpEntryPoint));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypeVector));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypePointer));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpVariable));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConstant));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConstantComposite));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsTextureDimensionsBuiltinForTextureHandle) {
  auto program = wgx::Program::Parse(R"(
@group(0) @binding(0) var tex: texture_2d<f32>;

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  let dims: vec2<u32> = textureDimensions(tex);
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_texture_dimensions.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypeImage));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpImageQuerySizeLod));
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationDescriptorSet));
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationBinding));
  EXPECT_TRUE(ContainsCapability(words, SpvCapabilityImageQuery));
}

TEST(WgxSpirvSmokeTest, EmitsTextureSampleBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@group(0) @binding(0) var tex: texture_2d<f32>;
@group(0) @binding(1) var samp: sampler;

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  return textureSample(tex, samp, vec2<f32>(0.5, 0.5));
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_texture_sample.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsExecutionMode(words, SpvExecutionModeOriginUpperLeft));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypeImage));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypeSampler));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypeSampledImage));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpSampledImage));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpImageSampleImplicitLod));
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationDescriptorSet));
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationBinding));
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationLocation));
}

TEST(WgxSpirvSmokeTest,
     EmitsTextureSampleBuiltinForFragmentShaderWithLocationInput) {
  auto program = wgx::Program::Parse(R"(
@group(0) @binding(0) var tex: texture_2d<f32>;
@group(0) @binding(1) var samp: sampler;

@fragment
fn fs_main(@location(0) uv: vec2<f32>) -> @location(0) vec4<f32> {
  return textureSample(tex, samp, uv);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_texture_sample_input.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsExecutionMode(words, SpvExecutionModeOriginUpperLeft));
  EXPECT_TRUE(ContainsVariableWithStorageClass(words, SpvStorageClassInput));
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationLocation));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpSampledImage));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpImageSampleImplicitLod));
}

TEST(WgxSpirvSmokeTest, EmitsTextureSampleLevelBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@group(0) @binding(0) var tex: texture_2d<f32>;
@group(0) @binding(1) var samp: sampler;

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  return textureSampleLevel(tex, samp, vec2<f32>(0.5, 0.5), 0.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_texture_sample_level.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypeSampledImage));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpSampledImage));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpImageSampleExplicitLod));
}

TEST(WgxSpirvSmokeTest, EmitsTextureLoadBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@group(0) @binding(0) var tex: texture_2d<f32>;

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  return textureLoad(tex, vec2<i32>(0, 0), 0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_texture_load.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypeImage));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpImageFetch));
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationDescriptorSet));
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationBinding));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForScalarConstantStore) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var x: f32 = 1.0;
  var y: f32 = x;
  return vec4<f32>(1.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConstant));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForI32ConstantStore) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var x: i32 = 42;
  var y: i32 = x;
  return vec4<f32>(1.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConstant));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForBoolConstantStore) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var b: bool = true;
  var c: bool = b;
  return vec4<f32>(1.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConstantTrue));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest,
     EmitsVertexSpirvBinaryForShadowedAssignmentUsingSemanticBindings) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var pos: vec4<f32> = vec4<f32>(1.0, 0.0, 0.0, 1.0);
  {
    var pos: vec4<f32> = vec4<f32>(0.0, 1.0, 0.0, 1.0);
    pos = vec4<f32>(0.0, 0.0, 1.0, 1.0);
  }
  return pos;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForAssignedLocalVariableReturn) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var pos: vec4<f32>;
  pos = vec4<f32>(0.0, 0.0, 0.0, 1.0);
  return pos;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_var_assign_return.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpEntryPoint));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypeVector));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypePointer));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpVariable));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConstant));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConstantComposite));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForVariableCopyAssignmentReturn) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var a: vec4<f32> = vec4<f32>(0.0, 0.0, 0.0, 1.0);
  var b: vec4<f32>;
  b = a;
  return b;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_var_copy_assign_return.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpEntryPoint));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypeVector));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypePointer));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpVariable));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConstant));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConstantComposite));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForVectorAddReturn) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var a: vec4<f32> = vec4<f32>(0.0, 0.0, 0.0, 0.5);
  var b: vec4<f32> = vec4<f32>(0.0, 0.0, 0.0, 0.5);
  return a + b;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_vec_add_return.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFAdd));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForVectorSubAssignmentReturn) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var a: vec4<f32> = vec4<f32>(0.0, 0.0, 0.0, 1.0);
  var b: vec4<f32> = vec4<f32>(0.0, 0.0, 0.0, 0.25);
  var c: vec4<f32>;
  c = a - b;
  return c;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_vec_sub_assign_return.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFSub));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

/**
 * Test scope stack: nested block with variable shadowing
 * Verifies that inner block's variable doesn't leak to outer scope
 */
TEST(WgxSpirvSmokeTest, HandlesNestedBlockScope) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var pos: vec4<f32> = vec4<f32>(1.0, 0.0, 0.0, 1.0);
  {
    var inner: vec4<f32> = vec4<f32>(0.0, 1.0, 0.0, 1.0);
    pos = inner;
  }
  return pos;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_nested_scope.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  // Should have 2 local variables (pos and inner)
  EXPECT_TRUE(ContainsInstruction(words, SpvOpVariable));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

/**
 * Test scope stack: variable shadowing in nested block
 * Inner variable shadows outer variable with same name
 */
TEST(WgxSpirvSmokeTest, HandlesVariableShadowingInNestedBlock) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var x: vec4<f32> = vec4<f32>(1.0, 0.0, 0.0, 1.0);
  {
    var x: vec4<f32> = vec4<f32>(0.0, 1.0, 0.0, 1.0);
  }
  return x;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_shadowing.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

/**
 * Test global variable reference.
 * Global variables should be resolvable from entry point.
 */
TEST(WgxSpirvSmokeTest, EmitsSpirvWithGlobalVariableReference) {
  auto program = wgx::Program::Parse(R"(
var<private> global_pos: vec4<f32>;

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  return global_pos;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_global_var.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

/**
 * Test global variable with constant initializer.
 * The initializer should be emitted as OpVariable's initializer operand.
 */
TEST(WgxSpirvSmokeTest, EmitsSpirvWithGlobalVariableInitializer) {
  auto program = wgx::Program::Parse(R"(
var<private> global_pos: vec4<f32> = vec4<f32>(1.0, 2.0, 3.0, 4.0);

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  return global_pos;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_global_init.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  // Should have OpConstantComposite for the vector initializer
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConstantComposite));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

/**
 * Test global i32 variable with constant initializer.
 */
TEST(WgxSpirvSmokeTest, EmitsSpirvWithGlobalI32Initializer) {
  auto program = wgx::Program::Parse(R"(
var<private> global_i: i32 = 42;

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var x: i32 = global_i;
  return vec4<f32>(1.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConstant));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpVariable));
}

/**
 * Test global u32 variable with constant initializer.
 */
TEST(WgxSpirvSmokeTest, EmitsSpirvWithGlobalU32Initializer) {
  auto program = wgx::Program::Parse(R"(
var<private> global_u: u32 = 7;

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var x: u32 = global_u;
  return vec4<f32>(1.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConstant));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpVariable));
}

/**
 * Test global bool variable with constant initializer.
 */
TEST(WgxSpirvSmokeTest, EmitsSpirvWithGlobalBoolInitializer) {
  auto program = wgx::Program::Parse(R"(
var<private> global_b: bool = true;

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var b: bool = global_b;
  return vec4<f32>(1.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConstantTrue));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpVariable));
}

/**
 * Test global vec2<i32> variable with constant initializer.
 */
TEST(WgxSpirvSmokeTest, EmitsSpirvWithGlobalVec2I32Initializer) {
  auto program = wgx::Program::Parse(R"(
var<private> global_v2i: vec2<i32> = vec2<i32>(1, 2);

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var x: vec2<i32> = global_v2i;
  return vec4<f32>(1.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConstantComposite));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpVariable));
}

/**
 * Test global variable with local shadowing.
 * Local variable should shadow global variable with same name.
 */
TEST(WgxSpirvSmokeTest, HandlesGlobalVariableShadowedByLocal) {
  auto program = wgx::Program::Parse(R"(
var<private> pos: vec4<f32> = vec4<f32>(1.0, 0.0, 0.0, 1.0);

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var pos: vec4<f32> = vec4<f32>(0.0, 1.0, 0.0, 1.0);
  return pos;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_global_shadowed.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

/**
 * Test uniform global variable with resource binding.
 * Uniform variables should have DescriptorSet and Binding decorations.
 *
 * Non-struct uniform types are automatically wrapped in a struct with Block
 * decoration and proper offset layout to comply with Vulkan SPIR-V
 * requirements.
 */
TEST(WgxSpirvSmokeTest, EmitsSpirvWithUniformGlobalAndBinding) {
  auto program = wgx::Program::Parse(R"(
@group(0) @binding(1)
var<uniform> u_data: vec4<f32>;

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  return u_data;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_uniform_binding.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationDescriptorSet));
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationBinding));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

/**
 * Test storage buffer global variable with resource binding.
 * Storage variables should have DescriptorSet and Binding decorations.
 *
 * Non-struct storage types are automatically wrapped in a struct with Block
 * decoration and proper offset layout to comply with Vulkan SPIR-V
 * requirements.
 */
TEST(WgxSpirvSmokeTest, EmitsSpirvWithStorageGlobalAndBinding) {
  auto program = wgx::Program::Parse(R"(
@group(0) @binding(0)
var<storage> s_data: vec4<f32>;

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  return s_data;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_storage_binding.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationDescriptorSet));
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationBinding));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

/**
 * Test workgroup global variable.
 * Workgroup variables are shared within workgroup and don't need binding.
 *
 * Note: Workgroup storage class is only valid in compute shaders (or mesh/task
 * shaders). This is a vertex shader test to verify storage class propagation.
 * Real-world usage would be in a compute shader.
 */
TEST(WgxSpirvSmokeTest, EmitsSpirvWithWorkgroupGlobal) {
  auto program = wgx::Program::Parse(R"(
var<workgroup> w_data: vec4<f32>;

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  return w_data;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_workgroup.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

/**
 * Test uniform vec3<f32> with std140 layout.
 * In std140, vec3 has 16-byte alignment and 16-byte size (rounded up from 12).
 * The generated SPIR-V should have Offset 0 and valid Block decoration.
 */
TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForIfElseControlFlow) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var pos: vec4<f32>;
  if (true) {
    pos = vec4<f32>(1.0, 0.0, 0.0, 1.0);
  } else {
    pos = vec4<f32>(0.0, 1.0, 0.0, 1.0);
  }
  return pos;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  if (result.success) {
    DumpSpirvBinary("wgx_vs_main_if_else.spv", result.spirv);
  }

  ASSERT_TRUE(result.success);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpSelectionMerge));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpBranchConditional));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpBranch));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

/**
 * Test if-else control flow with boolean variable as condition.
 * The condition should be properly loaded from the variable.
 */
TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForIfElseWithVariableCondition) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var pos: vec4<f32>;
  var use_red: bool = true;
  if (use_red) {
    pos = vec4<f32>(1.0, 0.0, 0.0, 1.0);
  } else {
    pos = vec4<f32>(0.0, 1.0, 0.0, 1.0);
  }
  return pos;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  if (result.success) {
    DumpSpirvBinary("wgx_vs_main_if_else_var_cond.spv", result.spirv);
  }

  ASSERT_TRUE(result.success);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpSelectionMerge));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpBranchConditional));
  EXPECT_TRUE(
      ContainsInstruction(words, SpvOpLoad));  // Should load the bool variable
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForIfWithF32Comparison) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var pos: vec4<f32> = vec4<f32>(0.0, 1.0, 0.0, 1.0);
  var x: f32 = 0.25;
  if (x < 1.0) {
    pos = vec4<f32>(1.0, 0.0, 0.0, 1.0);
  }
  return pos;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  if (result.success) {
    DumpSpirvBinary("wgx_vs_main_f32_compare_if.spv", result.spirv);
  }

  ASSERT_TRUE(result.success);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFOrdLessThan));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpSelectionMerge));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpBranchConditional));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForBoolFromI32Equality) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var pos: vec4<f32> = vec4<f32>(0.0, 1.0, 0.0, 1.0);
  var use_red: bool = 1 == 1;
  if (use_red) {
    pos = vec4<f32>(1.0, 0.0, 0.0, 1.0);
  }
  return pos;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  if (result.success) {
    DumpSpirvBinary("wgx_vs_main_i32_equal_bool.spv", result.spirv);
  }

  ASSERT_TRUE(result.success);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpIEqual));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest,
     EmitsVertexSpirvBinaryForSwitchWithMultipleSelectorsAndDefault) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var pos: vec4<f32> = vec4<f32>(0.0, 0.0, 1.0, 1.0);
  var mode: i32 = 2;
  switch mode {
    case 0: {
      pos = vec4<f32>(0.0, 1.0, 0.0, 1.0);
    }
    case 1, 2: {
      pos = vec4<f32>(1.0, 0.0, 0.0, 1.0);
    }
    default: {
      pos = vec4<f32>(1.0, 1.0, 0.0, 1.0);
    }
  }
  return pos;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  if (result.success) {
    DumpSpirvBinary("wgx_vs_main_switch_multi_default.spv", result.spirv);
  }

  ASSERT_TRUE(result.success);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpIEqual));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpSelectionMerge));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpBranchConditional));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpBranch));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForLoopWithBreak) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var pos: vec4<f32> = vec4<f32>(0.0, 0.0, 0.0, 1.0);
  loop {
    pos = vec4<f32>(1.0, 0.0, 0.0, 1.0);
    break;
  }
  return pos;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  if (result.success) {
    DumpSpirvBinary("wgx_vs_main_loop_break.spv", result.spirv);
  }

  ASSERT_TRUE(result.success);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoopMerge));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpBranch));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForLoopWithContinue) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var pos: vec4<f32> = vec4<f32>(0.0, 0.0, 0.0, 1.0);
  var done: bool = false;
  loop {
    if (done) {
      break;
    }
    done = true;
    continue;
  }
  return pos;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  if (result.success) {
    DumpSpirvBinary("wgx_vs_main_loop_continue.spv", result.spirv);
  }

  ASSERT_TRUE(result.success);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoopMerge));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpSelectionMerge));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpBranchConditional));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpBranch));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForCountedI32Loop) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var pos: vec4<f32> = vec4<f32>(0.0, 0.0, 0.0, 1.0);
  var i: i32 = 0;
  loop {
    if (i >= 2) {
      break;
    }
    i = i + 1;
    continue;
  }
  return pos;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  if (result.success) {
    DumpSpirvBinary("wgx_vs_main_i32_counted_loop.spv", result.spirv);
  }

  ASSERT_TRUE(result.success);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoopMerge));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpSGreaterThanEqual));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpIAdd));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpBranchConditional));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForLoopWithBreakIfContinuing) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var pos: vec4<f32> = vec4<f32>(0.0, 0.0, 0.0, 1.0);
  var done: bool = false;
  loop {
    pos = vec4<f32>(1.0, 0.0, 0.0, 1.0);
    continuing {
      done = true;
      break if done;
    }
  }
  return pos;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  if (result.success) {
    DumpSpirvBinary("wgx_vs_main_loop_break_if.spv", result.spirv);
  }

  ASSERT_TRUE(result.success);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoopMerge));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpBranchConditional));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForForLoop) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var pos: vec4<f32> = vec4<f32>(0.0, 0.0, 0.0, 1.0);
  for (var i: i32 = 0; i < 3; i++) {
    pos = vec4<f32>(1.0, 0.0, 0.0, 1.0);
  }
  return pos;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  if (result.success) {
    DumpSpirvBinary("wgx_vs_main_for_loop.spv", result.spirv);
  }

  ASSERT_TRUE(result.success);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoopMerge));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpSLessThan));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpIAdd));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpBranchConditional));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForWhileLoop) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var pos: vec4<f32> = vec4<f32>(0.0, 0.0, 0.0, 1.0);
  var i: i32 = 0;
  while (i < 2) {
    i = i + 1;
    pos = vec4<f32>(1.0, 0.0, 0.0, 1.0);
  }
  return pos;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  if (result.success) {
    DumpSpirvBinary("wgx_vs_main_while_loop.spv", result.spirv);
  }

  ASSERT_TRUE(result.success);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoopMerge));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpSLessThan));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpIAdd));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpBranchConditional));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsSpirvWithFunctionParameter) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32) -> @builtin(position) vec4<f32> {
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_param.spv", result.spirv);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsVariableWithStorageClass(words, SpvStorageClassInput));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInVertexIndex));
  EXPECT_FALSE(ContainsInstruction(words, SpvOpFunctionParameter));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForLocationInput) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main(@location(0) pos: vec2<f32>) -> @builtin(position) vec4<f32> {
  var local_pos: vec2<f32> = pos;
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_location_input.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsVariableWithStorageClass(words, SpvStorageClassInput));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationLocation));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
}

/**
 * Test function parameter with simple type (f32).
 */
TEST(WgxSpirvSmokeTest, EmitsSpirvWithF32FunctionParameter) {
  auto program = wgx::Program::Parse(R"(
fn helper(offset: f32) -> f32 {
  return offset + 1.0;
}

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  if (helper(0.5) > 1.0) {
    return vec4<f32>(1.0, 0.0, 0.0, 1.0);
  }
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_helper_call.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFunctionCall));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFunctionParameter));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFAdd));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFOrdGreaterThan));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpReturnValue));
}

TEST(WgxSpirvSmokeTest, EmitsSpirvWithDynamicVectorConstructor) {
  auto program = wgx::Program::Parse(R"(
fn helper(offset: f32) -> f32 {
  return offset + 1.0;
}

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  let x: f32 = helper(0.5);
  return vec4<f32>(x, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_dynamic_vec_ctor.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFunctionCall));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeConstruct));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpReturnValue));
}

#endif

}  // namespace
