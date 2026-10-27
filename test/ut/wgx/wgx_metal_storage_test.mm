// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#import <Metal/Metal.h>

#include <gtest/gtest.h>
#include <wgsl_cross.h>

TEST(WgxMetalStorageTest, CompilesTransitiveCaptureInBothStages) {
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  if (device == nil) GTEST_SKIP() << "Metal device unavailable";
  for (bool record : {false, true}) {
    std::string source = "struct Record { id: u32, transform: mat4x4<f32>, };\n";
    source += "@group(0) @binding(2) var<storage, read> records: array<";
    source += record ? "Record>;\n" : "mat4x4<f32>>;\n";
    source += R"(
@group(0) @binding(0) var<uniform> projection: mat4x4<f32>;
fn inner(index: u32) -> vec4<f32> {
  return records[index])";
    source += record ? ".transform" : "";
    source += R"( * vec4<f32>(1.0, 2.0, 0.0, 1.0);
}
fn outer() -> vec4<f32> { return projection * inner(u32(0)); }
@vertex fn vs_main() -> @builtin(position) vec4<f32> { return outer(); }
@fragment fn fs_main() -> @location(0) vec4<f32> { return outer(); }
)";
    auto program = wgx::Program::Parse(source);
    ASSERT_FALSE(program->GetDiagnosis().has_value()) << program->GetDiagnosis()->message;
    for (const char* entry : {"vs_main", "fs_main"}) {
      auto result = program->WriteToMsl(entry, {});
      ASSERT_TRUE(result);
      NSError* error = nil;
      id<MTLLibrary> library = [device newLibraryWithSource:@(result.content.c_str())
                                                    options:nil
                                                      error:&error];
      ASSERT_NE(library, nil) << (error ? error.localizedDescription.UTF8String : "") << "\n"
                              << result.content;
      EXPECT_NE([library newFunctionWithName:@(entry)], nil);
    }
  }
}
