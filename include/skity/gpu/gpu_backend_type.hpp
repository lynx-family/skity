// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GPU_GPU_BACKEND_TYPE_HPP
#define INCLUDE_SKITY_GPU_GPU_BACKEND_TYPE_HPP

namespace skity {

/**
 * @enum GPUBackendType indicate the under GPU backend type Skity used
 */
enum class GPUBackendType {
  /**
   * empty type, used to mock GPUContext in test code
   */
  kNone,
  /**
   * OpenGL and OpenGLES backend type, can be used in desktop and mobile device
   *  support OpenGL 3.3 in desktop environment
   *  support OpenGLES 3.0 in mobile device
   */
  kOpenGL,
  /**
   * Vulkan backend type, can be used in desktop and Android device
   */
  kVulkan,
  /**
   * WebGL 2.0 backend type, used in webassembly environment
   *
   * @note Skity not support WebGL1
   */
  kWebGL2,
  /**
   * WebGPU backend type, used in webassembly environment
   *
   * @note This backend is experimental, and not all features are supported.
   */
  kWebGPU,
  /**
   * Metal backend type, used in MacOS and iOS
   */
  kMetal,
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GPU_GPU_BACKEND_TYPE_HPP
