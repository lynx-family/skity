
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
#include "src/logging.hpp"

namespace skity {
namespace {

// ScalePixels writes pixels in the destination's declared space: the
// working space for an untagged (or sRGB) destination, or the source's own
// space when the caller resamples into a same-space destination (e.g.
// keeping a thumbnail in the source encoding). Any other declared
// destination would need a conversion between two arbitrary spaces that
// this path does not implement; fail instead of returning bytes that
// disagree with the destination tag.
bool ScaleDestinationSupported(const std::shared_ptr<const ColorSpace>& src,
                               const std::shared_ptr<const ColorSpace>& dst) {
  return ColorSpaceIsSRGB(dst) || ColorSpaceEquals(src, dst);
}

bool ScalePixelsForPixmap(std::shared_ptr<Pixmap> dst,
                          std::shared_ptr<Pixmap> contents,
                          const SamplingOptions& sampling_options) {
  if (!dst || dst->Width() == 0 || dst->Height() == 0 ||
      dst->GetAlphaType() == AlphaType::kUnknown_AlphaType || !contents ||
      contents->Width() == 0 || contents->Height() == 0 ||
      contents->GetAlphaType() == AlphaType::kUnknown_AlphaType) {
    return false;
  }
  if (!ScaleDestinationSupported(contents->GetColorSpace(),
                                 dst->GetColorSpace())) {
    return false;
  }

  Bitmap bitmap{contents, true};
  // Same-space destinations sample through without a conversion; the
  // default (untagged) destination decodes to the working space.
  BitmapSampler bitmap_sampler{bitmap, sampling_options, TileMode::kDecal,
                               TileMode::kDecal, dst->GetColorSpace()};
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
                           const std::shared_ptr<const ColorSpace>& color_space,
                           const SamplingOptions& sampling_options) {
  if (context == nullptr) {
    return false;
  }

  if (!dst || dst->Width() == 0 || dst->Height() == 0 || !texture ||
      texture->Width() == 0 || texture->Height() == 0) {
    return false;
  }
  if (!ScaleDestinationSupported(color_space, dst->GetColorSpace())) {
    return false;
  }

  // A same-space destination must receive the source-encoded bytes, so the
  // scaling draw runs without the decode (plain shader); every supported
  // other destination is the working space, which is what the decode
  // produces.
  bool keep_source_encoding =
      ColorSpaceEquals(color_space, dst->GetColorSpace());

  GPURenderTargetDescriptor desc;
  desc.width = dst->Width();
  desc.height = dst->Height();
  desc.sample_count = 1;
  auto render_target = context->CreateRenderTarget(desc);
  if (!render_target) {
    return false;
  }
  auto canvas = render_target->GetCanvas();
  // The raw texture keeps source-space encoded bytes; the image must carry
  // the source space or the scaling draw interprets them as sRGB — unless
  // the destination wants exactly those source-encoded bytes.
  canvas->DrawImageRect(
      Image::MakeHWImage(texture, keep_source_encoding ? nullptr : color_space),
      Rect::MakeWH(texture->Width(), texture->Height()),
      Rect::MakeWH(dst->Width(), dst->Height()), sampling_options);
  std::shared_ptr<Image> snapshot =
      context->MakeSnapshot(std::move(render_target));
  if (!snapshot) {
    return false;
  }
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
  explicit TextureImage(std::shared_ptr<Texture> texture,
                        std::shared_ptr<const ColorSpace> color_space)
      : Image(std::move(color_space)), texture_(std::move(texture)) {
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
    if (gpu_texture == nullptr) {
      return nullptr;
    }
    auto* gpu_context_impl = static_cast<GPUContextImpl*>(context);
    auto data = gpu_context_impl->ReadPixels(gpu_texture);
    if (data == nullptr) {
      return nullptr;
    }

    auto color_type = Texture::FormatToColorType(texture_image->GetFormat());
    // The GPU texture still holds source-space encoded bytes; the readback
    // pixmap is tagged with that source space.
    auto pixmap = std::make_shared<Pixmap>(
        data, Width(), Height(), AlphaType::kPremul_AlphaType, color_type);
    if (GetColorSpace() && !GetColorSpace()->IsSRGB()) {
      pixmap->SetColorInfo(AlphaType::kPremul_AlphaType, color_type,
                           GetColorSpace());
    }
    return pixmap;
  }

  bool ScalePixels(std::shared_ptr<Pixmap> dst, GPUContext* context,
                   const SamplingOptions& sampling_options) const override {
    return ScalePixelsForTexture(dst, context, texture_, GetColorSpace(),
                                 sampling_options);
  }

  ImageType GetImageType() const override { return ImageType::kTexture; }

 private:
  std::shared_ptr<Texture> texture_;
};

class PixmapImage : public Image {
 public:
  explicit PixmapImage(std::shared_ptr<Pixmap> pixmap)
      : Image(pixmap->GetColorSpace()), pixmap_(std::move(pixmap)) {
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
  return MakeHWImage(std::move(texture), nullptr);
}

std::shared_ptr<Image> Image::MakeHWImage(
    std::shared_ptr<Texture> texture,
    std::shared_ptr<const ColorSpace> color_space) {
  return std::shared_ptr<Image>(
      new TextureImage(std::move(texture), std::move(color_space)));
}

std::shared_ptr<Image> Image::MakeImage(std::shared_ptr<Pixmap> pixmap,
                                        GPUContext* context) {
  if (context) {
    auto color_space = pixmap->GetColorSpace();
    auto texture = context->CreateTexture(
        Texture::FormatFromColorType(pixmap->GetColorType()), pixmap->Width(),
        pixmap->Height(), pixmap->GetAlphaType());
    texture->DeferredUploadImage(std::move(pixmap));
    return std::shared_ptr<Image>(new TextureImage(texture, color_space));
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
    if (!gpu_texture) {
      // The promised texture has no GPU texture (e.g. creation failed).
      // Drop the cache so the promise can be resolved again later.
      LOGE("PromiseTextureImage: promise texture is not ready");
      texture_.reset();
      return nullptr;
    }

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
    if (!gpu_texture) {
      // The promised texture has no GPU texture (e.g. creation failed).
      // Drop the cache so the promise can be resolved again later.
      LOGE("PromiseTextureImage: promise texture is not ready");
      texture_.reset();
      return nullptr;
    }
    gpu_texture->SetRelease(release_callback_, promise_texture_context_);
  }
  return &texture_;
}

}  // namespace skity
