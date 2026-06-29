// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GPU_CAPS_HPP
#define SRC_GPU_GPU_CAPS_HPP

#include <cstdint>

namespace skity {

struct GPUCaps {
  bool supports_framebuffer_fetch = false;
  bool supports_host_visible_buffer = false;
  bool supports_native_advanced_blend = false;
  // true => advanced blend is coherent, no per-draw barrier required.
  bool supports_native_advanced_blend_coherent = false;
  // Native advanced blend needs a distinct shader variant only on GL (it must
  // inject #extension GL_KHR_blend_equation_advanced). Vulkan/Metal express it
  // purely through pipeline blend state, so their native-blend fragment shader
  // is identical to the plain one and must NOT occupy a separate cache slot.
  bool native_blend_shader_variant = false;
};

}  // namespace skity

#endif  // SRC_GPU_GPU_CAPS_HPP
