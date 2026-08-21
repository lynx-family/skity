// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/texture_impl.hpp"

#include <cassert>

#include "src/logging.hpp"

namespace skity {

TextureImpl::TextureImpl(std::weak_ptr<TextureImplDelegate> delegate,
                         TextureFormat format, size_t width, size_t height,
                         AlphaType alpha_type)
    : delegate_(delegate),
      format_(format),
      width_(width),
      height_(height),
      alpha_type_(alpha_type) {}

TextureImpl::TextureImpl(std::weak_ptr<TextureImplDelegate> delegate,
                         const TextureDescriptor* desc)
    : delegate_(std::move(delegate)),
      format_(desc->format),
      width_(desc->width),
      height_(desc->height),
      alpha_type_(desc->alpha_type),
      mipmapped_(desc->mipmapped),
      mipmap_level_count_(desc->mipmap_level_count) {}

TextureImpl::~TextureImpl() {
  if (auto delegate = delegate_.lock()) {
    delegate->DropTexture(handler_);
  }
}

size_t TextureImpl::GetTextureSize() {
  size_t bytes_per_pixel = 4;  // Default to pixel size of RGBA
  switch (format_) {
    case TextureFormat::kRGBA:
    case TextureFormat::kBGRA:
      bytes_per_pixel = 4;
      break;
    case TextureFormat::kRGB:
      bytes_per_pixel = 3;
      break;
    case TextureFormat::kRGB565:
      bytes_per_pixel = 2;
      break;
    case TextureFormat::kR:
    case TextureFormat::kS:
      bytes_per_pixel = 1;
  }
  return width_ * height_ * bytes_per_pixel;
}

void TextureImpl::DeferredUploadImage(std::shared_ptr<Pixmap> pixmap) {
  pending_pixmap_ = std::move(pixmap);
}

void TextureImpl::CommitDeferredImageUpload() {
  if (!pending_pixmap_) {
    return;
  }
  auto delegate = delegate_.lock();
  if (!delegate) {
    return;
  }
  if (delegate->UploadTextureImage(*this, pending_pixmap_)) {
    // Keep the pending pixmap when the upload fails (e.g. GPU memory
    // exhaustion) so the upload can be retried on a later frame.
    pending_pixmap_.reset();
  }
}

void TextureImpl::UploadImage(std::shared_ptr<Pixmap> pixmap) {
  if (!pixmap) {
    return;
  }

  if (auto delegate = delegate_.lock()) {
    delegate->UploadTextureImage(*this, std::move(pixmap));
  }
}

std::shared_ptr<GPUTexture> TextureImpl::GetGPUTexture() {
  if (auto delegate = delegate_.lock()) {
    return delegate->GetGPUTexture(this);
  }
  // Should not reach here.
  CHECK(false);
  return nullptr;
}

TextureFormat InternalTexture::GetFormat() const {
  switch (texture_->GetDescriptor().format) {
    case GPUTextureFormat::kR8Unorm:
      return TextureFormat::kR;
    case GPUTextureFormat::kRGB8Unorm:
      return TextureFormat::kRGB;
    case GPUTextureFormat::kRGBA8Unorm:
      return TextureFormat::kRGBA;
    case GPUTextureFormat::kBGRA8Unorm:
      return TextureFormat::kBGRA;
    case GPUTextureFormat::kRGB565Unorm:
      return TextureFormat::kRGB565;
    default:
      return TextureFormat::kS;
  }
}

}  // namespace skity
