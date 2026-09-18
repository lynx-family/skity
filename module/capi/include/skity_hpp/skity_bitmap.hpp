// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_BITMAP_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_BITMAP_HPP

// Bitmap / Pixmap wrappers of the header-only RAII layer: CPU-side pixel
// storage and pixel views. A Bitmap owns its pixels (software rendering
// target, upload source); a Pixmap is a view — either wrapping a foreign
// buffer via Data::MakeWithProc, or borrowed read-only from an image read /
// bitmap. See skity.hpp for the layer's design.

#include <skity_c/skity_bitmap.h>

#include <cstddef>
#include <cstdint>
#include <skity_hpp/skity_base.hpp>
#include <skity_hpp/skity_data.hpp>
#include <skity_hpp/skity_types.hpp>

namespace skity {
namespace raii {

/**
 * Pixel view over a buffer (legacy skity::Pixmap). Either wraps foreign
 * memory (keeps the Data alive) or borrows a bitmap's / image read's pixels;
 * the pixel pointer is exposed read-only.
 */
class Pixmap : public detail::OwnHandle<skity_pixmap, skity_pixmap_destroy> {
 public:
  /**
   * Wrap a data buffer without copying. The pixmap keeps @p data alive, so
   * the wrapper may be released right after this call. @p row_bytes may
   * exceed width * bytes-per-pixel; the buffer must hold at least
   * @p row_bytes * height bytes.
   */
  static Pixmap MakeFromData(const Data& data, size_t row_bytes, uint32_t width,
                             uint32_t height, AlphaType alpha_type,
                             ColorType color_type) {
    return Pixmap(skity_pixmap_create(data.get(), row_bytes, width, height,
                                      to_c(alpha_type), to_c(color_type)));
  }

  uint32_t Width() const { return skity_pixmap_get_width(get()); }
  uint32_t Height() const { return skity_pixmap_get_height(get()); }
  size_t RowBytes() const { return skity_pixmap_get_row_bytes(get()); }

  /** Read-only pixel pointer; valid for the handle's lifetime. */
  const void* GetPixels() const { return skity_pixmap_get_pixels(get()); }

  /**
   * Update the alpha / color interpretation of the wrapped buffer (e.g. the
   * decoder produced premultiplied output after creation).
   */
  skity_result SetColorInfo(AlphaType alpha_type, ColorType color_type) {
    return skity_pixmap_set_color_info(get(), to_c(alpha_type),
                                       to_c(color_type));
  }

 private:
  explicit Pixmap(skity_pixmap h) : OwnHandle(h) {}
  friend class Bitmap;
  friend class Image;
  friend class Surface;
};

/** CPU-side pixel buffer the wrapper owns (legacy skity::Bitmap). */
class Bitmap : public detail::OwnHandle<skity_bitmap, skity_bitmap_destroy> {
 public:
  Bitmap() = default;

  Bitmap(uint32_t width, uint32_t height,
         AlphaType alpha_type = AlphaType::kUnpremul_AlphaType,
         ColorType color_type = ColorType::kRGBA)
      : OwnHandle(skity_bitmap_create(width, height, to_c(alpha_type),
                                      to_c(color_type))) {}

  /**
   * Wrap a pixmap's pixel memory without copying (shares ownership of the
   * buffer). Pass @p read_only false when the pixels will be written through
   * this bitmap (software rendering target).
   */
  static Bitmap MakeFromPixmap(const Pixmap& pixmap, bool read_only) {
    return Bitmap(
        skity_bitmap_create_from_pixmap(pixmap.get(), read_only ? 1u : 0u));
  }

  uint32_t Width() const { return skity_bitmap_get_width(get()); }
  uint32_t Height() const { return skity_bitmap_get_height(get()); }
  size_t RowBytes() const { return skity_bitmap_get_row_bytes(get()); }

  /** Writable pixel pointer; valid for the handle's lifetime. */
  void* GetPixels() const { return skity_bitmap_get_pixels(get()); }

  /**
   * Borrow the writable pixel view of this bitmap (writes through it are
   * visible here). The returned pixmap keeps the pixels alive.
   */
  Pixmap GetPixmap() const { return Pixmap(skity_bitmap_get_pixmap(get())); }

 private:
  explicit Bitmap(skity_bitmap h) : OwnHandle(h) {}
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_BITMAP_HPP
