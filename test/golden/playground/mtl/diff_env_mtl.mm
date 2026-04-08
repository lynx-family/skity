// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "playground/mtl/diff_env_mtl.h"

#include <skity/gpu/gpu_context_mtl.h>
#include "common/mtl/golden_test_env_mtl.h"

namespace skity {
namespace testing {

DiffEnvMtlTest* g_diff_env_mtl_test = nullptr;

DiffEnvMtlTest* DiffEnvMtlTest::GetInstance() { return g_diff_env_mtl_test; }

::testing::Environment* CreateDiffEnv() {
  auto env = GoldenTestEnv::GetInstance();

  id<MTLDevice> device = nil;
  id<MTLCommandQueue> command_queue = nil;

  if (env->GetBackend() == Backend::kMetal) {
    auto env_mtl = static_cast<GoldenTestEnvMTL*>(env);

    device = env_mtl->GetDevice();
    command_queue = env_mtl->GetCommandQueue();
  } else {
    device = MTLCreateSystemDefaultDevice();

    command_queue = [device newCommandQueue];
  }

  g_diff_env_mtl_test = new DiffEnvMtlTest(device, command_queue);

  return g_diff_env_mtl_test;
}

void DiffEnvMtlTest::SetUp() {
  const char* diff_shader_source = R"(
  #include <metal_stdlib>
  #include <simd/simd.h>

  using namespace metal;

  kernel void diff_image(texture2d<float, access::read> source_image    [[texture(0)]],
                         texture2d<float, access::read> target_image    [[texture(1)]],
                         texture2d<float, access::write> isolate_image  [[texture(2)]],
                         texture2d<float, access::write> diff_image     [[texture(3)]],
                         uint2 gid                                      [[thread_position_in_grid]])
  {
    if (gid.x >= source_image.get_width() || gid.y >= source_image.get_height()) {
      return;
    }

    float4 source_color = source_image.read(gid);
    float4 target_color = target_image.read(gid);

    if (source_color.a > 0.001) {
      source_color.rgb /= source_color.a;
    }

    float4 diff = abs(source_color - target_color);

    if (diff.x > 0.001 || diff.y > 0.001 || diff.z > 0.001 || diff.w > 0.001) {
      float4 diff_image_color = float4(abs(float3(1.0, 1.0, 1.0) - target_color.rgb), 1.0);

      isolate_image.write(diff_image_color, gid);
      diff_image.write(diff_image_color, gid);
    } else {
      isolate_image.write(float4(0.0, 0.0, 0.0, 0.0), gid);

      diff_image.write(target_color, gid);
    }
  }
)";

  NSString* diff_shader_source_string = [NSString stringWithUTF8String:diff_shader_source];

  MTLCompileOptions* compileOptions = [MTLCompileOptions new];
  if (@available(macOS 10.13, iOS 11.0, *)) {
    compileOptions.languageVersion = MTLLanguageVersion2_0;
  }

  NSError* error = nil;

  id<MTLLibrary> library = [device_ newLibraryWithSource:diff_shader_source_string
                                                 options:compileOptions
                                                   error:&error];

  if (error != nil) {
    std::cerr << "diff shader compile error: " << error.localizedDescription.UTF8String
              << std::endl;
  }

  if (library == nil) {
    return;
  }

  diff_pipeline_state_ =
      [device_ newComputePipelineStateWithFunction:[library newFunctionWithName:@"diff_image"]
                                             error:&error];
  if (error != nil) {
    std::cerr << "diff pipeline state create error: " << error.localizedDescription.UTF8String
              << std::endl;
  }

  if (diff_pipeline_state_ == nil) {
    std::cerr << "Failed create diff pipeline" << std::endl;
  }

  if (GoldenTestEnv::GetInstance()->GetBackend() != Backend::kMetal) {
    gpu_context_ = MTLContextCreate(device_, command_queue_);
  }
}

void DiffEnvMtlTest::TearDown() { diff_pipeline_state_ = nil; }

}  // namespace testing
}  // namespace skity
