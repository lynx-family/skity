// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/atlas/atlas_texture.hpp"

#include "src/gpu/gpu_device.hpp"

namespace skity {

AtlasTexture::AtlasTexture(uint32_t width, uint32_t height, AtlasFormat format,
                           GPUDevice* gpu_device)
    : width_(width), height_(height), format_(format), gpu_device_(gpu_device) {
  InitTexture();
}

AtlasTexture::~AtlasTexture() = default;

void AtlasTexture::UploadAtlas(uint32_t x, uint32_t y, uint32_t width,
                               uint32_t height, uint8_t* data) {
  if (valid_texture_) {
    auto cmd_buffer = gpu_device_->CreateCommandBuffer();
    auto blit_pass = cmd_buffer->BeginBlitPass();
    blit_pass->UploadTextureData(texture_, x, y, width, height, data);
    cmd_buffer->Submit();
  }
}

std::shared_ptr<GPUTexture> AtlasTexture::GetTexture() const {
  return texture_;
}

std::shared_ptr<GPUSampler> AtlasTexture::GetSampler(
    const GPUSamplerDescriptor& descriptor) const {
  return gpu_device_->CreateSampler(descriptor);
}

void AtlasTexture::InitTexture() {
  GPUTextureDescriptor descriptor;
  descriptor.width = width_;
  descriptor.height = height_;
  switch (format_) {
    case AtlasFormat::A8:
      descriptor.format = GPUTextureFormat::kR8Unorm;
      break;
    case AtlasFormat::RGBA32:
      descriptor.format = GPUTextureFormat::kRGBA8Unorm;
      break;
  }
  descriptor.usage =
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kTextureBinding);
  descriptor.storage_mode = GPUTextureStorageMode::kPrivate;
  texture_ = gpu_device_->CreateTexture(descriptor);
  valid_texture_ = true;
}

AtlasTextureArray::AtlasTextureArray(uint32_t width, uint32_t height,
                                     AtlasFormat format, GPUDevice* gpu_device)
    : width_(width),
      height_(height),
      format_(format),
      gpu_device_(gpu_device) {}

AtlasTextureArray::~AtlasTextureArray() = default;

std::array<std::shared_ptr<GPUTexture>, AtlasConfig::MAX_NUM_TEXTURE_PER_ATLAS>
AtlasTextureArray::GetTextures() const {
  std::array<std::shared_ptr<GPUTexture>,
             AtlasConfig::MAX_NUM_TEXTURE_PER_ATLAS>
      textures{};
  for (size_t index = 0; index < AtlasConfig::MAX_NUM_TEXTURE_PER_ATLAS;
       index++) {
    if (texture_array_[index]) {
      textures[index] = texture_array_[index]->GetTexture();
    }
  }
  return textures;
}

std::array<std::shared_ptr<GPUSampler>, AtlasConfig::MAX_NUM_TEXTURE_PER_ATLAS>
AtlasTextureArray::GetSamplers(const GPUSamplerDescriptor& descriptor) const {
  std::array<std::shared_ptr<GPUSampler>,
             AtlasConfig::MAX_NUM_TEXTURE_PER_ATLAS>
      samplers;
  for (size_t index = 0; index < AtlasConfig::MAX_NUM_TEXTURE_PER_ATLAS;
       index++) {
    if (texture_array_[index]) {
      samplers[index] = texture_array_[index]->GetSampler(descriptor);
    }
  }
  return samplers;
}

std::shared_ptr<GPUSampler> AtlasTextureArray::GetSampler(
    const GPUSamplerDescriptor& descriptor) const {
  // one sampler for all textures in the same group
  return texture_array_[0]->GetSampler(descriptor);
}

void AtlasTextureArray::UploadAtlas(uint32_t index, uint32_t x, uint32_t y,
                                    uint32_t width, uint32_t height,
                                    uint8_t* data) {
  if (index >= AtlasConfig::MAX_NUM_TEXTURE_PER_ATLAS) {
    return;
  }
  if (!texture_array_[index]) {
    texture_array_[index] =
        std::make_unique<AtlasTexture>(width_, height_, format_, gpu_device_);
  }
  texture_array_[index]->UploadAtlas(x, y, width, height, data);
}

}  // namespace skity
