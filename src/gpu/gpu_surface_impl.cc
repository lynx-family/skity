// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gpu_surface_impl.hpp"

#include "src/render/hw/hw_canvas.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/utils/arena_allocator.hpp"

namespace skity {

GPUSurfaceImpl::GPUSurfaceImpl(const GPUSurfaceDescriptor& desc,
                               GPUContextImpl* ctx)
    : width_(desc.width),
      height_(desc.height),
      sample_count_(desc.sample_count),
      content_scale_(desc.content_scale),
      ctx_(ctx),
      stage_buffer_(),
      canvas_() {}

GPUSurfaceImpl::~GPUSurfaceImpl() {}

Canvas* GPUSurfaceImpl::LockCanvas(bool clear) {
  if (stage_buffer_ == nullptr) {
    stage_buffer_ = std::make_unique<HWStageBuffer>(ctx_->GetGPUDevice());
  }

  if (block_cache_allocator_ == nullptr) {
    block_cache_allocator_ = std::make_unique<BlockCacheAllocator>();
  }

  if (arena_allocator_ == nullptr) {
    arena_allocator_ = std::make_unique<ArenaAllocator>(block_cache_allocator_);
  }

  if (canvas_ == nullptr) {
    canvas_ = std::make_unique<HWCanvas>(this);
  }

  auto root_layer = OnBeginNextFrame(clear);

  root_layer->SetEnableMergingDrawCall(ctx_->IsEnableMergingDrawCall());

  canvas_->BeginNewFrame(root_layer);

  return canvas_.get();
}

void GPUSurfaceImpl::Flush() {
  OnFlush();

  ctx_->GetRenderTargetCache()->PurgeAsNeeded();
  ctx_->GetTextureManager()->ClearGPUTextures();
  ctx_->GetAtlasManager()->ClearExtraRes();
  if (arena_allocator_ != nullptr) {
    arena_allocator_->Reset();
  }
}

}  // namespace skity
