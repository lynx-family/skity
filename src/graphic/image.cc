
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cassert>
#include <cstring>
#include <skity/gpu/gpu_context.hpp>
#include <skity/gpu/gpu_render_target.hpp>
#include <skity/gpu/texture.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/image.hpp>
#include <skity/graphic/tile_mode.hpp>

#include "src/gpu/gpu_context_impl.hpp"
#include "src/gpu/texture_manager.hpp"
#include "src/graphic/bitmap_sampler.hpp"
#include "src/graphic/color_priv.hpp"

namespace skity {
namespace {

bool ScalePixelsForPixmap(std::shared_ptr<Pixmap> dst,
                          std::shared_ptr<Pixmap> contents,
                          const SamplingOptions& sampling_options) {
  if (!dst || dst->Width() == 0 || dst->Height() == 0 ||
      dst->GetAlphaType() == AlphaType::kUnknown_AlphaType || !contents ||
      contents->Width() == 0 || contents->Height() == 0 ||
      contents->GetAlphaType() == AlphaType::kUnknown_AlphaType) {
    return false;
  }

  Bitmap bitmap{contents, true};
  BitmapSampler bitmap_sampler{bitmap, sampling_options, TileMode::kDecal,
                               TileMode::kDecal};
  Bitmap dst_bitmap{dst, false};
  for (uint32_t x = 0; x < dst->Width(); x++) {
    for (uint32_t y = 0; y < dst->Height(); y++) {
      Vec2 uv = Vec2{x + 0.5, y + 0.5} / Vec2{dst->Width(), dst->Height()};
      Color color = bitmap_sampler.GetColor(uv);
      if (contents->GetAlphaType() != dst->GetAlphaType()) {
        if (contents->GetAlphaType() == AlphaType::kPremul_AlphaType) {
          color = PMColorToColor(color);
        } else if (dst->GetAlphaType() == AlphaType::kPremul_AlphaType) {
          color = ColorToPMColor(color);
        }
      }
      dst_bitmap.SetPixel(x, y, color);
    }
  }
  return true;
}

bool ScalePixelsForTexture(std::shared_ptr<Pixmap> dst, GPUContext* context,
                           std::shared_ptr<Texture> texture,
                           const SamplingOptions& sampling_options) {
  if (context == nullptr) {
    return false;
  }

  if (!dst || dst->Width() == 0 || dst->Height() == 0 || !texture ||
      texture->Width() == 0 || texture->Height() == 0) {
    return false;
  }

  GPURenderTargetDescriptor desc;
  desc.width = dst->Width();
  desc.height = dst->Height();
  desc.sample_count = 1;
  auto render_target = context->CreateRenderTarget(desc);
  auto canvas = render_target->GetCanvas();
  canvas->DrawImageRect(Image::MakeHWImage(texture),
                        Rect::MakeWH(texture->Width(), texture->Height()),
                        Rect::MakeWH(dst->Width(), dst->Height()),
                        sampling_options);
  std::shared_ptr<Image> snapshot =
      context->MakeSnapshot(std::move(render_target));
  auto snapshot_pixels = snapshot->ReadPixels(context);
  if (!snapshot_pixels) {
    return false;
  }
  // TODO(zhangzhijian.123): Handle ColorType and AlphaType
  std::memcpy(dst->WritableAddr(), snapshot_pixels->Addr(),
              snapshot_pixels->RowBytes() * snapshot_pixels->Height());
  return true;
}
}  // namespace

class TextureImage : public Image {
 public:
  explicit TextureImage(std::shared_ptr<Texture> texture)
      : texture_(std::move(texture)) {
    assert(texture_ != nullptr);
  }

  ~TextureImage() override = default;

  bool IsTextureBackend() const override { return true; }

  const std::shared_ptr<Texture>* GetTexture() const override {
    return &texture_;
  }

  const std::shared_ptr<Pixmap>* GetPixmap() const override { return nullptr; }

  size_t Width() const override { return texture_->Width(); }

  size_t Height() const override { return texture_->Height(); }

  AlphaType GetAlphaType() const override { return texture_->GetAlphaType(); }

  std::shared_ptr<Pixmap> ReadPixels(GPUContext* context) const override {
    if (context == nullptr || Width() == 0 || Height() == 0) {
      return nullptr;
    }
    const auto& texture_image = *GetTexture();
    auto gpu_texture = texture_image->GetGPUTexture();
    auto* gpu_context_impl = static_cast<GPUContextImpl*>(context);
    auto data = gpu_context_impl->ReadPixels(gpu_texture);

    return std::make_shared<Pixmap>(
        data, Width(), Height(), AlphaType::kPremul_AlphaType,
        Texture::FormatToColorType(texture_image->GetFormat()));
  }

  bool ScalePixels(std::shared_ptr<Pixmap> dst, GPUContext* context,
                   const SamplingOptions& sampling_options) const override {
    return ScalePixelsForTexture(dst, context, texture_, sampling_options);
  }

  ImageType GetImageType() const override { return ImageType::kTexture; }

 private:
  std::shared_ptr<Texture> texture_;
};

class PixmapImage : public Image {
 public:
  explicit PixmapImage(std::shared_ptr<Pixmap> pixmap)
      : pixmap_(std::move(pixmap)) {
    assert(pixmap_ != nullptr);
  }

  ~PixmapImage() override = default;

  bool IsTextureBackend() const override { return false; }

  const std::shared_ptr<Texture>* GetTexture() const override {
    return nullptr;
  }

  const std::shared_ptr<Pixmap>* GetPixmap() const override { return &pixmap_; }

  size_t Width() const override { return pixmap_->Width(); }

  size_t Height() const override { return pixmap_->Height(); }

  AlphaType GetAlphaType() const override { return pixmap_->GetAlphaType(); }

  bool ScalePixels(std::shared_ptr<Pixmap> dst, GPUContext* context,
                   const SamplingOptions& sampling_options) const override {
    return ScalePixelsForPixmap(dst, pixmap_, sampling_options);
  }

  ImageType GetImageType() const override { return ImageType::kPixmap; }

 private:
  std::shared_ptr<Pixmap> pixmap_;
};

std::shared_ptr<Image> Image::MakeHWImage(std::shared_ptr<Texture> texture) {
  return std::shared_ptr<Image>(new TextureImage(texture));
}

std::shared_ptr<Image> Image::MakeImage(std::shared_ptr<Pixmap> pixmap,
                                        GPUContext* context) {
  if (context) {
    auto texture = context->CreateTexture(
        Texture::FormatFromColorType(pixmap->GetColorType()), pixmap->Width(),
        pixmap->Height(), pixmap->GetAlphaType());
    texture->DeferredUploadImage(std::move(pixmap));
    return std::shared_ptr<Image>(new TextureImage(texture));
  } else {
    return std::shared_ptr<Image>(new PixmapImage(pixmap));
  }
}

std::shared_ptr<DeferredTextureImage> Image::MakeDeferredTextureImage(
    TextureFormat format, size_t width, size_t height, AlphaType alpha_type) {
  return std::shared_ptr<DeferredTextureImage>(
      new DeferredTextureImage(format, width, height, alpha_type));
}

std::shared_ptr<PromiseTextureImage> Image::MakePromiseTextureImage(
    TextureFormat format, size_t width, size_t height, AlphaType alpha_type,
    GetPromiseTexture get_promise_texture, ReleaseCallback release_callback,
    PromiseTextureContext promise_texture_context) {
  return std::shared_ptr<PromiseTextureImage>(new PromiseTextureImage(
      format, width, height, alpha_type, get_promise_texture, release_callback,
      promise_texture_context));
}

std::shared_ptr<PromiseTextureImage> Image::MakePromiseTextureImage2(
    TextureFormat format, size_t width, size_t height, AlphaType alpha_type,
    GetPromiseTexture2 get_promise_texture, ReleaseCallback release_callback,
    PromiseTextureContext promise_texture_context) {
  return std::shared_ptr<PromiseTextureImage>(new PromiseTextureImage(
      format, width, height, alpha_type, get_promise_texture, release_callback,
      promise_texture_context));
}

std::shared_ptr<Texture> PromiseTextureImage::GetTextureByContext(
    GPUContext* context) {
  if (get_promise_texture2_ == nullptr) {
    return nullptr;
  }

  if (!texture_) {
    texture_ = get_promise_texture2_(promise_texture_context_, context);
    if (!texture_) {
      return nullptr;
    }

    auto gpu_texture = texture_->GetGPUTexture();

    gpu_texture->SetRelease(release_callback_, promise_texture_context_);
  }

  return texture_;
}

const std::shared_ptr<Texture>* PromiseTextureImage::GetTexture() const {
  if (get_promise_texture_ == nullptr) {
    return nullptr;
  }

  if (!texture_) {
    texture_ = get_promise_texture_(promise_texture_context_);
    if (!texture_) {
      return nullptr;
    }
    auto gpu_texture = texture_->GetGPUTexture();
    gpu_texture->SetRelease(release_callback_, promise_texture_context_);
  }
  return &texture_;
}

}  // namespace skity
