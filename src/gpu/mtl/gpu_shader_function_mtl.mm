// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#if !__has_feature(objc_arc)
#error ARC must be enabled!
#endif

#include "src/gpu/mtl/gpu_shader_function_mtl.h"
#include "src/logging.hpp"

namespace skity {

GPUShaderFunctionMTL::GPUShaderFunctionMTL(GPULabel label, id<MTLDevice> device,
                                           GPUShaderStage stage, const char* source,
                                           const char* entry_point,
                                           GPUShaderFunctionErrorCallback error_callback)
    : GPUShaderFunction(std::move(label)) {
  NSError* err = nil;
  NSString* shader_str = [NSString stringWithCString:source encoding:NSUTF8StringEncoding];

  MTLCompileOptions* compileOptions = [MTLCompileOptions new];
  // Framebuffer fetch is supported in MSL 2.3 in MacOS 11+.
  if (@available(macOS 11.0, iOS 14.0, *)) {
    compileOptions.languageVersion = MTLLanguageVersion2_3;
  } else if (@available(macOS 10.13, iOS 11.0, *)) {
    compileOptions.languageVersion = MTLLanguageVersion2_0;
  }
  id<MTLLibrary> lib = [device newLibraryWithSource:shader_str options:compileOptions error:&err];

  if (err != nil) {
    // shader load failed
    if (error_callback) {
      error_callback([[err localizedDescription] UTF8String]);
    }

    LOGE("Failed to compile shader {} : {}", GetLabel(), [[err localizedDescription] UTF8String]);
  }

  // Some old version Metal may generate warning error during library creation
  // The error does not affect the generation of the library
  // So If the library is not nil, means this error is irrelevant
  if (lib == nil) {
    return;
  }

  NSString* function_name = [NSString stringWithCString:entry_point encoding:NSUTF8StringEncoding];

  MTLFunctionConstantValues* constant_values = [MTLFunctionConstantValues new];
  mtl_function_ = [lib newFunctionWithName:function_name constantValues:constant_values error:&err];

  if (mtl_function_ == nil || err != nil) {
    LOGE("Failed to create shader function {} with error: {}", GetLabel(),
         [[err localizedDescription] UTF8String]);
    return;
  }
}

}  // namespace skity
