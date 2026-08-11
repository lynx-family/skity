// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_FONT_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_FONT_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_text.h>
#include <skity_c/skity_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque font, the C handle for skity::Font. Pairs a typeface with a
 *         size and a set of glyph-rendering options (hinting, edging, etc.). */
SKITY_C_DEFINE_HANDLE(skity_font);

/** @brief Glyph hinting level. Values aligned with skity::Font::FontHinting. */
typedef enum {
  SKITY_FONT_HINTING_NONE = 0, /**< glyph outlines left unchanged */
  SKITY_FONT_HINTING_SLIGHT,   /**< minimal modification to improve contrast */
  SKITY_FONT_HINTING_NORMAL, /**< glyph outlines modified to improve contrast */
  SKITY_FONT_HINTING_FULL, /**< glyph outlines adjusted for maximum contrast */
} skity_font_hinting;

/** @brief Glyph edge rendering. Values aligned with skity::Font::Edging. */
typedef enum {
  SKITY_FONT_EDGING_ALIAS = 0,  /**< opaque edges, no transparency */
  SKITY_FONT_EDGING_ANTI_ALIAS, /**< edges may use partial transparency */
  SKITY_FONT_EDGING_SUBPIXEL_ANTI_ALIAS, /**< glyph positioned at subpixel
                                            resolution */
} skity_font_edging;

/** @brief Create a font initialized to its default typeface and size. */
SKITY_C_API skity_font skity_font_create(void);

/**
 * @brief Create a font from @p typeface at @p size.
 * @param typeface  typeface to use (NULL selects the default)
 * @param size      text size in pixels; values <= 0 are ignored
 */
SKITY_C_API skity_font skity_font_create_with_typeface(skity_typeface typeface,
                                                       float size);

/**
 * @brief Create a font from @p typeface at @p size with horizontal axis
 *        transforms.
 * @param typeface  typeface to use (NULL selects the default)
 * @param size      text size in pixels; values <= 0 are ignored
 * @param scale_x   horizontal scale; 1.0 leaves glyphs unmodified
 * @param skew_x    horizontal skew; 0.0 leaves glyphs unmodified
 */
SKITY_C_API skity_font skity_font_create_with_typeface_scale(
    skity_typeface typeface, float size, float scale_x, float skew_x);

/** @brief Release a font handle. Safe on NULL. */
SKITY_C_API void skity_font_destroy(skity_font font);

/**
 * @brief Replace the typeface. Pass NULL to revert to the default typeface.
 */
SKITY_C_API void skity_font_set_typeface(skity_font font,
                                         skity_typeface typeface);

/**
 * @brief Return the typeface currently bound to this font.
 * @return a new owning handle to the typeface, or NULL if the font has no
 *         typeface assigned (e.g. default-constructed and never set). The
 *         caller must release the returned handle with
 *         skity_typeface_destroy.
 */
SKITY_C_API skity_typeface skity_font_get_typeface(skity_font font);

/** @brief Set the text size in pixels. Values <= 0 are ignored. */
SKITY_C_API void skity_font_set_size(skity_font font, float size);

/** @brief Return the current text size in pixels. */
SKITY_C_API float skity_font_get_size(skity_font font);

/** @brief Return the horizontal scale applied to glyphs (default 1.0). */
SKITY_C_API float skity_font_get_scale_x(skity_font font);

/** @brief Set the horizontal scale applied to glyphs (default 1.0). */
SKITY_C_API void skity_font_set_scale_x(skity_font font, float scale_x);

/** @brief Return the horizontal skew applied to glyphs (default 0.0). */
SKITY_C_API float skity_font_get_skew_x(skity_font font);

/** @brief Set the horizontal skew applied to glyphs (default 0.0). */
SKITY_C_API void skity_font_set_skew_x(skity_font font, float skew_x);

/**
 * @brief Fetch the font metrics (ascent, descent, leading, ...).
 * @param out  receives the metrics; must point to a valid skity_font_metrics
 */
SKITY_C_API void skity_font_get_metrics(skity_font font,
                                        skity_font_metrics* out);

/** @brief Rendering-quality switches (see skity::Font). */

/** @brief Set how aggressively glyph outlines are snapped for contrast. */
SKITY_C_API void skity_font_set_hinting(skity_font font,
                                        skity_font_hinting hinting);

/** @brief Return the current hinting level. */
SKITY_C_API skity_font_hinting skity_font_get_hinting(skity_font font);

/** @brief Set how the edge pixels of a glyph are rendered. */
SKITY_C_API void skity_font_set_edging(skity_font font,
                                       skity_font_edging edging);

/** @brief Return the current edge rendering mode. */
SKITY_C_API skity_font_edging skity_font_get_edging(skity_font font);

/**
 * @brief Toggle subpixel glyph positioning.
 * @param enable  non-zero to enable subpixel positioning
 */
SKITY_C_API void skity_font_set_subpixel(skity_font font, uint32_t enable);

/**
 * @brief Force the auto-hinter even when the font ships its own hints.
 * @param enable  non-zero to force auto-hinting
 */
SKITY_C_API void skity_font_set_force_auto_hinting(skity_font font,
                                                   uint32_t enable);

/**
 * @brief Allow embedded bitmap (EBDT/sbix) strikes to be used when available.
 * @param enable  non-zero to use embedded bitmaps
 */
SKITY_C_API void skity_font_set_embedded_bitmaps(skity_font font,
                                                 uint32_t enable);

/**
 * @brief Use linear (non-rounded) metrics so advances are not snapped to
 *        whole pixels.
 * @param enable  non-zero to enable linear metrics
 */
SKITY_C_API void skity_font_set_linear_metrics(skity_font font,
                                               uint32_t enable);

/**
 * @brief Synthesize a bolder appearance by widening glyph stems.
 * @param enable  non-zero to embolden glyphs
 */
SKITY_C_API void skity_font_set_embolden(skity_font font, uint32_t enable);

/**
 * @brief Snap the baseline to an integer pixel boundary for crisper text.
 * @param enable  non-zero to snap the baseline
 */
SKITY_C_API void skity_font_set_baseline_snap(skity_font font, uint32_t enable);

/**
 * @brief Rendering-quality switch getters (see skity::Font). Each returns 1 if
 *        the corresponding switch is enabled, 0 otherwise.
 */
/** @brief Return 1 if the auto-hinter is forced on, 0 otherwise. */
SKITY_C_API uint32_t skity_font_is_force_auto_hinting(skity_font font);

/** @brief Return 1 if embedded bitmap strikes are allowed, 0 otherwise. */
SKITY_C_API uint32_t skity_font_is_embedded_bitmaps(skity_font font);

/** @brief Return 1 if subpixel glyph positioning is enabled, 0 otherwise. */
SKITY_C_API uint32_t skity_font_is_subpixel(skity_font font);

/** @brief Return 1 if linear (non-rounded) metrics are used, 0 otherwise. */
SKITY_C_API uint32_t skity_font_is_linear_metrics(skity_font font);

/** @brief Return 1 if glyphs are emboldened, 0 otherwise. */
SKITY_C_API uint32_t skity_font_is_embolden(skity_font font);

/** @brief Return 1 if the baseline snaps to pixel boundaries, 0 otherwise. */
SKITY_C_API uint32_t skity_font_is_baseline_snap(skity_font font);

/**
 * @brief Return a copy of this font configured with @p size.
 * @param size  the new text size in pixels
 * @return a new font handle
 */
SKITY_C_API skity_font skity_font_make_with_size(skity_font font, float size);

/**
 * @brief Measure the advance width of each glyph.
 *
 * The caller must ensure @p widths points to at least @p count entries.
 *
 * @param glyphs  array of @p count glyph ids
 * @param count   number of glyphs in @p glyphs
 * @param widths  output array receiving @p count advance widths
 */
SKITY_C_API void skity_font_get_widths(skity_font font, const uint16_t* glyphs,
                                       int32_t count, float* widths);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_FONT_H
