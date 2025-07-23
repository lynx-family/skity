// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GPU_TEXTURE_HPP
#define INCLUDE_SKITY_GPU_TEXTURE_HPP

#include <skity/gpu/gpu_backend_type.hpp>
#include <skity/io/pixmap.hpp>
#include <skity/macros.hpp>

namespace skity {

using ReleaseUserData = void*;
using ReleaseCallback = void (*)(ReleaseUserData);

class GPUTexture;

enum TextureFormat {
  kR,
  kRGB,
  kRGB565,
  kRGBA,
  kBGRA,
  kS,
};

struct GPUBackendTextureInfo {
  GPUBackendType backend = GPUBackendType::kNone;
  uint32_t width = 0;
  uint32_t height = 0;
  TextureFormat format = TextureFormat::kRGBA;
  AlphaType alpha_type = AlphaType::kPremul_AlphaType;
};

class SKITY_API Texture {
 public:
  static TextureFormat FormatFromColorType(ColorType color_type) {
    switch (color_type) {
      case ColorType::kRGBA:
        return TextureFormat::kRGBA;
      case ColorType::kBGRA:
        return TextureFormat::kBGRA;
      case ColorType::kRGB565:
        return TextureFormat::kRGB565;
      case ColorType::kA8:
        return TextureFormat::kR;
      case ColorType::kUnknown:
        return TextureFormat::kRGBA;
    }
  }

  static ColorType FormatToColorType(TextureFormat format) {
    switch (format) {
      case TextureFormat::kRGBA:
        return ColorType::kRGBA;
      case TextureFormat::kBGRA:
        return ColorType::kBGRA;
      case TextureFormat::kRGB565:
        return ColorType::kRGB565;
      case TextureFormat::kR:
        return ColorType::kA8;
      default:
        return ColorType::kUnknown;
    }
  }

  virtual ~Texture() = default;

  virtual size_t Width() = 0;
  virtual size_t Height() = 0;

  virtual AlphaType GetAlphaType() = 0;

  virtual TextureFormat GetFormat() const = 0;

  virtual size_t GetTextureSize() = 0;

  virtual void DeferredUploadImage(std::shared_ptr<Pixmap> pixmap) = 0;

  // called by gpu
  virtual void UploadImage(std::shared_ptr<Pixmap> pixmap) = 0;
  virtual std::shared_ptr<GPUTexture> GetGPUTexture() = 0;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GPU_TEXTURE_HPP
