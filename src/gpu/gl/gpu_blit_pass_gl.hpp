// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_MTL_GPU_BLIT_PASS_GL_H
#define SRC_GPU_MTL_GPU_BLIT_PASS_GL_H

#include "src/gpu/gpu_blit_pass.hpp"

namespace skity {

class GPUBlitPassGL : public GPUBlitPass {
 public:
  void UploadTextureData(std::shared_ptr<GPUTexture> texture, uint32_t offset_x,
                         uint32_t offset_y, uint32_t width, uint32_t height,
                         void* data) override;
};

}  // namespace skity

#endif  // SRC_GPU_MTL_GPU_BLIT_PASS_GL_H
