// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GL_GPU_DRIVER_INFO_GL_HPP
#define SRC_GPU_GL_GPU_DRIVER_INFO_GL_HPP

#include <string>

namespace skity {

enum class GLVendor {
  kUnknown,
  kPowerVR,
};

struct GLDriverInfo {
  static GLDriverInfo FromStrings(std::string vendor, std::string renderer,
                                  std::string version);

  GLVendor vendor = GLVendor::kUnknown;
  std::string vendor_name;
  std::string renderer;
  std::string version;
};

struct GLDriverWorkarounds {
  bool use_draw_for_clear = false;
};

GLDriverWorkarounds ResolveGLDriverWorkarounds(const GLDriverInfo& driver_info);

}  // namespace skity

#endif  // SRC_GPU_GL_GPU_DRIVER_INFO_GL_HPP
