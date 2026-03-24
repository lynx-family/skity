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

/**
 * @struct TextureDescriptor
 *
 * Describe parameters used to create a Texture instance.
 *
 * This descriptor is backend-agnostic and only contains common texture
 * creation attributes. Backend specific options should be extended through
 * backend side descriptor types when needed.
 */
struct TextureDescriptor {
  /** Texture pixel format. */
  TextureFormat format = TextureFormat::kRGBA;

  /** Texture width in pixels. */
  uint32_t width = 0;

  /** Texture height in pixels. */
  uint32_t height = 0;

  /** Alpha representation of texture contents. */
  AlphaType alpha_type = AlphaType::kPremul_AlphaType;

  /**
   * Whether mipmap should be created for this texture.
   *
   * When enabled, backend implementation should allocate mip levels and keep
   * them synchronized with level 0 texture data.
   */
  bool mipmapped = false;

  /**
   * Requested mipmap level count.
   *
   * 0 means backend should choose level count automatically (usually based on
   * texture size). Values greater than 0 request an explicit level count.
   *
   * @note This value only takes effect when mipmapped is true.
   */
  uint32_t mipmap_level_count = 0;
};

/**
 * @class Texture
 *
 * Abstract texture interface used by GPU backends.
 *
 * A Texture instance owns (or references) a backend texture resource and
 * exposes a unified upload/query API for higher level rendering modules.
 *
 * Upload flow:
 * 1. DeferredUploadImage() stores CPU-side image data for lazy upload.
 * 2. UploadImage() performs actual backend texture upload.
 *
 * Note:
 * Only Texture can use mipmaps to improve render quality when an image is
 * rendered at a small scale.
 */
class SKITY_API Texture {
 public:
  /**
   * Convert ColorType to TextureFormat.
   *
   * @param color_type input color type
   * @return mapped texture format for backend texture allocation
   */
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

  /**
   * Convert TextureFormat to ColorType.
   *
   * @param format backend texture format
   * @return mapped color type used by Pixmap/image utilities
   */
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

  virtual bool IsMipmapped() const = 0;

  virtual uint32_t GetMipmapLevelCount() const = 0;

  /**
   * Store image for deferred upload.
   *
   * This function does not require immediate backend operations. The stored
   * image is expected to be uploaded later by UploadImage().
   *
   * @param pixmap source image data
   */
  virtual void DeferredUploadImage(std::shared_ptr<Pixmap> pixmap) = 0;

  /**
   * Upload image data to backend texture immediately.
   *
   * Called by GPU pipeline when the texture resource must be available.
   *
   * @param pixmap source image data
   */
  virtual void UploadImage(std::shared_ptr<Pixmap> pixmap) = 0;
  virtual std::shared_ptr<GPUTexture> GetGPUTexture() = 0;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GPU_TEXTURE_HPP
