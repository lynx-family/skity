// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_TEXTURE_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_TEXTURE_HPP

// Texture wrapper of the header-only RAII layer: a GPU image allocated on a
// Context (optionally mipmapped), pixel upload, and wrapping of externally
// created backend textures via the p_next extension chain. See skity.hpp for
// the layer's design.

#include <skity_c/skity_texture.h>

#include <cstddef>
#include <cstdint>
#include <skity_hpp/skity_base.hpp>
#include <skity_hpp/skity_context.hpp>
#include <skity_hpp/skity_types.hpp>

namespace skity {
namespace raii {

/** Texture wrapper: a GPU image resource owned by a Context. */
class Texture : public detail::OwnHandle<skity_texture, skity_texture_destroy> {
 public:
  /** Pixel format, mirroring the C skity_texture_format. */
  enum class Format : uint32_t {
    kR = SKITY_TEXTURE_FORMAT_R,
    kRGB = SKITY_TEXTURE_FORMAT_RGB,
    kRGB565 = SKITY_TEXTURE_FORMAT_RGB565,
    kRGBA = SKITY_TEXTURE_FORMAT_RGBA,
    kBGRA = SKITY_TEXTURE_FORMAT_BGRA,
    kS = SKITY_TEXTURE_FORMAT_S,
  };

  /** Allocate a texture on @p context's backend; empty on failure. */
  static Texture Create(const Context& context, Format format, uint32_t width,
                        uint32_t height, AlphaType alpha_type) {
    return Adopt(skity_texture_create(
        context.get(), static_cast<skity_texture_format>(format), width, height,
        static_cast<skity_alpha_type>(alpha_type)));
  }

  /** Create from a full descriptor (mipmap control); empty on failure. */
  static Texture CreateWithDesc(const Context& context,
                                const skity_texture_descriptor& desc) {
    return Adopt(skity_texture_create_with_desc(context.get(), &desc));
  }

  /**
   * Wrap an existing backend texture as a skity texture. The backend is
   * selected by @p info's p_next extension (see skity_backend_texture_info_gl
   * / skity_backend_texture_info_vk). @p release (with @p userdata) fires
   * when skity drops its last reference; may be null.
   */
  static Texture CreateFromBackend(
      const Context& context, const skity_backend_texture_info& info,
      skity_texture_release_callback release = nullptr,
      void* userdata = nullptr) {
    return Adopt(skity_texture_create_from_backend(context.get(), &info,
                                                   release, userdata));
  }

  /** Wrap an existing handle created through the raw C API. */
  static Texture Adopt(skity_texture handle) { return Texture(handle); }

  /**
   * Upload pixel data immediately. @p pixels is copied before the upload,
   * so the caller's buffer may be released right away.
   */
  void Upload(const void* pixels, size_t row_bytes, ColorType color_type,
              AlphaType alpha_type) {
    skity_texture_upload(get(), pixels, row_bytes,
                         static_cast<skity_color_type>(color_type),
                         static_cast<skity_alpha_type>(alpha_type));
  }

  /**
   * Store pixel data for a deferred upload; the GPU upload happens when the
   * texture is first used by the rendering pipeline.
   */
  void DeferredUpload(const void* pixels, size_t row_bytes,
                      ColorType color_type, AlphaType alpha_type) {
    skity_texture_deferred_upload(get(), pixels, row_bytes,
                                  static_cast<skity_color_type>(color_type),
                                  static_cast<skity_alpha_type>(alpha_type));
  }

  uint32_t GetWidth() const { return skity_texture_get_width(get()); }
  uint32_t GetHeight() const { return skity_texture_get_height(get()); }

  /** Empty texture; filled by the Create* factories or Adopt. */
  Texture() = default;

 private:
  explicit Texture(skity_texture handle) : OwnHandle(handle) {}
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_TEXTURE_HPP
