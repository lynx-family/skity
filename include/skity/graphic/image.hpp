// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GRAPHIC_IMAGE_HPP
#define INCLUDE_SKITY_GRAPHIC_IMAGE_HPP

#include <memory>
#include <skity/gpu/texture.hpp>
#include <skity/graphic/sampling_options.hpp>
#include <skity/io/pixmap.hpp>
#include <skity/macros.hpp>

namespace skity {

class DeferredTextureImage;
class PromiseTextureImage;

class GPUContext;

using PromiseTextureContext = void*;
using GetPromiseTexture = std::shared_ptr<Texture> (*)(PromiseTextureContext);
using GetPromiseTexture2 = std::shared_ptr<Texture> (*)(PromiseTextureContext,
                                                        GPUContext*);

enum class SKITY_API ImageType {
  kUnknown,
  kCustom,
  kPixmap,
  kTexture,
  kDeferredTexture,
  kPromiseTexture,
};

class SKITY_API Image {
 public:
  static std::shared_ptr<Image> MakeHWImage(std::shared_ptr<Texture> texture);

  static std::shared_ptr<Image> MakeImage(std::shared_ptr<Pixmap> pixmap,
                                          GPUContext* context = nullptr);

  static std::shared_ptr<DeferredTextureImage> MakeDeferredTextureImage(
      TextureFormat format, size_t width, size_t height, AlphaType alpha_type);

  /**
   * @deprecated use MakePromiseTextureImage2 instead
   */
  static std::shared_ptr<PromiseTextureImage> MakePromiseTextureImage(
      TextureFormat format, size_t width, size_t height, AlphaType alpha_type,
      GetPromiseTexture get_promise_texture, ReleaseCallback release_callback,
      PromiseTextureContext promise_texture_context);

  static std::shared_ptr<PromiseTextureImage> MakePromiseTextureImage2(
      TextureFormat format, size_t width, size_t height, AlphaType alpha_type,
      GetPromiseTexture2 get_promise_texture, ReleaseCallback release_callback,
      PromiseTextureContext promise_texture_context);

  virtual ~Image() = default;

  virtual bool IsTextureBackend() const = 0;

  virtual std::shared_ptr<Texture> GetTextureByContext(GPUContext* context) {
    return nullptr;
  }

  virtual const std::shared_ptr<Texture>* GetTexture() const = 0;

  virtual const std::shared_ptr<Pixmap>* GetPixmap() const = 0;

  virtual size_t Width() const = 0;

  virtual size_t Height() const = 0;

  virtual AlphaType GetAlphaType() const = 0;

  virtual std::shared_ptr<Pixmap> ReadPixels(GPUContext* context) const {
    return nullptr;
  }

  virtual bool ScalePixels(std::shared_ptr<Pixmap> dst, GPUContext* context,
                           const SamplingOptions& sampling_options) const {
    return false;
  }

  bool IsLazy() const { return GetImageType() == ImageType::kPromiseTexture; }

  virtual ImageType GetImageType() const { return ImageType::kUnknown; }
};

class SKITY_API DeferredTextureImage : public Image {
 public:
  ~DeferredTextureImage() override = default;

  bool IsTextureBackend() const override { return true; }

  const std::shared_ptr<Texture>* GetTexture() const override {
    return texture_ ? &texture_ : nullptr;
  }

  const std::shared_ptr<Pixmap>* GetPixmap() const override { return nullptr; }

  size_t Width() const override { return width_; }

  size_t Height() const override { return height_; }

  AlphaType GetAlphaType() const override { return alpha_type_; }

  TextureFormat GetFormat() const { return format_; }

  void SetTexture(std::shared_ptr<Texture> texture) {
    texture_ = std::move(texture);
  }

  void SetAlphaType(AlphaType alpha_type) { alpha_type_ = alpha_type; }

  ImageType GetImageType() const override {
    return ImageType::kDeferredTexture;
  }

 private:
  DeferredTextureImage(TextureFormat format, size_t width, size_t height,
                       AlphaType alpha_type)
      : format_(format),
        width_(width),
        height_(height),
        alpha_type_(alpha_type) {}

  TextureFormat format_;
  size_t width_;
  size_t height_;
  AlphaType alpha_type_;
  std::shared_ptr<Texture> texture_;

  friend class Image;
};

class SKITY_API PromiseTextureImage : public Image {
 public:
  ~PromiseTextureImage() override = default;

  bool IsTextureBackend() const override { return true; }

  std::shared_ptr<Texture> GetTextureByContext(GPUContext* context) override;

  const std::shared_ptr<Texture>* GetTexture() const override;

  const std::shared_ptr<Pixmap>* GetPixmap() const override { return nullptr; }

  size_t Width() const override { return width_; }

  size_t Height() const override { return height_; }

  AlphaType GetAlphaType() const override { return alpha_type_; }

  TextureFormat GetFormat() const { return format_; }

  void SetAlphaType(AlphaType alpha_type) { alpha_type_ = alpha_type; }

  ImageType GetImageType() const override { return ImageType::kPromiseTexture; }

 private:
  PromiseTextureImage(TextureFormat format, size_t width, size_t height,
                      AlphaType alpha_type,
                      GetPromiseTexture get_promise_texture,
                      ReleaseCallback release_callback,
                      PromiseTextureContext promise_texture_context)
      : format_(format),
        width_(width),
        height_(height),
        alpha_type_(alpha_type),
        get_promise_texture_(get_promise_texture),
        get_promise_texture2_(nullptr),
        release_callback_(release_callback),
        promise_texture_context_(promise_texture_context) {}

  PromiseTextureImage(TextureFormat format, size_t width, size_t height,
                      AlphaType alpha_type,
                      GetPromiseTexture2 get_promise_texture,
                      ReleaseCallback release_callback,
                      PromiseTextureContext promise_texture_context)
      : format_(format),
        width_(width),
        height_(height),
        alpha_type_(alpha_type),
        get_promise_texture_(nullptr),
        get_promise_texture2_(get_promise_texture),
        release_callback_(release_callback),
        promise_texture_context_(promise_texture_context) {}

  TextureFormat format_;
  size_t width_;
  size_t height_;
  AlphaType alpha_type_;
  mutable std::shared_ptr<Texture> texture_;
  GetPromiseTexture get_promise_texture_;
  GetPromiseTexture2 get_promise_texture2_;
  ReleaseCallback release_callback_;
  PromiseTextureContext promise_texture_context_;

  friend class Image;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GRAPHIC_IMAGE_HPP
