// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_WEB_GPU_SURFACE_WEB_HPP
#define SRC_GPU_WEB_GPU_SURFACE_WEB_HPP

#include <webgpu/webgpu.h>

#include "src/gpu/gpu_surface_impl.hpp"

namespace skity {

class GPUSurfaceImplWEB : public GPUSurfaceImpl {
 public:
  GPUSurfaceImplWEB(const GPUSurfaceDescriptor& desc, GPUContextImpl* ctx,
                    WGPUTexture texture);

  ~GPUSurfaceImplWEB() override;

  GPUTextureFormat GetGPUFormat() const override;

  std::shared_ptr<Pixmap> ReadPixels(const skity::Rect& rect) override {
    return {};
  }

 protected:
  HWRootLayer* OnBeginNextFrame(bool clear) override;

  void OnFlush() override {}

 private:
  WGPUTexture texture_;
};

}  // namespace skity

#endif  // SRC_GPU_WEB_GPU_SURFACE_WEB_HPP
