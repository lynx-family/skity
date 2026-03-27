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

  ASSERT_TRUE(result.success);
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
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeConstruct));
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
  EXPECT_TRUE(ContainsInstruction(words, SpvOpCompositeConstruct));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpStore));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpLoad));
  EXPECT_TRUE(ContainsBuiltInDecoration(words, SpvBuiltInPosition));
}
#endif

}  // namespace
