// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <wgsl_cross.h>

namespace {

constexpr const char* kStorageSource = R"(
struct Record { transform: mat4x4<f32>, };
@group(0) @binding(2) var<storage, read> records: array<Record>;
@vertex fn vs_main() -> @builtin(position) vec4<f32> {
  return records[1].transform * vec4<f32>(1.0, 2.0, 0.0, 1.0);
}
)";

TEST(WgxRuntimeArrayTest, ReflectsRuntimeRecordArray) {
  auto program = wgx::Program::Parse(kStorageSource);
  ASSERT_FALSE(program->GetDiagnosis().has_value())
      << program->GetDiagnosis()->message;
  auto groups = program->GetWGSLBindGroups("vs_main");
  ASSERT_EQ(groups.size(), 1u);
  const auto* entry = groups[0].GetEntry(2);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->type, wgx::BindingType::kStorageBuffer);
  EXPECT_EQ(entry->stage, wgx::ShaderStage_kVertex);
  ASSERT_NE(entry->type_definition, nullptr);
  ASSERT_TRUE(entry->type_definition->IsArray());
  auto* array =
      static_cast<wgx::ArrayDefinition*>(entry->type_definition.get());
  EXPECT_TRUE(array->IsRuntimeSized());
  EXPECT_EQ(array->count, 0u);
  EXPECT_EQ(array->size, 0u);
  EXPECT_EQ(array->alignment, 16u);
  EXPECT_EQ(array->stride, 64u);
  EXPECT_EQ(array->GetElementAt(0), nullptr);
  ASSERT_NE(array->GetElementType(), nullptr);
  EXPECT_EQ(array->GetElementType()->size, 64u);
  EXPECT_FALSE(array->SetData(nullptr, 0));
}

TEST(WgxRuntimeArrayTest, RejectsUnsupportedStorageAccess) {
  for (const char* declaration :
       {"var<storage, read_write> records: array<mat4x4<f32>>;",
        "var<storage, write> records: array<mat4x4<f32>>;"}) {
    std::string source = "@group(0) @binding(2) ";
    source += declaration;
    source += R"(
@vertex fn vs_main() -> @builtin(position) vec4<f32> {
  return records[0] * vec4<f32>(0.0, 0.0, 0.0, 1.0);
})";
    auto program = wgx::Program::Parse(source);
    EXPECT_TRUE(program->GetWGSLBindGroups("vs_main").empty()) << declaration;
    EXPECT_FALSE(program->WriteToMsl("vs_main", {})) << declaration;
#ifdef WGX_SPIRV
    EXPECT_FALSE(program->WriteToSpirv("vs_main", {})) << declaration;
#endif
  }
}

TEST(WgxRuntimeArrayTest, GlslEmitsStorageBlocksAndUsesSeparateSlots) {
  auto program = wgx::Program::Parse(R"(
struct Record { id: u32, transform: mat4x4<f32>, };
@group(0) @binding(0) var<uniform> projection: mat4x4<f32>;
@group(0) @binding(2) var<storage, read> records: array<Record>;
@group(0) @binding(3) var<storage> matrices: array<mat4x4<f32>>;
fn position() -> vec4<f32> {
  return projection * records[0].transform * matrices[1] * vec4<f32>(1.0);
}
@vertex fn vs_main() -> @builtin(position) vec4<f32> { return position(); }
@fragment fn fs_main() -> @location(0) vec4<f32> { return position(); }
)");
  ASSERT_FALSE(program->GetDiagnosis().has_value());
  for (bool desktop : {false, true}) {
    wgx::GlslOptions options;
    options.standard = desktop ? wgx::GlslOptions::Standard::kDesktop
                               : wgx::GlslOptions::Standard::kES;
    options.major_version = desktop ? 4 : 3;
    options.minor_version = desktop ? 3 : 1;
    wgx::CompilerContext context;
    context.last_ubo_binding = 7;
    context.last_storage_binding = 1;
    auto result = program->WriteToGlsl("vs_main", options, context);
    ASSERT_TRUE(result);
    EXPECT_NE(result.content.find("binding = 1, std430 ) readonly buffer"),
              std::string::npos);
    EXPECT_NE(result.content.find("binding = 2, std430 ) readonly buffer"),
              std::string::npos);
    EXPECT_NE(result.content.find("inner[]"), std::string::npos);
    EXPECT_NE(result.content.find(".inner["), std::string::npos);
    ASSERT_EQ(result.bind_groups.size(), 1u);
    ASSERT_NE(result.bind_groups[0].GetEntry(0), nullptr);
    ASSERT_NE(result.bind_groups[0].GetEntry(2), nullptr);
    ASSERT_NE(result.bind_groups[0].GetEntry(3), nullptr);
    EXPECT_EQ(result.bind_groups[0].GetEntry(0)->index, 7u);
    EXPECT_EQ(result.bind_groups[0].GetEntry(2)->index, 1u);
    EXPECT_EQ(result.bind_groups[0].GetEntry(3)->index, 2u);
    EXPECT_EQ(result.context.last_ubo_binding, 8u);
    EXPECT_EQ(result.context.last_storage_binding, 3u);
    auto next = program->WriteToGlsl("fs_main", options, result.context);
    ASSERT_TRUE(next);
    ASSERT_EQ(next.bind_groups.size(), 1u);
    ASSERT_NE(next.bind_groups[0].GetEntry(2), nullptr);
    EXPECT_EQ(next.bind_groups[0].GetEntry(2)->index, 3u);
  }
}

TEST(WgxRuntimeArrayTest, MslUsesDevicePointersAndSharedBufferSlots) {
  auto program = wgx::Program::Parse(R"(
struct Record { transform: mat4x4<f32>, };
@group(0) @binding(2) var<storage, read> records: array<Record>;
@group(0) @binding(0) var<uniform> projection: mat4x4<f32>;
fn inner() -> vec4<f32> {
  return records[0].transform * vec4<f32>(1.0, 2.0, 0.0, 1.0);
}
fn outer() -> vec4<f32> { return projection * inner(); }
@vertex fn vs_main() -> @builtin(position) vec4<f32> { return outer(); }
)");
  ASSERT_FALSE(program->GetDiagnosis().has_value());
  auto result = program->WriteToMsl("vs_main", {});
  ASSERT_TRUE(result);
  EXPECT_NE(result.content.find("const device "), std::string::npos)
      << result.content;
  EXPECT_EQ(result.content.find("array<"), std::string::npos);
  ASSERT_EQ(result.bind_groups.size(), 1u);
  const auto* storage = result.bind_groups[0].GetEntry(2);
  const auto* uniform = result.bind_groups[0].GetEntry(0);
  ASSERT_NE(storage, nullptr);
  ASSERT_NE(uniform, nullptr);
  EXPECT_GE(storage->index, 2u);
  EXPECT_GE(uniform->index, 2u);
  EXPECT_NE(storage->index, uniform->index);
  auto next = program->WriteToMsl("vs_main", {}, result.context);
  ASSERT_TRUE(next);
  EXPECT_GE(next.bind_groups[0].GetEntry(2)->index,
            result.context.last_ubo_binding);
}

TEST(WgxRuntimeArrayTest, ReflectsMatrixAndRecordStride) {
  for (bool record : {false, true}) {
    std::string source =
        "struct Record { id: u32, transform: mat4x4<f32>, };\n";
    source += "@group(0) @binding(0) var<storage> data: array<";
    source += record ? "Record>;\n" : "mat4x4<f32>>;\n";
    source +=
        "@vertex fn main() -> @builtin(position) vec4<f32> { return data[0]";
    source += record ? ".transform" : "";
    source += " * vec4<f32>(0.0, 0.0, 0.0, 1.0); }";
    auto program = wgx::Program::Parse(source);
    ASSERT_FALSE(program->GetDiagnosis().has_value());
    auto groups = program->GetWGSLBindGroups("main");
    ASSERT_EQ(groups.size(), 1u);
    auto* entry = groups[0].GetEntry(0);
    ASSERT_NE(entry, nullptr);
    ASSERT_NE(entry->type_definition, nullptr);
    ASSERT_TRUE(entry->type_definition->IsArray());
    auto* array =
        static_cast<wgx::ArrayDefinition*>(entry->type_definition.get());
    EXPECT_EQ(array->stride, record ? 80u : 64u);
    if (record) {
      auto* element = array->GetElementType();
      ASSERT_NE(element, nullptr);
      ASSERT_TRUE(element->IsStruct());
      auto* fields = static_cast<wgx::StructDefinition*>(element);
      ASSERT_EQ(fields->members.size(), 2u);
      EXPECT_EQ(fields->members[0]->offset, 0u);
      EXPECT_EQ(fields->members[1]->offset, 16u);
    }
  }
}

#ifdef WGX_SPIRV
TEST(WgxRuntimeArrayTest, KeepsExistingSpirvTypeSupport) {
  auto program = wgx::Program::Parse(R"(
@group(0) @binding(0) var<uniform> matrix: mat2x3<f32>;
@group(0) @binding(1) var<storage> value: vec4<f32>;
@vertex fn vs_main() -> @builtin(position) vec4<f32> {
  let position: vec3<f32> = matrix * vec2<f32>(1.0, 2.0);
  return vec4<f32>(position, 1.0) + value;
}
)");
  ASSERT_FALSE(program->GetDiagnosis().has_value());
  // Existing SPIR-V types do not depend on CPU reflection support.
  EXPECT_TRUE(program->WriteToSpirv("vs_main", {}));
}
#endif

}  // namespace
