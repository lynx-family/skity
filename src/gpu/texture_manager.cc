// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/texture_manager.hpp"

#include <algorithm>

#include "src/gpu/gpu_device.hpp"
#include "src/logging.hpp"

namespace skity {

namespace {

uint32_t max_mipmap_level(size_t size) {
  if (size <= 1) {
    return 1;
  }

  uint32_t levels = 1;
  while (size > 1) {
    size >>= 1;
    levels++;
  }

  return levels;
}

}  // namespace

TextureManager::~TextureManager() {
  UniqueLock lock(shared_mutex_);
  // Force destorying all gpu textures to avoid gpu resource leak.
  handler_to_texture_.clear();
  gpu_release_queue_.clear();
}

std::shared_ptr<Texture> TextureManager::CreateTexture(TextureFormat format,
                                                       size_t width,
                                                       size_t height,
                                                       AlphaType alpha_type) {
  return RegisterTexture(format, width, height, alpha_type, nullptr);
}

std::shared_ptr<Texture> TextureManager::FindOrCreateTexture(
    const TextureKey& key) {
  UniqueLock lock(shared_mutex_);
  auto find = texture_cache_.find(key);

  if (find == texture_cache_.end()) {
    TextureDescriptor desc{
        key.format_,
        static_cast<uint32_t>(key.width_),
        static_cast<uint32_t>(key.height_),
        key.alpha_type_,
        key.mipmapped_,
        key.mipmap_level_count_,
    };

    auto texture = std::make_shared<TextureImpl>(shared_from_this(), &desc);
    handler_to_texture_.emplace(texture->GetHandler(), nullptr);
    texture_cache_.emplace(key, texture);
    return texture;
  }
  return find->second;
}

std::shared_ptr<Texture> TextureManager::RegisterTexture(
    TextureFormat format, size_t width, size_t height, AlphaType alpha_type,
    std::shared_ptr<GPUTexture> hw_texture) {
  auto texture = std::make_shared<TextureImpl>(shared_from_this(), format,
                                               width, height, alpha_type);
  UniqueLock lock(shared_mutex_);
  handler_to_texture_.emplace(texture->GetHandler(), std::move(hw_texture));
  return std::move(texture);
}

std::shared_ptr<Texture> TextureManager::RegisterTexture(
    const TextureDescriptor* desc, std::shared_ptr<GPUTexture> hw_texture) {
  if (desc == nullptr) {
    return nullptr;
  }

  auto texture = std::make_shared<TextureImpl>(weak_from_this(), desc);

  UniqueLock lock(shared_mutex_);
  handler_to_texture_.emplace(texture->GetHandler(), std::move(hw_texture));
  return std::move(texture);
}

TextureState TextureManager::QueryState(const UniqueID& handler) {
  SharedLock lock(shared_mutex_);
  if (handler_to_texture_.find(handler) == handler_to_texture_.end()) {
    return TextureState::Unknowing;
  }
  if (handler_to_texture_[handler] == nullptr) {
    return TextureState::Created;
  }
  return TextureState::Uploaded;
}

void TextureManager::SaveGPUTexture(const UniqueID& handler,
                                    CreateGPUTextureCallback callback) {
  UniqueLock lock(shared_mutex_);
  if (handler_to_texture_.find(handler) == handler_to_texture_.end() ||
      handler_to_texture_[handler] == nullptr) {
    auto hw_texture = callback();
    handler_to_texture_.insert_or_assign(handler, std::move(hw_texture));
  }
}

std::shared_ptr<GPUTexture> TextureManager::QueryGPUTexture(
    const UniqueID& handler) {
  SharedLock lock(shared_mutex_);
  if (handler_to_texture_.find(handler) == handler_to_texture_.end()) {
    return nullptr;
  }
  return handler_to_texture_[handler] ? handler_to_texture_[handler] : nullptr;
}

void TextureManager::UploadTextureImage(const TextureImpl& texture,
                                        std::shared_ptr<Pixmap> pixmap) {
  auto device = gpu_device_;

  uint32_t mipmap_level_count = 1;
  if (texture.IsMipmapped()) {
    auto max_size = std::max(pixmap->Width(), pixmap->Height());
    auto max_level_count = max_mipmap_level(max_size);
    auto requested_level_count = texture.GetMipmapLevelCount();
    mipmap_level_count = requested_level_count == 0
                             ? max_level_count
                             : std::min(requested_level_count, max_level_count);
  }

  SaveGPUTexture(
      texture.GetHandler(),
      [device, pixmap = std::move(pixmap), format = texture.GetFormat(),
       mipmap_level_count]() -> std::shared_ptr<GPUTexture> {
        GPUTextureDescriptor descriptor;
        descriptor.width = pixmap->Width();
        descriptor.height = pixmap->Height();
        descriptor.format = static_cast<GPUTextureFormat>(format);
        descriptor.mip_level_count = mipmap_level_count;
        descriptor.usage =
            static_cast<GPUTextureUsageMask>(GPUTextureUsage::kTextureBinding) |
            static_cast<GPUTextureUsageMask>(GPUTextureUsage::kCopyDst);
        descriptor.storage_mode = GPUTextureStorageMode::kHostVisible;
        auto gpu_texture = device->CreateTexture(descriptor);
        if (!gpu_texture) {
          return nullptr;
        }

        gpu_texture->UploadData(0, 0, pixmap->Width(), pixmap->Height(),
                                const_cast<void*>(pixmap->Addr()));

        if (mipmap_level_count > 1) {
          auto cmd = device->CreateCommandBuffer();
          cmd->SetLabel("mipmap generate cmd");

          auto pass = cmd->BeginBlitPass();
          pass->GenerateMipmaps(gpu_texture);
          pass->End();

          cmd->Submit();
        }

        return gpu_texture;
      });
}

std::shared_ptr<GPUTexture> TextureManager::GetGPUTexture(
    TextureImpl* texture) {
  CHECK(texture &&
        QueryState(texture->GetHandler()) != TextureState::Unknowing);
  if (QueryState(texture->GetHandler()) == TextureState::Created) {
    texture->CommitDeferredImageUpload();
  }
  CHECK(QueryState(texture->GetHandler()) == TextureState::Uploaded);
  return QueryGPUTexture(texture->GetHandler());
}

void TextureManager::DropTexture(const UniqueID& handler) {
  UniqueLock lock(shared_mutex_);
  if (handler_to_texture_.find(handler) == handler_to_texture_.end()) {
    return;
  }
  if (handler_to_texture_[handler]) {
    gpu_release_queue_.emplace_back(std::move(handler_to_texture_[handler]));
  }
  handler_to_texture_.erase(handler);
}

void TextureManager::ClearGPUTextures() {
  // GPU textures are pushed into gpu_release_queue_ to be released.
  texture_cache_.clear();

  UniqueLock lock(shared_mutex_);
  gpu_release_queue_.clear();
}

}  // namespace skity
