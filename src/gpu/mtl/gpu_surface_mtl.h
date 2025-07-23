// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_MTL_GPU_SURFACE_MTL_H
#define SRC_GPU_MTL_GPU_SURFACE_MTL_H

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <memory>

#include "src/gpu/gpu_surface_impl.hpp"

namespace skity {

class GPUSurfaceMTL : public GPUSurfaceImpl {
 public:
  GPUSurfaceMTL(const GPUSurfaceDescriptor& desc, GPUContextImpl* ctx,
                GPUTextureFormat format)
      : GPUSurfaceImpl(desc, ctx), format_(format) {}

  ~GPUSurfaceMTL() override = default;

  GPUTextureFormat GetGPUFormat() const override { return format_; }

  std::shared_ptr<Pixmap> ReadPixels(const skity::Rect& rect) override {
    return {};
  }

 protected:
  HWRootLayer* OnBeginNextFrame(bool clear) override;

  void OnFlush() override {}

  virtual id<MTLTexture> AcquireNextTexture() = 0;

 private:
  GPUTextureFormat format_;
};

class MTLTextureSurface : public GPUSurfaceMTL {
 public:
  MTLTextureSurface(const GPUSurfaceDescriptor& desc, GPUContextImpl* ctx,
                    id<MTLTexture> texture);

  ~MTLTextureSurface() override = default;

 protected:
  id<MTLTexture> AcquireNextTexture() override;

 private:
  id<MTLTexture> texture_;
};

class MTLLayerSurface : public GPUSurfaceMTL {
 public:
  MTLLayerSurface(const GPUSurfaceDescriptor& desc, GPUContextImpl* ctx,
                  CAMetalLayer* layer);

  ~MTLLayerSurface() override = default;

 protected:
  id<MTLTexture> AcquireNextTexture() override;

  void OnFlush() override;

 private:
  CAMetalLayer* layer_;
  id<MTLCommandQueue> queue_;
  id<CAMetalDrawable> drawable_ = nil;
};

}  // namespace skity

#endif  // SRC_GPU_MTL_GPU_SURFACE_MTL_H
