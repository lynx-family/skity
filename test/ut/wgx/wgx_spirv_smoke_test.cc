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
#include <unordered_map>
#include <vector>

#include "spirv/unified1/spirv.h"
#include "src/render/hw/draw/fragment/wgsl_gradient_fragment.hpp"
#include "src/render/hw/draw/fragment/wgsl_text_fragment.hpp"
#include "src/render/hw/draw/geometry/wgsl_rrect_geometry.hpp"
#include "src/render/hw/draw/hw_wgsl_shader_writer.hpp"

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

bool ContainsMemberDecoration(const std::vector<uint32_t>& words,
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

    if (opcode == SpvOpMemberDecorate && word_count >= 4u &&
        words[offset + 3u] == static_cast<uint32_t>(decoration)) {
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

bool ContainsStoreToStorageClass(const std::vector<uint32_t>& words,
                                 SpvStorageClass storage_class) {
  std::unordered_map<uint32_t, SpvStorageClass> variable_storage_classes;

  size_t offset = 5u;
  while (offset < words.size()) {
    const uint32_t inst = words[offset];
    const uint16_t word_count =
        static_cast<uint16_t>(inst >> SpvWordCountShift);
    const auto opcode = static_cast<SpvOp>(inst & SpvOpCodeMask);

    if (word_count == 0u) {
      return false;
    }

    if (opcode == SpvOpVariable && word_count >= 4u) {
      variable_storage_classes.emplace(
          words[offset + 2u], static_cast<SpvStorageClass>(words[offset + 3u]));
    }

    offset += word_count;
  }

  offset = 5u;
  while (offset < words.size()) {
    const uint32_t inst = words[offset];
    const uint16_t word_count =
        static_cast<uint16_t>(inst >> SpvWordCountShift);
    const auto opcode = static_cast<SpvOp>(inst & SpvOpCodeMask);

    if (word_count == 0u) {
      return false;
    }

    if (opcode == SpvOpStore && word_count >= 3u) {
      auto it = variable_storage_classes.find(words[offset + 1u]);
      if (it != variable_storage_classes.end() && it->second == storage_class) {
        return true;
      }
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

TEST(WgxSpirvSmokeTest, EmitsDotBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: f32 = dot(vec2<f32>(1.0, 2.0), vec2<f32>(3.0, 4.0));
  return vec4<f32>(value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_dot.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsExecutionMode(words, SpvExecutionModeOriginUpperLeft));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpDot));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpReturn));
}

TEST(WgxSpirvSmokeTest, EmitsDistanceBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: f32 = distance(vec2<f32>(1.0, 2.0), vec2<f32>(3.0, 4.0));
  return vec4<f32>(value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_distance.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFSub));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpDot));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInstImport));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInst));
}

TEST(WgxSpirvSmokeTest, EmitsAbsBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: vec2<f32> = abs(vec2<f32>(-1.0, -2.0));
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_abs.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInstImport));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInst));
}

TEST(WgxSpirvSmokeTest, EmitsSignBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: vec2<f32> = sign(vec2<f32>(-1.0, 2.0));
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_sign.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInstImport));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInst));
}

TEST(WgxSpirvSmokeTest, EmitsMaxBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: vec2<f32> = max(vec2<f32>(1.0, 4.0), vec2<f32>(2.0, 3.0));
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_max.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInstImport));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInst));
}

TEST(WgxSpirvSmokeTest, EmitsMinBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: vec2<f32> = min(vec2<f32>(1.0, 4.0), vec2<f32>(2.0, 3.0));
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_min.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInstImport));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInst));
}

TEST(WgxSpirvSmokeTest, EmitsClampBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: f32 = clamp(1.5, 0.0, 1.0);
  return vec4<f32>(value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_clamp.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInstImport));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInst));
}

TEST(WgxSpirvSmokeTest, EmitsFloorBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: f32 = floor(1.5);
  return vec4<f32>(value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_floor.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInstImport));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInst));
}

TEST(WgxSpirvSmokeTest, EmitsFractBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: f32 = fract(1.5);
  return vec4<f32>(value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_fract.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInstImport));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInst));
}

TEST(WgxSpirvSmokeTest, EmitsCeilBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: f32 = ceil(1.5);
  return vec4<f32>(value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_ceil.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInstImport));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInst));
}

TEST(WgxSpirvSmokeTest, EmitsRoundBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: f32 = round(1.5);
  return vec4<f32>(value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_round.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInstImport));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInst));
}

TEST(WgxSpirvSmokeTest, EmitsStepBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: vec2<f32> = step(0.5, vec2<f32>(0.25, 0.75));
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_step.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInstImport));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInst));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeConstruct));
}

TEST(WgxSpirvSmokeTest, EmitsSmoothStepBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: f32 = smoothstep(-1.0, 1.0, 0.25);
  return vec4<f32>(value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_smoothstep.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInstImport));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInst));
}

TEST(WgxSpirvSmokeTest, EmitsMixBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: vec2<f32> = mix(vec2<f32>(0.0, 1.0), vec2<f32>(2.0, 3.0), 0.5);
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_mix.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInstImport));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInst));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeConstruct));
}

TEST(WgxSpirvSmokeTest, EmitsSelectBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: f32 = select(0.0, 1.0, true);
  return vec4<f32>(value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_select.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpSelect));
}

TEST(WgxSpirvSmokeTest, EmitsVectorMaskSelectBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let mask: vec2<bool> = vec2<bool>(true, false);
  let value: vec2<f32> = select(vec2<f32>(0.0, 1.0), vec2<f32>(2.0, 3.0), mask);
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_select_vec_mask.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypeVector));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpSelect));
}

TEST(WgxSpirvSmokeTest, EmitsAtanBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: f32 = atan(-1.0, 1.0);
  return vec4<f32>(value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_atan.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInstImport));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInst));
}

TEST(WgxSpirvSmokeTest, EmitsPowBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: f32 = pow(4.0, 0.5);
  return vec4<f32>(value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_pow.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInstImport));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInst));
}

TEST(WgxSpirvSmokeTest, EmitsExpBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: f32 = exp(1.0);
  return vec4<f32>(value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_exp.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInstImport));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInst));
}

TEST(WgxSpirvSmokeTest, EmitsSinBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: f32 = sin(1.0);
  return vec4<f32>(value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_sin.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInstImport));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInst));
}

TEST(WgxSpirvSmokeTest, EmitsCosBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: f32 = cos(1.0);
  return vec4<f32>(value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_cos.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInstImport));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInst));
}

TEST(WgxSpirvSmokeTest, EmitsLengthBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: f32 = length(vec2<f32>(3.0, 4.0));
  return vec4<f32>(value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_length.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInstImport));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInst));
}

TEST(WgxSpirvSmokeTest, EmitsNormalizeBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: vec2<f32> = normalize(vec2<f32>(3.0, 4.0));
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_normalize.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInstImport));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInst));
}

TEST(WgxSpirvSmokeTest, EmitsInverseSqrtBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: f32 = inversesqrt(4.0);
  return vec4<f32>(value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_inversesqrt.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInstImport));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInst));
}

TEST(WgxSpirvSmokeTest, EmitsSqrtBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  let value: f32 = sqrt(4.0);
  return vec4<f32>(value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_sqrt.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInstImport));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpExtInst));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpReturn));
}

TEST(WgxSpirvSmokeTest, EmitsScalarCastFromU32ToF32) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32) -> @builtin(position) vec4<f32> {
  let index_f32: f32 = f32(vertex_index);
  return vec4<f32>(index_f32, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_cast_u32_to_f32.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConvertUToF));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
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

TEST(WgxSpirvSmokeTest, EmitsDFdxBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main(@location(0) value: f32) -> @location(0) vec4<f32> {
  let grad: f32 = dFdx(value);
  return vec4<f32>(grad, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_dfdx.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsExecutionMode(words, SpvExecutionModeOriginUpperLeft));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpDPdx));
}

TEST(WgxSpirvSmokeTest, EmitsDFdyBuiltinForFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@fragment
fn fs_main(@location(0) uv: vec2<f32>) -> @location(0) vec4<f32> {
  let grad: vec2<f32> = dFdy(uv);
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_dfdy.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsExecutionMode(words, SpvExecutionModeOriginUpperLeft));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpDPdy));
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

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForVectorScalarMulReturn) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var color: vec4<f32> = vec4<f32>(0.0, 0.0, 0.0, 0.5);
  return color * 2.0;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_vec_scalar_mul_return.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeConstruct));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFMul));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForVectorMulReturn) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var a: vec4<f32> = vec4<f32>(0.0, 0.0, 0.0, 0.5);
  var b: vec4<f32> = vec4<f32>(1.0, 1.0, 1.0, 2.0);
  return a * b;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_vec_mul_return.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFMul));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForScalarDivideReturn) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var w: f32 = 1.0 / 2.0;
  return vec4<f32>(0.0, 0.0, 0.0, w);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_scalar_div_return.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFDiv));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsScalarCastFromF32ToI32) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var value: f32 = 1.5;
  var index: i32 = i32(value);
  return vec4<f32>(f32(index), 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_cast_f32_to_i32.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConvertFToS));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConvertSToF));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForVectorDivideReturn) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var a: vec4<f32> = vec4<f32>(0.0, 0.0, 0.0, 1.0);
  var b: vec4<f32> = vec4<f32>(1.0, 1.0, 1.0, 2.0);
  return a / b;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_vec_div_return.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFDiv));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForUnsignedShiftLeftReturn) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var base: u32 = u32(1);
  var value: u32 = base << u32(3);
  return vec4<f32>(f32(value), 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_u32_shift_left_return.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpShiftLeftLogical));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConvertUToF));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForSignedShiftRightReturn) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var base: i32 = 8;
  var value: i32 = base >> 1;
  return vec4<f32>(f32(value), 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_i32_shift_right_return.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpShiftRightArithmetic));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConvertSToF));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForBitwiseAndReturn) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var value: i32 = 13 & 7;
  return vec4<f32>(f32(value), 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_bitwise_and_return.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpBitwiseAnd));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConvertSToF));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForBitwiseOrReturn) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var base: u32 = u32(4);
  var value: u32 = base | u32(1);
  return vec4<f32>(f32(value), 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_bitwise_or_return.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpBitwiseOr));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConvertUToF));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForBitwiseXorReturn) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var lhs: u32 = u32(6);
  var rhs: u32 = u32(3);
  var value: u32 = lhs ^ rhs;
  return vec4<f32>(f32(value), 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_bitwise_xor_return.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpBitwiseXor));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpConvertUToF));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForLogicalAndReturn) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var value: bool = (1.0 < 2.0) && (3.0 > 2.0);
  return vec4<f32>(select(0.0, 1.0, value), 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_logical_and_return.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLogicalAnd));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpSelect));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForLogicalOrReturn) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var value: bool = (1.0 > 2.0) || (3.0 > 2.0);
  return vec4<f32>(select(0.0, 1.0, value), 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_logical_or_return.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLogicalOr));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpSelect));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvUnaryNotReturn) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var value: bool = !(1.0 > 2.0);
  return vec4<f32>(select(0.0, 1.0, value), 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_unary_not_return.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLogicalEqual));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpSelect));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvUnaryNegationScalarReturn) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var input: f32 = 2.5;
  var value: f32 = -input;
  return vec4<f32>(value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_unary_neg_scalar_return.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFSub));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvUnaryNegationVectorReturn) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var value: vec2<f32> = -vec2<f32>(1.0, 2.0);
  return vec4<f32>(value[0], value[1], 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_unary_neg_vector_return.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeConstruct));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFSub));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvDynamicVectorIndexReturn) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var values: vec4<f32> = vec4<f32>(1.0, 2.0, 3.0, 4.0);
  var idx: i32 = 2;
  var value: f32 = values[idx];
  return vec4<f32>(value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_vec_dynamic_index_return.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpAccessChain));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvMatrixIndexReturn) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var m: mat2x2<f32> = mat2x2<f32>(1.0, 2.0, 3.0, 4.0);
  var col: vec2<f32> = m[1];
  return vec4<f32>(col[0], col[1], 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_mat_index_return.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeExtract));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvArrayIndexReturn) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var points: array<vec2<f32>, 4> = array<vec2<f32>, 4>(
      vec2<f32>(1.0, 2.0),
      vec2<f32>(3.0, 4.0),
      vec2<f32>(5.0, 6.0),
      vec2<f32>(7.0, 8.0));
  var idx: i32 = 2;
  var point: vec2<f32> = points[idx];
  return vec4<f32>(point[0], point[1], 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_array_index_return.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypeArray));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeConstruct));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpAccessChain));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
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

TEST(WgxSpirvSmokeTest, EmitsFragmentSpirvWithUniformStructMemberAccess) {
  auto program = wgx::Program::Parse(R"(
struct FragmentUniforms {
  color: vec4<f32>,
};

@group(0) @binding(0)
var<uniform> u_data: FragmentUniforms;

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  return u_data.color;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_uniform_struct_member_access.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsExecutionMode(words, SpvExecutionModeOriginUpperLeft));
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationDescriptorSet));
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationBinding));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpAccessChain));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
}

TEST(WgxSpirvSmokeTest, EmitsUniformMatrixStructWithMatrixStrideDecoration) {
  auto program = wgx::Program::Parse(R"(
struct VertexUniforms {
  transform: mat4x4<f32>,
};

@group(0) @binding(0)
var<uniform> u_data: VertexUniforms;

@vertex
fn vs_main(@location(0) position: vec4<f32>) -> @builtin(position) vec4<f32> {
  return u_data.transform * position;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_uniform_matrix_struct.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationDescriptorSet));
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationBinding));
  EXPECT_TRUE(ContainsMemberDecoration(words, SpvDecorationColMajor));
  EXPECT_TRUE(ContainsMemberDecoration(words, SpvDecorationMatrixStride));
}

TEST(WgxSpirvSmokeTest,
     EmitsVertexSpirvForHelperCallWithUniformStructArgument) {
  auto program = wgx::Program::Parse(R"(
struct CommonSlot {
  transform: mat4x4<f32>,
};

@group(0) @binding(0)
var<uniform> common_slot: CommonSlot;

fn get_vertex_position(a_pos: vec2<f32>, cs: CommonSlot) -> vec4<f32> {
  return cs.transform * vec4<f32>(a_pos, 0.0, 1.0);
}

@vertex
fn vs_main(@location(0) a_pos: vec2<f32>) -> @builtin(position) vec4<f32> {
  return get_vertex_position(a_pos, common_slot);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_helper_uniform_struct_arg.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFunctionCall));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsMemberDecoration(words, SpvDecorationMatrixStride));
}

TEST(WgxSpirvSmokeTest,
     EmitsUniformStructArrayMemberWithArrayStrideDecoration) {
  auto program = wgx::Program::Parse(R"(
struct GradientInfo {
  colors: array<vec4<f32>, 4>,
  stops: array<vec4<f32>, 2>,
  global_alpha: f32,
};

@group(0) @binding(0)
var<uniform> gradient_info: GradientInfo;

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  return gradient_info.colors[0] * gradient_info.global_alpha;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_uniform_array_stride.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationDescriptorSet));
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationBinding));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypeArray));
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationArrayStride));
}

TEST(WgxSpirvSmokeTest, EmitsUniformVec3IndexComparisonAsScalarExtract) {
  auto program = wgx::Program::Parse(R"(
struct GradientInfo {
  infos: vec3<i32>,
  global_alpha: f32,
};

@group(0) @binding(0)
var<uniform> gradient_info: GradientInfo;

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  if gradient_info.infos[0] == 0 {
    return vec4<f32>(1.0, 0.0, 0.0, 1.0);
  }
  return vec4<f32>(0.0, 0.0, 1.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_uniform_vec3_index_compare.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeExtract) ||
              ContainsInstruction(words, SpvOpVectorExtractDynamic));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpIEqual));
}

TEST(WgxSpirvSmokeTest,
     EmitsRepositoryStyleGradientInfoScalarComparisonAndArrayAccess) {
  auto program = wgx::Program::Parse(R"(
struct GradientInfo {
  infos: vec4<i32>,
  colors: array<vec4<f32>, 4>,
  global_alpha: f32,
  flags: i32,
};

@group(0) @binding(0)
var<uniform> gradient_info: GradientInfo;

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  if gradient_info.infos.z == 3 {
    return gradient_info.colors[1] * gradient_info.global_alpha;
  }
  return gradient_info.colors[0];
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_repository_gradient_info.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpIEqual));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypeArray));
}

TEST(WgxSpirvSmokeTest,
     EmitsHelperCallWithUniformScalarMemberArgumentAsPlainScalar) {
  auto program = wgx::Program::Parse(R"(
fn classify(mode: i32) -> f32 {
  if mode == 2 {
    return 1.0;
  }
  return 0.0;
}

struct ImageColorInfo {
  infos: vec3<i32>,
  global_alpha: f32,
};

@group(0) @binding(0)
var<uniform> image_color_info: ImageColorInfo;

@fragment
fn fs_main(@location(0) uv: vec2<f32>) -> @location(0) vec4<f32> {
  var x: f32 = classify(image_color_info.infos.y);
  return vec4<f32>(x, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_uniform_scalar_helper_arg.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFunctionCall));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpIEqual));
}

TEST(WgxSpirvSmokeTest,
     EmitsHelperCallWithRepositoryStyleUniformScalarMemberArgument) {
  auto program = wgx::Program::Parse(R"(
fn classify(mode: i32) -> f32 {
  if mode == 2 {
    return 1.0;
  }
  return 0.0;
}

struct GradientInfo {
  infos: vec4<i32>,
  colors: array<vec4<f32>, 4>,
  global_alpha: f32,
  flags: i32,
};

@group(0) @binding(0)
var<uniform> gradient_info: GradientInfo;

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  var x: f32 = classify(gradient_info.infos.z);
  return vec4<f32>(x, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_repository_scalar_helper_arg.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFunctionCall));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpIEqual));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypeArray));
}

TEST(WgxSpirvSmokeTest, EmitsRepositoryStyleTextureTileHelperShader) {
  auto program = wgx::Program::Parse(R"(
fn remap_float_tile(t: f32, tile_mode: i32) -> f32 {
  if tile_mode == 0 {
    return clamp(t, 0.0, 1.0);
  } else if tile_mode == 1 {
    return fract(t);
  } else if tile_mode == 2 {
    var t1: f32 = t - 1.0;
    var t2: f32 = t1 - 2.0 * floor(t1 / 2.0) - 1.0;
    return abs(t2);
  }
  return t;
}

struct ImageColorInfo {
  infos: vec3<i32>,
  global_alpha: f32,
};

@group(0) @binding(0)
var<uniform> image_color_info: ImageColorInfo;

@group(0) @binding(1)
var uSampler: sampler;

@group(0) @binding(2)
var uTexture: texture_2d<f32>;

@fragment
fn fs_main(@location(0) f_frag_coord: vec2<f32>) -> @location(0) vec4<f32> {
  var uv: vec2<f32> = f_frag_coord;
  var color: vec4<f32> = textureSample(uTexture, uSampler, uv);

  if (image_color_info.infos.y == 3 && (uv.x < 0.0 || uv.x >= 1.0)) ||
      (image_color_info.infos.z == 3 && (uv.y < 0.0 || uv.y >= 1.0)) {
    return vec4<f32>(0.0, 0.0, 0.0, 0.0);
  }

  uv.x = remap_float_tile(uv.x, image_color_info.infos.y);
  uv.y = remap_float_tile(uv.y, image_color_info.infos.z);

  if image_color_info.infos.x == 3 {
    color = vec4<f32>(color.xyz * color.w, color.w);
  }

  return color * image_color_info.global_alpha;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_repository_texture_tile_helper.spv",
                  result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFunctionCall));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpIEqual));
  EXPECT_FALSE(
      ContainsStoreToStorageClass(words, SpvStorageClassUniformConstant));
}

TEST(WgxSpirvSmokeTest, EmitsRepositoryStyleColorTextFragmentShader) {
  auto program = wgx::Program::Parse(R"(
@group(1) @binding(0) var uSampler      : sampler;
@group(1) @binding(1) var uFontTexture0 : texture_2d<f32>;
@group(1) @binding(2) var uFontTexture1 : texture_2d<f32>;
@group(1) @binding(3) var uFontTexture2 : texture_2d<f32>;
@group(1) @binding(4) var uFontTexture3 : texture_2d<f32>;

fn get_texture_color(font_index: i32, uv: vec2<f32>) -> vec4<f32> {
  var texture_dimension : vec2<u32> = vec2<u32>(textureDimensions(uFontTexture0));
  var texture_uv        : vec2<f32> = vec2<f32>(uv.x / f32(texture_dimension.x),
                                                uv.y / f32(texture_dimension.y));

  var color1 : vec4<f32> = textureSample(uFontTexture0, uSampler, texture_uv);
  var color2 : vec4<f32> = textureSample(uFontTexture1, uSampler, texture_uv);
  var color3 : vec4<f32> = textureSample(uFontTexture2, uSampler, texture_uv);
  var color4 : vec4<f32> = textureSample(uFontTexture3, uSampler, texture_uv);

  if font_index == 0 {
    return color1;
  } else if font_index == 1 {
    return color2;
  } else if font_index == 2 {
    return color3;
  } else if font_index == 3 {
    return color4;
  } else {
    return color1;
  }
}

struct ColorTextFSInput {
  @location(0) @interpolate(flat) txt_index : i32,
  @location(1)                    v_uv      : vec2<f32>,
  @location(2)                    v_color   : vec4<f32>
};

@fragment
fn fs_main(vs_in : ColorTextFSInput) -> @location(0) vec4<f32> {
  var fontAlpha: f32 = get_texture_color(vs_in.txt_index, vs_in.v_uv).r;
  var color: vec4<f32> =
      vec4<f32>(vs_in.v_color.rgb * vs_in.v_color.a, vs_in.v_color.a);
  return color * fontAlpha;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_color_text_fragment.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFunctionCall));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpImageSampleImplicitLod));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpIEqual));
  EXPECT_FALSE(
      ContainsStoreToStorageClass(words, SpvStorageClassUniformConstant));
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

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForForLoopWithCompoundAssign) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var pos: vec4<f32> = vec4<f32>(0.0, 0.0, 0.0, 1.0);
  for (var i: i32 = 0; i < 3; i += 1) {
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
    DumpSpirvBinary("wgx_vs_main_for_loop_compound.spv", result.spirv);
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

TEST(WgxSpirvSmokeTest, EmitsVertexSpirvBinaryForStructInterfaceIo) {
  auto program = wgx::Program::Parse(R"(
struct VertexInput {
  @location(0) pos: vec2<f32>,
  @location(1) uv: vec2<f32>,
};

struct VertexOutput {
  @builtin(position) position: vec4<f32>,
  @location(0) uv: vec2<f32>,
};

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {
  var output: VertexOutput;
  output.position = vec4<f32>(0.0, 0.0, 0.0, 1.0);
  output.uv = input.uv;
  return output;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_struct_interface_io.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsVariableWithStorageClass(words, SpvStorageClassInput));
  EXPECT_TRUE(ContainsVariableWithStorageClass(words, SpvStorageClassOutput));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationLocation));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_FALSE(ContainsInstruction(words, SpvOpFunctionParameter));
}

TEST(WgxSpirvSmokeTest, EmitsSpirvWithStructFunctionParameter) {
  auto program = wgx::Program::Parse(R"(
struct VertexInput {
  position: vec4<f32>,
};

fn helper(input: VertexInput) -> vec4<f32> {
  return input.position;
}

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var input: VertexInput;
  input.position = vec4<f32>(0.0, 0.0, 0.0, 1.0);
  return helper(input);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFunctionCall));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFunctionParameter));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpAccessChain));
}

TEST(WgxSpirvSmokeTest, EmitsSpirvWithStructReturnMemberExtract) {
  auto program = wgx::Program::Parse(R"(
struct VertexOutput {
  position: vec4<f32>,
};

fn helper() -> VertexOutput {
  var output: VertexOutput;
  output.position = vec4<f32>(0.0, 0.0, 0.0, 1.0);
  return output;
}

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  return helper().position;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFunctionCall));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeExtract));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsSpirvWithNestedStructMemberAccess) {
  auto program = wgx::Program::Parse(R"(
struct Inner {
  position: vec4<f32>,
};

struct Outer {
  inner: Inner,
};

fn helper(input: Outer) -> vec4<f32> {
  return input.inner.position;
}

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var value: Outer;
  value.inner.position = vec4<f32>(0.0, 0.0, 0.0, 1.0);
  return helper(value);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_nested_struct_member_access.spv", result.spirv);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFunctionCall));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpAccessChain));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsSpirvWithWholeStructAssignment) {
  auto program = wgx::Program::Parse(R"(
struct VertexOutput {
  position: vec4<f32>,
};

fn make_output() -> VertexOutput {
  var output: VertexOutput;
  output.position = vec4<f32>(0.0, 0.0, 0.0, 1.0);
  return output;
}

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var a: VertexOutput;
  var b: VertexOutput;
  a = make_output();
  b = a;
  return b.position;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_whole_struct_assignment.spv", result.spirv);
  auto words = result.spirv;
  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFunctionCall));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpAccessChain));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}

TEST(WgxSpirvSmokeTest, EmitsFragmentSpirvBinaryForStructInterfaceIo) {
  auto program = wgx::Program::Parse(R"(
struct FragmentInput {
  @location(0) color: vec4<f32>,
};

struct FragmentOutput {
  @location(0) color: vec4<f32>,
};

@fragment
fn fs_main(input: FragmentInput) -> FragmentOutput {
  var output: FragmentOutput;
  output.color = input.color;
  return output;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_struct_interface_io.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsExecutionMode(words, SpvExecutionModeOriginUpperLeft));
  EXPECT_TRUE(ContainsVariableWithStorageClass(words, SpvStorageClassInput));
  EXPECT_TRUE(ContainsVariableWithStorageClass(words, SpvStorageClassOutput));
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationLocation));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
}

TEST(WgxSpirvSmokeTest, EmitsFragmentSpirvBinaryForStructControlFlow) {
  auto program = wgx::Program::Parse(R"(
struct FragmentInput {
  @location(0) color: vec4<f32>,
};

struct FragmentOutput {
  @location(0) color: vec4<f32>,
};

fn make_output(color: vec4<f32>) -> FragmentOutput {
  var output: FragmentOutput;
  output.color = color;
  return output;
}

@fragment
fn fs_main(input: FragmentInput) -> FragmentOutput {
  var output: FragmentOutput;
  var use_input: bool = true;
  var i: i32 = 0;

  if (use_input) {
    output = make_output(input.color);
  } else {
    output = make_output(vec4<f32>(0.0, 0.0, 0.0, 1.0));
  }

  while (i < 1) {
    i = i + 1;
    output.color = output.color;
  }

  return output;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_struct_control_flow.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsExecutionMode(words, SpvExecutionModeOriginUpperLeft));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFunctionCall));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpSelectionMerge));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoopMerge));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpBranchConditional));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpAccessChain));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationLocation));
}

TEST(WgxSpirvSmokeTest, EmitsFragmentSpirvBinaryForStructForLoop) {
  auto program = wgx::Program::Parse(R"(
struct FragmentInput {
  @location(0) color: vec4<f32>,
};

struct FragmentOutput {
  @location(0) color: vec4<f32>,
};

@fragment
fn fs_main(input: FragmentInput) -> FragmentOutput {
  var output: FragmentOutput;
  output.color = vec4<f32>(0.0, 0.0, 0.0, 1.0);

  for (var i: i32 = 0; i < 2; i++) {
    output.color = input.color;
  }

  return output;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_struct_for_loop.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsExecutionMode(words, SpvExecutionModeOriginUpperLeft));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoopMerge));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpSLessThan));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpIAdd));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpAccessChain));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationLocation));
}

TEST(WgxSpirvSmokeTest, EmitsFragmentSpirvBinaryForStructSwitch) {
  auto program = wgx::Program::Parse(R"(
struct FragmentInput {
  @location(0) color: vec4<f32>,
};

struct FragmentOutput {
  @location(0) color: vec4<f32>,
};

fn make_output(color: vec4<f32>) -> FragmentOutput {
  var output: FragmentOutput;
  output.color = color;
  return output;
}

@fragment
fn fs_main(input: FragmentInput) -> FragmentOutput {
  var output: FragmentOutput;
  var mode: i32 = 1;

  switch mode {
    case 0: {
      output = make_output(vec4<f32>(0.0, 0.0, 0.0, 1.0));
    }
    case 1, 2: {
      output = make_output(input.color);
    }
    default: {
      output = make_output(vec4<f32>(1.0, 0.0, 0.0, 1.0));
    }
  }

  return output;
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_fs_main_struct_switch.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsExecutionMode(words, SpvExecutionModeOriginUpperLeft));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpFunctionCall));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpSelectionMerge));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpBranchConditional));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpIEqual));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpAccessChain));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsDecoration(words, SpvDecorationLocation));
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

TEST(WgxSpirvSmokeTest, EmitsSpirvWithMixedVec4ConstructorFromVec2AndScalars) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  let xy: vec2<f32> = vec2<f32>(0.25, 0.5);
  return vec4<f32>(xy, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_mixed_vec2_scalar_vec4_ctor.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeConstruct));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeExtract));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpReturn));
}

TEST(WgxSpirvSmokeTest, EmitsSpirvWithMixedVec4ConstructorFromVec3AndScalar) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  let xyz: vec3<f32> = vec3<f32>(0.25, 0.5, 0.75);
  return vec4<f32>(xyz, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_mixed_vec3_scalar_vec4_ctor.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeConstruct));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeExtract));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpReturn));
}

TEST(WgxSpirvSmokeTest, EmitsSpirvWithVectorSwizzleRead) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  let color: vec4<f32> = vec4<f32>(0.25, 0.5, 0.75, 1.0);
  let xy: vec2<f32> = color.xy;
  let ba: vec2<f32> = color.ba;
  return vec4<f32>(xy, ba.x, ba.y);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_vector_swizzle_read.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeExtract));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeConstruct));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpReturn));
}

TEST(WgxSpirvSmokeTest, EmitsSpirvWithVectorSwizzleAssignment) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var uv: vec2<f32> = vec2<f32>(0.25, 0.5);
  uv.x = 1.0 - uv.x;

  var edge_distances: vec4<f32> = vec4<f32>(0.25, 0.5, 0.75, 1.0);
  edge_distances.zw = -edge_distances.zw;

  return vec4<f32>(uv.xy, edge_distances.zw);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_vector_swizzle_assignment.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpAccessChain));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeExtract));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeConstruct));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpReturn));
}

TEST(WgxSpirvSmokeTest, EmitsSpirvWithMat2x2ConstructorAndMultiply) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  let m: mat2x2<f32> = mat2x2<f32>(1.0, 0.0, 0.0, 1.0);
  let xy: vec2<f32> = m * vec2<f32>(0.25, 0.5);
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_mat2_mul.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypeMatrix));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeConstruct));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpMatrixTimesVector));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpReturn));
}

TEST(WgxSpirvSmokeTest, EmitsSpirvWithMat3x3ConstructorAndMultiply) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  let m: mat3x3<f32> = mat3x3<f32>(
      1.0, 0.0, 0.0,
      0.0, 1.0, 0.0,
      0.0, 0.0, 1.0);
  let xyz: vec3<f32> = m * vec3<f32>(0.25, 0.5, 1.0);
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_mat3_mul.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypeMatrix));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeConstruct));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpMatrixTimesVector));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpReturn));
}

TEST(WgxSpirvSmokeTest, EmitsSpirvWithMat4x4VectorConstructorMultiply) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  let c0: vec4<f32> = vec4<f32>(1.0, 0.0, 0.0, 0.0);
  let c1: vec4<f32> = vec4<f32>(0.0, 1.0, 0.0, 0.0);
  let c2: vec4<f32> = vec4<f32>(0.0, 0.0, 1.0, 0.0);
  let c3: vec4<f32> = vec4<f32>(0.0, 0.0, 0.0, 1.0);
  let m: mat4x4<f32> = mat4x4<f32>(c0, c1, c2, c3);
  return m * vec4<f32>(0.25, 0.5, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_mat4_mul.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypeMatrix));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeConstruct));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpMatrixTimesVector));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpReturn));
}

TEST(WgxSpirvSmokeTest, EmitsSpirvWithMat4x4ScalarConstructorMultiply) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  let m: mat4x4<f32> = mat4x4<f32>(
      1.0, 0.0, 0.0, 0.0,
      0.0, 1.0, 0.0, 0.0,
      0.0, 0.0, 1.0, 0.0,
      0.0, 0.0, 0.0, 1.0);
  return m * vec4<f32>(0.25, 0.5, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_mat4_scalar_ctor_mul.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypeMatrix));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeConstruct));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpMatrixTimesVector));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpReturn));
}

TEST(WgxSpirvSmokeTest, EmitsSpirvWithMat4x4MatrixMultiplyChain) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  let lhs: mat4x4<f32> = mat4x4<f32>(
      vec4<f32>(1.0, 0.0, 0.0, 0.0),
      vec4<f32>(0.0, 1.0, 0.0, 0.0),
      vec4<f32>(0.0, 0.0, 1.0, 0.0),
      vec4<f32>(0.0, 0.0, 0.0, 1.0));
  let rhs: mat4x4<f32> = mat4x4<f32>(
      vec4<f32>(2.0, 0.0, 0.0, 0.0),
      vec4<f32>(0.0, 2.0, 0.0, 0.0),
      vec4<f32>(0.0, 0.0, 1.0, 0.0),
      vec4<f32>(0.0, 0.0, 0.0, 1.0));
  let transform: mat4x4<f32> = lhs * rhs;
  return transform * vec4<f32>(0.25, 0.5, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_mat4_chain_mul.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypeMatrix));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeConstruct));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpMatrixTimesMatrix));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpMatrixTimesVector));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpReturn));
}

TEST(WgxSpirvSmokeTest, EmitsSpirvWithMat3x3MatrixMultiply) {
  auto program = wgx::Program::Parse(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  let lhs: mat3x3<f32> = mat3x3<f32>(
      1.0, 0.0, 0.0,
      0.0, 1.0, 0.0,
      0.0, 0.0, 1.0);
  let rhs: mat3x3<f32> = mat3x3<f32>(
      2.0, 0.0, 0.0,
      0.0, 2.0, 0.0,
      0.0, 0.0, 1.0);
  let transform: mat3x3<f32> = lhs * rhs;
  let pos: vec3<f32> = transform * vec3<f32>(0.25, 0.5, 1.0);
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("vs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_vs_main_mat3_matrix_mul.spv", result.spirv);
  auto words = result.spirv;

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpTypeMatrix));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeConstruct));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpMatrixTimesMatrix));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpMatrixTimesVector));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpReturn));
}

TEST(WgxSpirvSmokeTest, EmitsGradientLinear4OffsetFastTextWGSL) {
  skity::Shader::GradientInfo gradient_info;
  gradient_info.color_count = 4;
  gradient_info.colors = {{0.0, 0.0, 0.0, 1.0},
                          {1.0, 0.0, 0.0, 1.0},
                          {0.0, 1.0, 0.0, 1.0},
                          {0.0, 0.0, 1.0, 1.0}};
  skity::WGSLGradientTextFragment gradient_fragment(
      skity::WGSLGradientTextFragment::BatchedTexture{}, {}, gradient_info,
      skity::Shader::GradientType::kLinear, 1.0f);

  auto program = wgx::Program::Parse(gradient_fragment.GenSourceWGSL());

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_GradientLinear4OffsetFastTextWGSL.spv", result.spirv);
  auto words = result.spirv;
}

TEST(WgxSpirvSmokeTest, EmitsGradientLinear4WGSL) {
  skity::Shader::GradientInfo gradient_info;
  gradient_info.color_count = 4;
  gradient_info.colors = {{0.0, 0.0, 0.0, 1.0},
                          {1.0, 0.0, 0.0, 1.0},
                          {0.0, 1.0, 0.0, 1.0},
                          {0.0, 0.0, 1.0, 1.0}};
  gradient_info.color_offsets = {0.0f, 0.5f, 1.0f, 1.5f};
  skity::WGSLGradientFragment gradient_fragment(
      gradient_info, skity::Shader::GradientType::kLinear, 1.0f, {});

  skity::Paint paint;
  auto rrect = skity::RRect::MakeOval(skity::Rect::MakeWH(100.0f, 100.0f));
  std::vector<skity::BatchGroup<skity::RRect>> rrect_batch_group;
  rrect_batch_group.push_back({rrect, paint});
  skity::WGSLRRectGeometry geometry{rrect_batch_group};

  skity::HWWGSLShaderWriter shader_writer{&geometry, &gradient_fragment};

  auto fs = shader_writer.GenFSSourceWGSL();

  auto program = wgx::Program::Parse(fs);

  std::cerr << fs << std::endl;

  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv("fs_main", options);

  ASSERT_TRUE(result.success);
  DumpSpirvBinary("wgx_GradientLinear4WGSL.spv", result.spirv);
  auto words = result.spirv;
}

#endif

}  // namespace
