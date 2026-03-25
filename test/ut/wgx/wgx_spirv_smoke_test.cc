// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <wgsl_cross.h>

#include <cstdint>
#include <vector>

#include "spirv/unified1/spirv.h"

namespace {

#if defined(WGX_VULKAN)
std::vector<uint32_t> DecodeWords(const std::string& binary) {
  if (binary.size() % sizeof(uint32_t) != 0u) {
    return {};
  }

  std::vector<uint32_t> words(binary.size() / sizeof(uint32_t), 0u);
  for (size_t i = 0; i < words.size(); ++i) {
    words[i] =
        static_cast<uint32_t>(static_cast<uint8_t>(binary[i * 4u + 0u])) |
        (static_cast<uint32_t>(static_cast<uint8_t>(binary[i * 4u + 1u]))
         << 8u) |
        (static_cast<uint32_t>(static_cast<uint8_t>(binary[i * 4u + 2u]))
         << 16u) |
        (static_cast<uint32_t>(static_cast<uint8_t>(binary[i * 4u + 3u]))
         << 24u);
  }

  return words;
}

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
  auto words = DecodeWords(result.content);

  ASSERT_GE(words.size(), 5u);
  EXPECT_EQ(words[0], SpvMagicNumber);
  EXPECT_EQ(words[1], 0x00010300u);
  EXPECT_EQ(words[3], 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpEntryPoint));
  EXPECT_TRUE(ContainsInstruction(words, SpvOpModuleProcessed));
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
  auto words = DecodeWords(result.content);

  ASSERT_GE(words.size(), 5u);
  EXPECT_TRUE(ContainsInstruction(words, SpvOpEntryPoint));
  EXPECT_TRUE(ContainsExecutionMode(words, SpvExecutionModeOriginUpperLeft));
}

TEST(WgxSpirvSmokeTest,
     RejectsVertexEntryPointWithBuiltinPositionReturnForNow) {
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

  EXPECT_FALSE(result.success);
  EXPECT_TRUE(result.content.empty());
}
#endif

}  // namespace
