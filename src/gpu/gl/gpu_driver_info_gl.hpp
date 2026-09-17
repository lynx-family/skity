// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GL_GPU_DRIVER_INFO_GL_HPP
#define SRC_GPU_GL_GPU_DRIVER_INFO_GL_HPP

#include <cstdint>
#include <string>

#include "src/gpu/gpu_caps.hpp"

namespace skity {

enum class GLVendor { kUnknown, kARM, kPowerVR, kQualcomm };

enum class GLRendererFamily { kUnknown, kAdreno, kMali, kPowerVR };

struct GLDriverInfo {
  static GLDriverInfo FromStrings(std::string vendor, std::string renderer,
                                  std::string version);

  GLVendor vendor = GLVendor::kUnknown;
  GLRendererFamily renderer_family = GLRendererFamily::kUnknown;
  int32_t renderer_model = -1;
  std::string vendor_name;
  std::string renderer;
  std::string version;
};

struct GLDriverWorkarounds {
  bool use_draw_for_clear = false;
  bool disable_framebuffer_fetch = false;
  bool disable_native_advanced_blend = false;
};

GLDriverWorkarounds ResolveGLDriverWorkarounds(const GLDriverInfo& driver_info);
void ApplyGLDriverWorkarounds(const GLDriverWorkarounds& workarounds,
                              GPUCaps& caps);

}  // namespace skity

#endif  // SRC_GPU_GL_GPU_DRIVER_INFO_GL_HPP
