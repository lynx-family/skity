// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_TEXTURE_IMPL_HPP
#define SRC_GPU_TEXTURE_IMPL_HPP

#include <skity/gpu/texture.hpp>
#include <unordered_map>

#include "src/gpu/gpu_texture.hpp"
#include "src/utils/annotation_mutex.hpp"
#include "src/utils/unique_id.hpp"

namespace skity {

class TextureImpl;

class TextureImplDelegate {
 public:
  virtual ~TextureImplDelegate() = default;

  virtual void UploadTextureImage(const TextureImpl& texture,
                                  std::shared_ptr<Pixmap> pixmap) = 0;
  virtual std::shared_ptr<GPUTexture> GetGPUTexture(TextureImpl* texture) = 0;
  virtual void DropTexture(const UniqueID& handler) = 0;
};

class TextureImpl : public Texture {
 public:
  TextureImpl(std::weak_ptr<TextureImplDelegate> delegate, TextureFormat format,
              size_t width, size_t height, AlphaType alpha_type);

  ~TextureImpl() override;

  void UploadImage(std::shared_ptr<Pixmap> pixmap) override;

  void DeferredUploadImage(std::shared_ptr<Pixmap> pixmap) override;
  void CommitDeferredImageUpload();

  std::shared_ptr<GPUTexture> GetGPUTexture() override;

  const UniqueID& GetHandler() const { return handler_; }

  size_t Width() override { return width_; }
  size_t Height() override { return height_; }

  AlphaType GetAlphaType() override { return alpha_type_; }
  TextureFormat GetFormat() const override { return format_; }

  size_t GetTextureSize() override;

 private:
  std::weak_ptr<TextureImplDelegate> delegate_;
  UniqueID handler_;

  TextureFormat format_;
  size_t width_ = 0;
  size_t height_ = 0;
  AlphaType alpha_type_ = AlphaType::kUnknown_AlphaType;

  std::shared_ptr<Pixmap> pending_pixmap_;
};

class InternalTexture : public Texture {
 public:
  InternalTexture(std::shared_ptr<GPUTexture> texture, AlphaType alpha_type)
      : texture_(std::move(texture)), alpha_type_(alpha_type) {}

  ~InternalTexture() override = default;

  size_t Width() override { return texture_->GetDescriptor().width; }
  size_t Height() override { return texture_->GetDescriptor().height; }

  AlphaType GetAlphaType() override { return alpha_type_; }

  TextureFormat GetFormat() const override;

  size_t GetTextureSize() override { return texture_->GetBytes(); }

  void UploadImage(std::shared_ptr<Pixmap> pixmap) override {}

  void DeferredUploadImage(std::shared_ptr<Pixmap> pixmap) override {}

  std::shared_ptr<GPUTexture> GetGPUTexture() override { return texture_; }

 private:
  std::shared_ptr<GPUTexture> texture_;
  AlphaType alpha_type_ = AlphaType::kUnknown_AlphaType;
};

}  // namespace skity

#endif  // SRC_GPU_TEXTURE_IMPL_HPP
