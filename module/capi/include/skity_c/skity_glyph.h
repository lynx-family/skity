// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_GLYPH_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_GLYPH_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_types.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque glyph data, the C handle for skity::GlyphData. Produced by the
 *        skity_font_load_glyph_* family, which fills a caller-supplied array
 *        with these handles.
 *
 * The handle is NON-OWNING: the underlying GlyphData lives in skity's global
 * scaler-context cache and stays valid only until other glyph loading work
 * evicts it. Read out what you need (metrics, path, bitmap pointer) instead of
 * holding the handle long-term. skity_glyph_data_destroy only reclaims the
 * small wrapper and never touches the cached object.
 */
SKITY_C_DEFINE_HANDLE(skity_glyph_data);

/** @brief Forward declaration; skity_path is defined in skity_path.h. */
SKITY_C_DEFINE_HANDLE(skity_path);

/** @brief Glyph raster format. Values aligned with skity::GlyphFormat. */
typedef enum {
  SKITY_GLYPH_FORMAT_A8 = 0, /**< 8-bit alpha coverage mask */
  SKITY_GLYPH_FORMAT_RGBA32, /**< 32-bit RGBA color glyph */
  SKITY_GLYPH_FORMAT_BGRA32, /**< 32-bit BGRA color glyph */
} skity_glyph_format;

/** @brief Bitmap memory layout. Values aligned with skity::BitmapFormat. */
typedef enum {
  SKITY_BITMAP_FORMAT_UNKNOWN = 0,
  SKITY_BITMAP_FORMAT_GRAY8,
  SKITY_BITMAP_FORMAT_BGRA8,
  SKITY_BITMAP_FORMAT_RGBA8,
} skity_bitmap_format;

/**
 * @brief Glyph bitmap description (copy-out projection of
 *        skity::GlyphBitmapData).
 *
 * @p buffer is borrowed from the glyph cache and stays valid for the
 * skity_glyph_data handle's documented lifetime only; it must not be freed
 * by the caller.
 */
typedef struct skity_glyph_bitmap {
  float origin_x;            /**< render origin X (canvas coordinates) */
  float origin_y;            /**< render origin Y (canvas coordinates) */
  float origin_x_for_raster; /**< origin X used by the rasterizer */
  float origin_y_for_raster; /**< origin Y used by the rasterizer */
  float width;               /**< bitmap width in pixels */
  float height;              /**< bitmap height in pixels */
  const void* buffer;        /**< pixel bytes; row-major, stride row_bytes */
  size_t row_bytes;          /**< bytes per row; 0 = tightly packed */
  skity_bitmap_format format;
} skity_glyph_bitmap;

/** @brief Reclaim a glyph-data wrapper. Safe on NULL; never frees the cached
 *         GlyphData. */
SKITY_C_API void skity_glyph_data_destroy(skity_glyph_data glyph);

/** @brief The glyph id this data was loaded for (0 on an invalid handle). */
SKITY_C_API uint16_t skity_glyph_data_get_id(skity_glyph_data glyph);

/** @brief Horizontal advance in font-size units (0 on an invalid handle). */
SKITY_C_API float skity_glyph_data_get_advance_x(skity_glyph_data glyph);

/** @brief Vertical advance in font-size units (0 on an invalid handle). */
SKITY_C_API float skity_glyph_data_get_advance_y(skity_glyph_data glyph);

/** @brief Glyph width / height in font-size units. */
SKITY_C_API float skity_glyph_data_get_width(skity_glyph_data glyph);
SKITY_C_API float skity_glyph_data_get_height(skity_glyph_data glyph);

/** @brief Left / top of the glyph outline relative to the origin. */
SKITY_C_API float skity_glyph_data_get_left(skity_glyph_data glyph);
SKITY_C_API float skity_glyph_data_get_top(skity_glyph_data glyph);

/** @brief Horizontal bearing of the glyph outline. */
SKITY_C_API float skity_glyph_data_get_hori_bearing_x(skity_glyph_data glyph);
SKITY_C_API float skity_glyph_data_get_hori_bearing_y(skity_glyph_data glyph);

/** @brief Tight vertical extent of the glyph outline. */
SKITY_C_API float skity_glyph_data_get_y_min(skity_glyph_data glyph);
SKITY_C_API float skity_glyph_data_get_y_max(skity_glyph_data glyph);

/** @brief Font size the metrics are expressed in. */
SKITY_C_API float skity_glyph_data_get_font_size(skity_glyph_data glyph);

/** @brief Fixed pixel size of a bitmap (color/emoji) font, 0 otherwise. */
SKITY_C_API float skity_glyph_data_get_fixed_size(skity_glyph_data glyph);

/**
 * @brief Fetch the glyph's raster format.
 * @return 1 and writes @p out when the format is known, 0 otherwise
 */
SKITY_C_API uint32_t skity_glyph_data_get_format(skity_glyph_data glyph,
                                                 skity_glyph_format* out);

/**
 * @brief Fetch the glyph outline as a non-owning skity_path handle.
 *
 * Requires skity_font_load_glyph_path (metrics-only loads leave the path
 * empty). The handle borrows the cached GlyphData's path — same eviction
 * caveat as the glyph handle itself; skity_path_destroy only reclaims the
 * wrapper.
 *
 * @return a non-owning skity_path handle, or NULL on an invalid handle
 */
SKITY_C_API skity_path skity_glyph_data_get_path(skity_glyph_data glyph);

/**
 * @brief Fetch the glyph's rasterized bitmap.
 *
 * Requires skity_font_load_glyph_bitmap (or _info). @p out receives a copy of
 * the bitmap description with a borrowed buffer pointer.
 *
 * @return 1 and writes @p out when a bitmap is available, 0 otherwise
 */
SKITY_C_API uint32_t skity_glyph_data_get_bitmap(skity_glyph_data glyph,
                                                 skity_glyph_bitmap* out);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_GLYPH_H
