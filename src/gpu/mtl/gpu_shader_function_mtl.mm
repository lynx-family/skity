// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#if !__has_feature(objc_arc)
#error ARC must be enabled!
#endif

#include "src/gpu/mtl/gpu_shader_function_mtl.h"

namespace skity {

GPUShaderFunctionMTL::GPUShaderFunctionMTL(std::string label, id<MTLDevice> device,
                                           GPUShaderStage stage, const char* source,
                                           const char* entry_point,
                                           const std::vector<int32_t>& constant_values,
                                           GPUShaderFunctionErrorCallback error_callback)
    : GPUShaderFunction(std::move(label)) {
  NSError* err = nil;
  NSString* shader_str = [NSString stringWithCString:source encoding:NSUTF8StringEncoding];

  MTLCompileOptions* compileOptions = [MTLCompileOptions new];
  if (@available(macOS 10.13, iOS 11.0, *)) {
    compileOptions.languageVersion = MTLLanguageVersion2_0;
  }
  id<MTLLibrary> lib = [device newLibraryWithSource:shader_str options:compileOptions error:&err];

  if (err != nil) {
    // shader load failed
    if (error_callback) {
      error_callback([[err localizedDescription] UTF8String]);
    }
  }

  // Some old version Metal may generate warning error during library creation
  // The error does not affect the generation of the library
  // So If the library is not nil, means this error is irrelevant
  if (lib == nil) {
    return;
  }

  NSString* function_name = [NSString stringWithCString:entry_point encoding:NSUTF8StringEncoding];

  if (constant_values.empty()) {
    mtl_function_ = [lib newFunctionWithName:function_name];
  } else {
    MTLFunctionConstantValues* values = [MTLFunctionConstantValues new];
    NSUInteger index = 0;
    for (int32_t v : constant_values) {
      [values setConstantValue:&v type:MTLDataTypeInt atIndex:index];
      index++;
    }

    mtl_function_ = [lib newFunctionWithName:function_name constantValues:values error:&err];
    if (err != nil) {
      if (error_callback) {
        error_callback([[err localizedDescription] UTF8String]);
      }
    }
  }
}

}  // namespace skity
