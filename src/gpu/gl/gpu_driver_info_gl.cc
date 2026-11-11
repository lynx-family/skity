// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gl/gpu_driver_info_gl.hpp"

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace skity {
namespace {

bool StartsWith(std::string_view value, std::string_view prefix) {
  return value.substr(0, prefix.size()) == prefix;
}

int32_t ParseAdrenoModel(std::string_view renderer) {
  for (std::string_view prefix : {"Adreno (TM) ", "FD"}) {
    if (StartsWith(renderer, prefix)) {
      int32_t model = -1;
      const auto parsed =
          std::from_chars(renderer.data() + prefix.size(),
                          renderer.data() + renderer.size(), model);
      return parsed.ec == std::errc{} ? model : -1;
    }
  }
  return -1;
}

bool NeedsIndexResourceRetention(const GLDriverInfo& info) {
  if (info.vendor_name != "Mesa/X.org" || info.renderer != "virgl") {
    return false;
  }
  // These GLES versions on Mesa 20.2.3 have reproduced stale index resources.
  // Match a complete version token, allowing the optional build identifier.
  const std::string_view version = info.version;
  for (std::string_view prefix :
       {"OpenGL ES 3.1 Mesa 20.2.3", "OpenGL ES 3.2 Mesa 20.2.3"}) {
    if (StartsWith(version, prefix) &&
        (version.size() == prefix.size() || version[prefix.size()] == ' ')) {
      return true;
    }
  }
  return false;
}

}  // namespace

GLDriverInfo GLDriverInfo::FromStrings(std::string vendor, std::string renderer,
                                       std::string version) {
  GLDriverInfo info;
  if (vendor == "PowerVR" || vendor == "Imagination Technologies") {
    info.vendor = GLVendor::kPowerVR;
  } else if (vendor == "ARM") {
    info.vendor = GLVendor::kARM;
  } else if (vendor == "Qualcomm" || vendor == "freedreno") {
    info.vendor = GLVendor::kQualcomm;
  }
  info.renderer_model = ParseAdrenoModel(renderer);
  if (info.renderer_model >= 0) {
    info.renderer_family = GLRendererFamily::kAdreno;
  } else if (StartsWith(renderer, "Mali-")) {
    info.renderer_family = GLRendererFamily::kMali;
  } else if (StartsWith(renderer, "PowerVR")) {
    info.renderer_family = GLRendererFamily::kPowerVR;
  }
  info.vendor_name = std::move(vendor);
  info.renderer = std::move(renderer);
  info.version = std::move(version);
  return info;
}

GLDriverWorkarounds ResolveGLDriverWorkarounds(
    const GLDriverInfo& driver_info) {
  const bool is_adreno =
      driver_info.renderer_family == GLRendererFamily::kAdreno;
  const int32_t model = driver_info.renderer_model;
  GLDriverWorkarounds workarounds;
  workarounds.use_draw_for_clear =
      driver_info.renderer_family == GLRendererFamily::kPowerVR;
  workarounds.disable_framebuffer_fetch =
      is_adreno && model >= 500 && model < 600;
  workarounds.disable_native_advanced_blend =
      (is_adreno && model >= 400 && model < 600) ||
      driver_info.vendor == GLVendor::kARM;
  workarounds.retain_last_index_buffer =
      NeedsIndexResourceRetention(driver_info);
  return workarounds;
}

void ApplyGLDriverWorkarounds(const GLDriverWorkarounds& workarounds,
                              GPUCaps& caps) {
  if (workarounds.disable_framebuffer_fetch) {
    caps.supports_framebuffer_fetch = false;
  }
  if (workarounds.disable_native_advanced_blend) {
    caps.supports_native_advanced_blend = false;
    caps.supports_native_advanced_blend_coherent = false;
    caps.native_blend_shader_variant = false;
  }
}

}  // namespace skity
