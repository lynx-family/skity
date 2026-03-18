// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <wgsl_cross.h>

namespace {

#if defined(WGX_VULKAN)
TEST(WgxSpirvSmokeTest, EmitsSpirvPlaceholderForValidEntryPoint) {
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
  EXPECT_NE(result.content.find("SPIR-V emission is under development"),
            std::string::npos);
  EXPECT_NE(result.content.find("entry_point=vs_main"), std::string::npos);
  EXPECT_NE(result.content.find("inst_count="), std::string::npos);
}
#endif

}  // namespace
