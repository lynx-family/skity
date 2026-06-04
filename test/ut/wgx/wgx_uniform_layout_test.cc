// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <wgsl_cross.h>

namespace {

// Helper: find uniform binding entry by group and binding index.
const wgx::BindGroupEntry* FindUniformEntry(
    const std::vector<wgx::BindGroup>& bind_groups, uint32_t group,
    uint32_t binding) {
  for (const auto& bg : bind_groups) {
    if (bg.group != group) continue;
    for (const auto& entry : bg.entries) {
      if (entry.binding == binding &&
          entry.type == wgx::BindingType::kUniformBuffer) {
        return &entry;
      }
    }
  }
  return nullptr;
}

}  // namespace

// ---------------------------------------------------------------------------
// Test: Struct with only small-alignment members (vec2 + scalars).
// std140 rule 9 requires struct alignment >= 16.
// This is the "ConicalInfo"-style pattern that exposed the bug.
// ---------------------------------------------------------------------------
TEST(WgxUniformLayoutTest,
     StructWithSmallAlignmentMembersHasCorrectSizeInGlsl) {
  const char* source = R"(
struct SmallAlign {
  center1 : vec2<f32>,
  center2 : vec2<f32>,
  radius1 : f32,
  radius2 : f32,
};

@group(0) @binding(0)
var<uniform> info : SmallAlign;

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  return vec4<f32>(info.center1.x, info.radius1, 0.0, 1.0);
}
)";

  auto program = wgx::Program::Parse(source);
  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  // GLSL uses kStd140 — struct alignment must be >= 16
  wgx::GlslOptions glsl_opts;
  glsl_opts.standard = wgx::GlslOptions::Standard::kDesktop;
  glsl_opts.major_version = 4;
  glsl_opts.minor_version = 1;
  auto result = program->WriteToGlsl("fs_main", glsl_opts);
  ASSERT_TRUE(result.success);

  const auto* entry = FindUniformEntry(result.bind_groups, 0, 0);
  ASSERT_NE(entry, nullptr);
  ASSERT_NE(entry->type_definition, nullptr);

  // Members: vec2(8) + vec2(8) + f32(4) + f32(4) = 24 bytes
  // std140 struct alignment = max(8,8,4,4)=8, rounded up to 16
  // struct size = round_up(16, 24) = 32
  EXPECT_EQ(entry->type_definition->size, 32u)
      << "std140 struct size should be 32 (24 bytes of members + 8 padding)";
  EXPECT_EQ(entry->type_definition->alignment, 16u)
      << "std140 struct alignment should be 16 (rule 9: rounded to vec4)";

  // GLSL output must contain std140 and inner wrapper
  EXPECT_NE(result.content.find("std140"), std::string::npos);
  EXPECT_NE(result.content.find("inner"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Test: MSL uses kStd430MSL — struct alignment is NOT rounded to 16.
// The same struct should have natural alignment (8) and smaller size.
// ---------------------------------------------------------------------------
TEST(WgxUniformLayoutTest,
     StructWithSmallAlignmentMembersHasNaturalAlignmentInMsl) {
  const char* source = R"(
struct SmallAlign {
  center1 : vec2<f32>,
  center2 : vec2<f32>,
  radius1 : f32,
  radius2 : f32,
};

@group(0) @binding(0)
var<uniform> info : SmallAlign;

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  return vec4<f32>(info.center1.x, info.radius1, 0.0, 1.0);
}
)";

  auto program = wgx::Program::Parse(source);
  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  // MSL uses kStd430MSL — no 16-byte alignment rounding
  wgx::MslOptions msl_opts;
  auto result = program->WriteToMsl("fs_main", msl_opts);
  ASSERT_TRUE(result.success);

  const auto* entry = FindUniformEntry(result.bind_groups, 0, 0);
  ASSERT_NE(entry, nullptr);
  ASSERT_NE(entry->type_definition, nullptr);

  // Members: vec2(8) + vec2(8) + f32(4) + f32(4) = 24 bytes
  // std430 struct alignment = max(8,8,4,4) = 8 (no rounding)
  // struct size = round_up(8, 24) = 24
  EXPECT_EQ(entry->type_definition->alignment, 8u)
      << "MSL struct alignment should be 8 (natural max, no std140 rounding)";
  EXPECT_EQ(entry->type_definition->size, 24u)
      << "MSL struct size should be 24 (exact member total)";
}

// ---------------------------------------------------------------------------
// Test: Struct with already-large alignment (mat4x4, vec4).
// Alignment is already 16, fix should not change anything.
// ---------------------------------------------------------------------------
TEST(WgxUniformLayoutTest, StructWithLargeAlignmentMembersIsUnchanged) {
  const char* source = R"(
struct LargeAlign {
  mvp           : mat4x4<f32>,
  userTransform : mat4x4<f32>,
  extraInfo     : vec4<f32>,
};

@group(0) @binding(0)
var<uniform> common : LargeAlign;

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  return common.mvp * vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)";

  auto program = wgx::Program::Parse(source);
  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::GlslOptions glsl_opts;
  glsl_opts.standard = wgx::GlslOptions::Standard::kDesktop;
  glsl_opts.major_version = 4;
  glsl_opts.minor_version = 1;
  auto result = program->WriteToGlsl("vs_main", glsl_opts);
  ASSERT_TRUE(result.success);

  const auto* entry = FindUniformEntry(result.bind_groups, 0, 0);
  ASSERT_NE(entry, nullptr);
  ASSERT_NE(entry->type_definition, nullptr);

  // mat4x4(64) + mat4x4(64) + vec4(16) = 144, alignment already 16
  EXPECT_EQ(entry->type_definition->size, 144u);
  EXPECT_EQ(entry->type_definition->alignment, 16u);
}

// ---------------------------------------------------------------------------
// Test: Nested struct where inner struct has small alignment.
// In std140, the inner struct's alignment must be 16, affecting the outer
// struct's member offsets.
// ---------------------------------------------------------------------------
TEST(WgxUniformLayoutTest,
     NestedStructWithSmallAlignmentInnerHasCorrectLayout) {
  const char* source = R"(
struct Inner {
  a : vec2<f32>,
  b : vec2<f32>,
};

struct Outer {
  inner : Inner,
  extra : f32,
};

@group(0) @binding(0)
var<uniform> data : Outer;

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  return vec4<f32>(data.inner.a.x, data.extra, 0.0, 1.0);
}
)";

  auto program = wgx::Program::Parse(source);
  ASSERT_NE(program, nullptr);
  ASSERT_FALSE(program->GetDiagnosis().has_value());

  wgx::GlslOptions glsl_opts;
  glsl_opts.standard = wgx::GlslOptions::Standard::kDesktop;
  glsl_opts.major_version = 4;
  glsl_opts.minor_version = 1;
  auto result = program->WriteToGlsl("fs_main", glsl_opts);
  ASSERT_TRUE(result.success);

  const auto* entry = FindUniformEntry(result.bind_groups, 0, 0);
  ASSERT_NE(entry, nullptr);
  ASSERT_NE(entry->type_definition, nullptr);
  ASSERT_TRUE(entry->type_definition->IsStruct());

  auto* outer =
      static_cast<wgx::StructDefinition*>(entry->type_definition.get());

  // Outer struct layout (std140):
  //   Inner: alignment = round_up(max(8,8), 16) = 16, size = round_up(16, 16) =
  //   16 inner at offset 0, size 16 extra (f32, align 4) at offset 16, size 4
  //   total = 20, Outer alignment = max(16, 4) = 16
  //   Outer size = round_up(16, 20) = 32
  EXPECT_EQ(outer->alignment, 16u);
  EXPECT_EQ(outer->size, 32u);

  // Verify inner struct has alignment 16 in std140
  auto* inner_field = outer->GetMember("inner");
  ASSERT_NE(inner_field, nullptr);
  EXPECT_EQ(inner_field->offset, 0u);
  ASSERT_NE(inner_field->type, nullptr);
  EXPECT_EQ(inner_field->type->alignment, 16u)
      << "Inner struct should have alignment 16 (std140 rule 9)";
  EXPECT_EQ(inner_field->type->size, 16u);

  // "extra" should start at offset 16 (after the 16-byte inner struct)
  auto* extra_field = outer->GetMember("extra");
  ASSERT_NE(extra_field, nullptr);
  EXPECT_EQ(extra_field->offset, 16u)
      << "extra should follow inner at offset 16";
}
