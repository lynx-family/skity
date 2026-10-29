// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GRAPHIC_BITMAP_HPP
#define MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GRAPHIC_BITMAP_HPP

#include <skity/c/skity_bitmap.h>

#include <cstddef>
#include <cstdint>
#include <skity/base.hpp>
#include <skity/io/data.hpp>
#include <skity/types.hpp>

namespace skity {

/**
 * @class Pixmap
 *
 * Pixel view over a buffer (legacy skity::Pixmap). Either wraps foreign
 * memory (keeps the Data alive) or borrows a bitmap's / image read's pixels;
 * the pixel pointer is exposed read-only.
 *
 */
class Pixmap : public detail::OwnHandle<skity_pixmap, skity_pixmap_destroy> {
 public:
  /**
   * Wrap a data buffer without copying. The pixmap keeps data alive, so
   * the wrapper may be released right after this call. row_bytes may
   * exceed width * bytes-per-pixel; the buffer must hold at least
   * row_bytes * height bytes.
   */
  static Pixmap MakeFromData(const std::shared_ptr<Data>& data,
                             size_t row_bytes, uint32_t width, uint32_t height,
                             AlphaType alpha_type, ColorType color_type) {
    return Pixmap(skity_pixmap_create(data ? data->get() : nullptr, row_bytes,
                                      width, height, to_c(alpha_type),
                                      to_c(color_type)));
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

/**
 * @class Bitmap
 *
 * CPU-side pixel buffer the wrapper owns (legacy skity::Bitmap).
 *
 */
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
   * buffer), the legacy constructor shape. Pass read_only false when the
   * pixels will be written through this bitmap (software rendering target);
   * empty wrapper on failure.
   */
  explicit Bitmap(std::shared_ptr<Pixmap> pixmap, bool read_only = true)
      : OwnHandle(skity_bitmap_create_from_pixmap(
            pixmap ? pixmap->get() : nullptr, read_only ? 1u : 0u)) {}

  uint32_t Width() const { return skity_bitmap_get_width(get()); }
  uint32_t Height() const { return skity_bitmap_get_height(get()); }
  size_t RowBytes() const { return skity_bitmap_get_row_bytes(get()); }

  /** Writable pixel pointer; valid for the handle's lifetime. */
  void* GetPixels() const { return skity_bitmap_get_pixels(get()); }

  /**
   * Borrow the writable pixel view of this bitmap (writes through it are
   * visible here). The returned pixmap keeps the pixels alive.
   */
  std::shared_ptr<Pixmap> GetPixmap() const {
    auto handle = skity_bitmap_get_pixmap(get());
    return handle != nullptr ? std::shared_ptr<Pixmap>(new Pixmap(handle))
                             : nullptr;
  }

 private:
  explicit Bitmap(skity_bitmap h) : OwnHandle(h) {}
};

}  // namespace skity

#endif  // MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GRAPHIC_BITMAP_HPP
