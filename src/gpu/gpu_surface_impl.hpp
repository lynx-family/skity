// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GPU_SURFACE_IMPL_HPP
#define SRC_GPU_GPU_SURFACE_IMPL_HPP

#include <skity/gpu/gpu_surface.hpp>

#include "src/gpu/gpu_context_impl.hpp"
#include "src/utils/arena_allocator.hpp"

namespace skity {

class HWCanvas;
class HWRootLayer;
class HWStageBuffer;

class GPUSurfaceImpl : public GPUSurface {
 public:
  GPUSurfaceImpl(const GPUSurfaceDescriptor& desc, GPUContextImpl* ctx);

  ~GPUSurfaceImpl() override;

  uint32_t GetWidth() const override { return width_; }

  uint32_t GetHeight() const override { return height_; }

  float ContentScale() const override { return content_scale_; }

  uint32_t GetSampleCount() const { return sample_count_; }

  Canvas* LockCanvas(bool clear) override;

  void Flush() override;

  GPUContextImpl* GetGPUContext() const { return ctx_; }

  HWStageBuffer* GetStageBuffer() const { return stage_buffer_.get(); }

  ArenaAllocator* GetArenaAllocator() const { return arena_allocator_.get(); }

  virtual GPUTextureFormat GetGPUFormat() const = 0;

  virtual bool UseFxaa() const { return false; }

 protected:
  virtual HWRootLayer* OnBeginNextFrame(bool clear) = 0;

  virtual void OnFlush() = 0;

 private:
  uint32_t width_;
  uint32_t height_;
  uint32_t sample_count_;
  float content_scale_;

  GPUContextImpl* ctx_;
  std::unique_ptr<HWStageBuffer> stage_buffer_;
  std::unique_ptr<HWCanvas> canvas_;
  std::shared_ptr<BlockCacheAllocator> block_cache_allocator_;
  std::unique_ptr<ArenaAllocator> arena_allocator_;
};

}  // namespace skity

#endif  // SRC_GPU_GPU_SURFACE_IMPL_HPP
