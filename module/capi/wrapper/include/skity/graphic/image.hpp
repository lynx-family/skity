// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GRAPHIC_IMAGE_HPP
#define MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GRAPHIC_IMAGE_HPP

#include <skity/c/skity_image.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <skity/base.hpp>
#include <skity/gpu/context.hpp>
#include <skity/gpu/texture.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/types.hpp>

namespace skity {

/**
 * @class Image
 *
 * Drawn with Canvas::DrawImage, sampled by Shader.
 *
 */
class Image : public detail::OwnHandle<skity_image, skity_image_destroy> {
 public:
  /**
   * Raster image from raw pixel data; the pixels are copied, so the caller's
   * buffer may be released immediately. row_bytes 0 means tightly packed.
   */
  static std::shared_ptr<Image> MakeFromPixels(uint32_t width, uint32_t height,
                                               const void* pixels,
                                               size_t row_bytes,
                                               AlphaType alpha_type,
                                               ColorType color_type) {
    auto handle = skity_image_create_from_pixels(
        width, height, pixels, row_bytes, to_c(alpha_type), to_c(color_type));
    return handle != nullptr ? std::shared_ptr<Image>(new Image(handle))
                             : nullptr;
  }

  /**
   * Raster image bound to a GPU context (uploadable / usable as a texture on
   * that device); otherwise identical to MakeFromPixels. A null context
   * degrades to the pure-CPU path.
   */
  static std::shared_ptr<Image> MakeFromPixels(const Context& context,
                                               uint32_t width, uint32_t height,
                                               const void* pixels,
                                               size_t row_bytes,
                                               AlphaType alpha_type,
                                               ColorType color_type) {
    auto handle = skity_image_create_from_pixels_with_context(
        width, height, pixels, row_bytes, to_c(alpha_type), to_c(color_type),
        context.get());
    return handle != nullptr ? std::shared_ptr<Image>(new Image(handle))
                             : nullptr;
  }

  /** Wrap an existing GPU texture (legacy MakeHWImage shape). */
  static std::shared_ptr<Image> MakeFromTexture(
      std::shared_ptr<Texture> texture) {
    auto handle =
        skity_image_create_from_texture(texture ? texture->get() : nullptr);
    return handle != nullptr ? std::shared_ptr<Image>(new Image(handle))
                             : nullptr;
  }

  /**
   * Deferred-upload placeholder: format and size are known but the GPU
   * texture is bound later with SetDeferredTexture.
   */
  static std::shared_ptr<Image> MakeDeferred(skity_texture_format format,
                                             uint32_t width, uint32_t height,
                                             AlphaType alpha_type) {
    auto handle =
        skity_image_create_deferred(format, width, height, to_c(alpha_type));
    return handle != nullptr ? std::shared_ptr<Image>(new Image(handle))
                             : nullptr;
  }

  /**
   * Promise-texture image: get_texture is invoked whenever skity needs
   * the current GPU texture (video / camera frames); release when the
   * promise is no longer needed. The returned texture handle is shared.
   */
  static std::shared_ptr<Image> MakePromise(
      skity_texture_format format, uint32_t width, uint32_t height,
      AlphaType alpha_type, skity_promise_texture_callback get_texture,
      skity_promise_release_callback release, void* userdata) {
    auto handle =
        skity_image_create_promise(format, width, height, to_c(alpha_type),
                                   get_texture, release, userdata);
    return handle != nullptr ? std::shared_ptr<Image>(new Image(handle))
                             : nullptr;
  }

  /** Promise variant whose callback also receives the GPU context in use. */
  static std::shared_ptr<Image> MakePromise2(
      skity_texture_format format, uint32_t width, uint32_t height,
      AlphaType alpha_type, skity_promise_texture_callback2 get_texture,
      skity_promise_release_callback release, void* userdata) {
    auto handle =
        skity_image_create_promise2(format, width, height, to_c(alpha_type),
                                    get_texture, release, userdata);
    return handle != nullptr ? std::shared_ptr<Image>(new Image(handle))
                             : nullptr;
  }

  /** Bind a texture to a deferred image; no-op semantics on other kinds. */
  void SetDeferredTexture(skity_texture texture) {
    skity_image_deferred_set_texture(get(), texture);
  }

  uint32_t Width() const { return skity_image_get_width(get()); }
  uint32_t Height() const { return skity_image_get_height(get()); }

  /**
   * Read pixels back to CPU memory. GPU-backed images require a live
   * context; returns an empty wrapper on failure.
   */
  std::shared_ptr<Pixmap> ReadPixels(const Context& context) const {
    auto handle = skity_image_read_pixels(get(), context.get());
    return handle != nullptr ? std::shared_ptr<Pixmap>(new Pixmap(handle))
                             : nullptr;
  }

  /** Resample into dst, scaling to fill it (legacy shared_ptr shape). */
  bool ScalePixels(const std::shared_ptr<Pixmap>& dst, const Context& context,
                   const SamplingOptions& sampling) const {
    return skity_image_scale_pixels(get(), dst ? dst->get() : nullptr,
                                    context.get(), &sampling) != 0;
  }
  bool ScalePixels(Pixmap& dst, const Context& context) const {
    return skity_image_scale_pixels(get(), dst.get(), context.get(), nullptr) !=
           0;
  }

  /** How the image's alpha channel is interpreted. */
  AlphaType GetAlphaType() const {
    return static_cast<AlphaType>(skity_image_get_alpha_type(get()));
  }

  /** Whether the image is backed by a GPU texture. */
  bool IsTextureBackend() const {
    return skity_image_is_texture_backend(get()) != 0u;
  }

  /** Origin / backing classification, mirroring the legacy ImageType. */
  enum class ImageType : uint32_t {
    kUnknown = SKITY_IMAGE_TYPE_UNKNOWN,
    kCustom = SKITY_IMAGE_TYPE_CUSTOM,
    kPixmap = SKITY_IMAGE_TYPE_PIXMAP,
    kTexture = SKITY_IMAGE_TYPE_TEXTURE,
    kDeferredTexture = SKITY_IMAGE_TYPE_DEFERRED_TEXTURE,
    kPromiseTexture = SKITY_IMAGE_TYPE_PROMISE_TEXTURE,
  };

  ImageType GetImageType() const {
    return static_cast<ImageType>(skity_image_get_image_type(get()));
  }

  /** Whether the pixels are not yet available (promise / deferred). */
  bool IsLazy() const { return skity_image_is_lazy(get()) != 0u; }

 private:
  explicit Image(skity_image h) : OwnHandle(h) {}
};

}  // namespace skity

#endif  // MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GRAPHIC_IMAGE_HPP
