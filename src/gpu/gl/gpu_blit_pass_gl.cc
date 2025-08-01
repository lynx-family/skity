// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gl/gpu_blit_pass_gl.hpp"

#include "src/gpu/gl/formats_gl.h"
#include "src/gpu/gl/gl_interface.hpp"
#include "src/gpu/gl/gpu_sampler_gl.hpp"
#include "src/gpu/gl/gpu_texture_gl.hpp"
#include "src/tracing.hpp"

namespace skity {

void GPUBlitPassGL::UploadTextureData(std::shared_ptr<GPUTexture> texture,
                                      uint32_t offset_x, uint32_t offset_y,
                                      uint32_t width, uint32_t height,
                                      void* data) {
  auto gl_texture = static_cast<GPUTextureGL*>(texture.get());
  gl_texture->UploadData(offset_x, offset_y, width, height, data);
}

}  // namespace skity
