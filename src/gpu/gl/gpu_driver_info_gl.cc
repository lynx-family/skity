// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gl/gpu_driver_info_gl.hpp"

#include <utility>

namespace skity {
namespace {

GLVendor DetectGLVendor(const std::string& renderer) {
  constexpr char kPowerVRPrefix[] = "PowerVR";
  if (renderer.compare(0, sizeof(kPowerVRPrefix) - 1, kPowerVRPrefix) == 0) {
    return GLVendor::kPowerVR;
  }

  return GLVendor::kUnknown;
}

}  // namespace

GLDriverInfo GLDriverInfo::FromStrings(std::string vendor, std::string renderer,
                                       std::string version) {
  GLDriverInfo info;
  info.vendor = DetectGLVendor(renderer);
  info.vendor_name = std::move(vendor);
  info.renderer = std::move(renderer);
  info.version = std::move(version);
  return info;
}

GLDriverWorkarounds ResolveGLDriverWorkarounds(
    const GLDriverInfo& driver_info) {
  GLDriverWorkarounds workarounds;
  workarounds.use_draw_for_clear = driver_info.vendor == GLVendor::kPowerVR;
  return workarounds;
}

}  // namespace skity
