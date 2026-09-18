// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_IMAGE_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_IMAGE_HPP

// Image wrapper of the header-only RAII layer: a 2D pixel raster drawn to a
// canvas or sampled by a shader. Raster images copy their pixels at creation;
// deferred and promise images bind GPU textures lazily. See skity.hpp for
// the layer's design.

#include <skity_c/skity_image.h>

#include <cstddef>
#include <cstdint>
#include <skity_hpp/skity_base.hpp>
#include <skity_hpp/skity_bitmap.hpp>
#include <skity_hpp/skity_context.hpp>
#include <skity_hpp/skity_types.hpp>

namespace skity {
namespace raii {

/** Image wrapper: drawn with Canvas::DrawImage, sampled by Shader. */
class Image : public detail::OwnHandle<skity_image, skity_image_destroy> {
 public:
  /**
   * Raster image from raw pixel data; the pixels are copied, so the caller's
   * buffer may be released immediately. @p row_bytes 0 means tightly packed.
   */
  static Image MakeFromPixels(uint32_t width, uint32_t height,
                              const void* pixels, size_t row_bytes,
                              AlphaType alpha_type, ColorType color_type) {
    return Image(skity_image_create_from_pixels(
        width, height, pixels, row_bytes, to_c(alpha_type), to_c(color_type)));
  }

  /**
   * Raster image bound to a GPU context (uploadable / usable as a texture on
   * that device); otherwise identical to MakeFromPixels. A null @p context
   * degrades to the pure-CPU path.
   */
  static Image MakeFromPixels(const Context& context, uint32_t width,
                              uint32_t height, const void* pixels,
                              size_t row_bytes, AlphaType alpha_type,
                              ColorType color_type) {
    return Image(skity_image_create_from_pixels_with_context(
        width, height, pixels, row_bytes, to_c(alpha_type), to_c(color_type),
        context.get()));
  }

  /** Wrap an existing GPU texture; the texture must outlive the image. */
  static Image MakeFromTexture(skity_texture texture) {
    return Image(skity_image_create_from_texture(texture));
  }

  /**
   * Deferred-upload placeholder: format and size are known but the GPU
   * texture is bound later with SetDeferredTexture.
   */
  static Image MakeDeferred(skity_texture_format format, uint32_t width,
                            uint32_t height, AlphaType alpha_type) {
    return Image(skity_image_create_deferred(format, width, height,
                                              to_c(alpha_type)));
  }

  /**
   * Promise-texture image: @p get_texture is invoked whenever skity needs
   * the current GPU texture (video / camera frames); @p release when the
   * promise is no longer needed. The returned texture handle is shared.
   */
  static Image MakePromise(skity_texture_format format, uint32_t width,
                           uint32_t height, AlphaType alpha_type,
                           skity_promise_texture_callback get_texture,
                           skity_promise_release_callback release,
                           void* userdata) {
    return Image(skity_image_create_promise(format, width, height,
                                             to_c(alpha_type), get_texture,
                                             release, userdata));
  }

  /** Promise variant whose callback also receives the GPU context in use. */
  static Image MakePromise2(skity_texture_format format, uint32_t width,
                            uint32_t height, AlphaType alpha_type,
                            skity_promise_texture_callback2 get_texture,
                            skity_promise_release_callback release,
                            void* userdata) {
    return Image(skity_image_create_promise2(format, width, height,
                                              to_c(alpha_type), get_texture,
                                              release, userdata));
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
  Pixmap ReadPixels(const Context& context) const {
    return Pixmap(skity_image_read_pixels(get(), context.get()));
  }

  /** Resample into @p dst, scaling to fill it. */
  bool ScalePixels(Pixmap& dst, const Context& context,
                   const SamplingOptions& sampling) const {
    return skity_image_scale_pixels(get(), dst.get(), context.get(),
                                    &sampling) != 0;
  }
  bool ScalePixels(Pixmap& dst, const Context& context) const {
    return skity_image_scale_pixels(get(), dst.get(), context.get(),
                                    nullptr) != 0;
  }

 private:
  explicit Image(skity_image h) : OwnHandle(h) {}
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_IMAGE_HPP
