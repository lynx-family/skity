// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_TEXTURE_MANAGER_HPP
#define SRC_GPU_TEXTURE_MANAGER_HPP

#include <unordered_map>
#include <vector>

#include "src/gpu/gpu_texture.hpp"
#include "src/gpu/texture_impl.hpp"
#include "src/utils/annotation_mutex.hpp"
#include "src/utils/unique_id.hpp"

namespace skity {

enum class TextureState {
  Unknowing,
  Created,
  Uploaded,
};

using CreateGPUTextureCallback = std::function<std::shared_ptr<GPUTexture>()>;

class GPUDevice;

class TextureKey {
 public:
  TextureKey(TextureFormat f, size_t w, size_t h, AlphaType at, intptr_t ptr)
      : format_(f), width_(w), height_(h), alpha_type_(at), pixmap_ptr_(ptr) {
    hash_ = ComputeHash();
  }

  uint32_t hash() const { return hash_; }

  bool operator==(const TextureKey& other) const;

  struct Hash {
    std::size_t operator()(const TextureKey& key) const { return key.hash(); }
  };

  struct Equal {
    bool operator()(const TextureKey& lhs, const TextureKey& rhs) const {
      return lhs.format_ == rhs.format_ && lhs.width_ == rhs.width_ &&
             lhs.height_ == rhs.height_ && lhs.alpha_type_ == rhs.alpha_type_ &&
             lhs.pixmap_ptr_ == rhs.pixmap_ptr_;
    }
  };

 private:
  uint32_t ComputeHash() const {
    uint32_t hash = 0;
    hash = mix(hash, std::hash<uint32_t>{}(format_));
    hash = mix(hash, std::hash<size_t>{}(width_));
    hash = mix(hash, std::hash<size_t>{}(height_));
    hash = mix(hash, std::hash<uint32_t>{}(alpha_type_));
    hash = mix(hash, std::hash<intptr_t>{}(pixmap_ptr_));

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
  }

  uint32_t mix(uint32_t hash, uint32_t data) const {
    hash += data;
    hash += (hash << 10);
    hash ^= (hash >> 6);
    return hash;
  }

  TextureFormat format_;
  size_t width_;
  size_t height_;
  AlphaType alpha_type_;
  intptr_t pixmap_ptr_;
  uint32_t hash_;
};

class TextureManager : public TextureImplDelegate,
                       public std::enable_shared_from_this<TextureManager> {
 public:
  explicit TextureManager(GPUDevice* device) : gpu_device_(device) {}

  ~TextureManager() override;

  std::shared_ptr<Texture> CreateTexture(TextureFormat format, size_t width,
                                         size_t height, AlphaType alpha_type);

  // This method is intended for internal use to draw a pixmap image during
  // hardware rendering. Note that it is currently used temporarily within a
  // single frame, so no cache limit is set. In the future, we plan to change it
  // to support reuse across frames.
  std::shared_ptr<Texture> FindOrCreateTexture(TextureFormat format,
                                               size_t width, size_t height,
                                               AlphaType alpha_type,
                                               std::shared_ptr<Pixmap> pixmap);

  // TextureImplDelegate
  void UploadTextureImage(const TextureImpl& texture,
                          std::shared_ptr<Pixmap> pixmap) override;
  std::shared_ptr<GPUTexture> GetGPUTexture(TextureImpl* texture) override;
  void DropTexture(const UniqueID& handler) override;

  std::shared_ptr<Texture> RegisterTexture(
      TextureFormat format, size_t width, size_t height, AlphaType alpha_type,
      std::shared_ptr<GPUTexture> hw_texture);

  TextureState QueryState(const UniqueID& handler);

  void SaveGPUTexture(const UniqueID& handler,
                      CreateGPUTextureCallback callback);

  std::shared_ptr<GPUTexture> QueryGPUTexture(const UniqueID& handler);

  // Called by gpu thread
  void ClearGPUTextures();

 private:
  GPUDevice* gpu_device_;
  mutable SharedMutex shared_mutex_;
  std::unordered_map<UniqueID, std::shared_ptr<GPUTexture>, UniqueID::Hash,
                     UniqueID::Equal>
      handler_to_texture_ SKITY_GUARDED_BY(shared_mutex_);
  std::vector<std::shared_ptr<GPUTexture>> gpu_release_queue_
      SKITY_GUARDED_BY(shared_mutex_);

  std::unordered_map<TextureKey, std::shared_ptr<Texture>, TextureKey::Hash,
                     TextureKey::Equal>
      texture_cache_ SKITY_GUARDED_BY(shared_mutex_);
};

}  // namespace skity

#endif  // SRC_GPU_TEXTURE_MANAGER_HPP
