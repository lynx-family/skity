// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_RENDER_TARGET_CACHE_HPP
#define SRC_RENDER_HW_HW_RENDER_TARGET_CACHE_HPP

#include <memory>

#include "src/gpu/gpu_device.hpp"
#include "src/gpu/gpu_texture.hpp"
#include "src/render/hw/hw_resource_cache.hpp"

namespace skity {

struct HWTextureCompare {
  bool operator()(const GPUTextureDescriptor& lhs,
                  const GPUTextureDescriptor& rhs) const {
    if (lhs.width != rhs.width) {
      return lhs.width < rhs.width;
    }

    if (lhs.height != rhs.height) {
      return lhs.height < rhs.height;
    }

    if (lhs.mip_level_count != rhs.mip_level_count) {
      return lhs.mip_level_count < rhs.mip_level_count;
    }

    if (lhs.sample_count != rhs.sample_count) {
      return lhs.sample_count < rhs.sample_count;
    }

    if (lhs.format != rhs.format) {
      return lhs.format < rhs.format;
    }

    if (lhs.usage != rhs.usage) {
      return lhs.usage < rhs.usage;
    }

    if (lhs.storage_mode != rhs.storage_mode) {
      return lhs.storage_mode < rhs.storage_mode;
    }
    return false;
  }
};

class HWRenderTarget
    : public HWResource<GPUTextureDescriptor, std::shared_ptr<GPUTexture>> {
 public:
  explicit HWRenderTarget(std::shared_ptr<GPUTexture> texture)
      : texture_(texture) {}

  const GPUTextureDescriptor& GetKey() const override {
    return texture_->GetDescriptor();
  }
  std::shared_ptr<GPUTexture> GetValue() const override { return texture_; }

  size_t GetBytes() const override { return texture_->GetBytes(); }

 private:
  std::shared_ptr<GPUTexture> texture_;
};

class HWRenderTargetAllocator
    : public HWResourceAllocator<GPUTextureDescriptor,
                                 std::shared_ptr<GPUTexture>> {
 public:
  explicit HWRenderTargetAllocator(GPUDevice* device) : device_(device) {}
  std::shared_ptr<HWResource<GPUTextureDescriptor, std::shared_ptr<GPUTexture>>>
  AllocateResource(const GPUTextureDescriptor& key) override {
    auto texture = device_->CreateTexture(key);
    return std::make_shared<HWRenderTarget>(texture);
  }

 private:
  GPUDevice* device_;
};

class HWRenderTargetCache
    : public HWResourceCache<GPUTextureDescriptor, std::shared_ptr<GPUTexture>,
                             HWTextureCompare> {
 public:
  HWRenderTargetCache(std::unique_ptr<HWResourceAllocator<
                          GPUTextureDescriptor, std::shared_ptr<GPUTexture>>>
                          allocator,
                      size_t max_bytes = kDefaultMaxBytes)
      : HWResourceCache<GPUTextureDescriptor, std::shared_ptr<GPUTexture>,
                        HWTextureCompare>(std::move(allocator), max_bytes) {}

  static std::unique_ptr<HWRenderTargetCache> Create(GPUDevice* device) {
    auto allocator = std::make_unique<HWRenderTargetAllocator>(device);
    return std::make_unique<HWRenderTargetCache>(std::move(allocator));
  }
};

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_RENDER_TARGET_CACHE_HPP
