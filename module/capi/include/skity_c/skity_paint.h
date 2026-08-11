// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PAINT_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PAINT_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_color_filter.h>
#include <skity_c/skity_image_filter.h>
#include <skity_c/skity_mask_filter.h>
#include <skity_c/skity_path_effect.h>
#include <skity_c/skity_shader.h>
#include <skity_c/skity_text.h>
#include <skity_c/skity_types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Paint controls the options applied when drawing geometry and text:
 *        style (fill / stroke), color, stroke parameters, anti-aliasing, blend
 *        mode, and the attached shader / filters / typeface.
 */
SKITY_C_DEFINE_HANDLE(skity_paint);

/** @brief Geometry draw style. Values aligned with skity::Paint::Style. */
typedef enum {
  SKITY_PAINT_STYLE_FILL = 0,         /**< fill the geometry */
  SKITY_PAINT_STYLE_STROKE,           /**< outline the geometry */
  SKITY_PAINT_STYLE_STROKE_AND_FILL,  /**< fill then stroke */
  SKITY_PAINT_STYLE_STROKE_THEN_FILL, /**< stroke then fill */
} skity_paint_style;

/** @brief Decoration drawn at the ends of an open contour. Values aligned
 *         with skity::Paint::Cap. */
typedef enum {
  SKITY_PAINT_CAP_BUTT = 0, /**< no extension */
  SKITY_PAINT_CAP_ROUND,    /**< cap with a circle */
  SKITY_PAINT_CAP_SQUARE,   /**< cap with a square */
} skity_paint_cap;

/** @brief Decoration drawn at the corners of a stroked shape. Values aligned
 *         with skity::Paint::Join. */
typedef enum {
  SKITY_PAINT_JOIN_MITER = 0, /**< extend to the miter limit */
  SKITY_PAINT_JOIN_ROUND,     /**< join with a circle */
  SKITY_PAINT_JOIN_BEVEL,     /**< connect the outside edges */
} skity_paint_join;

/** @brief Create a paint initialized to its defaults (fill style, black,
 *         no effects). */
SKITY_C_API skity_paint skity_paint_create(void);

/** @brief Release the paint handle and its underlying object. Safe on NULL. */
SKITY_C_API void skity_paint_destroy(skity_paint paint);

/** @brief Restore the paint to its default values. */
SKITY_C_API void skity_paint_reset(skity_paint paint);

/** @brief Set whether the geometry is filled, stroked, or both. */
SKITY_C_API void skity_paint_set_style(skity_paint paint,
                                       skity_paint_style style);

/** @brief Set the pen thickness used to outline the shape. */
SKITY_C_API void skity_paint_set_stroke_width(skity_paint paint, float width);

/** @brief Set how the beginnings and ends of open contours are drawn. */
SKITY_C_API void skity_paint_set_stroke_cap(skity_paint paint,
                                            skity_paint_cap cap);

/** @brief Set how corners are drawn when the shape is stroked. */
SKITY_C_API void skity_paint_set_stroke_join(skity_paint paint,
                                             skity_paint_join join);

/** @brief Set the limit at which a sharp corner is drawn beveled. */
SKITY_C_API void skity_paint_set_stroke_miter(skity_paint paint, float miter);

/**
 * @brief Set the alpha and RGB used for both stroking and filling.
 * @param color  unpremultiplied ARGB, packed as 0xAARRGGBB
 */
SKITY_C_API void skity_paint_set_color(skity_paint paint, skity_color color);

/**
 * @brief Set the color used when stroking. skity stores stroke and fill colors
 *        separately; this replaces only the stroke color and leaves the fill
 *        color untouched.
 * @param color  unpremultiplied ARGB, packed as 0xAARRGGBB
 */
SKITY_C_API void skity_paint_set_stroke_color(skity_paint paint,
                                              skity_color color);

/**
 * @brief Set the color used when filling. skity stores stroke and fill colors
 *        separately; this replaces only the fill color and leaves the stroke
 *        color untouched.
 * @param color  unpremultiplied ARGB, packed as 0xAARRGGBB
 */
SKITY_C_API void skity_paint_set_fill_color(skity_paint paint,
                                            skity_color color);

/** @brief Replace the alpha component of the color (0 = fully transparent,
 *         255 = fully opaque). */
SKITY_C_API void skity_paint_set_alpha(skity_paint paint, uint8_t alpha);

/** @brief Replace the alpha component as a float in [0, 1]. */
SKITY_C_API void skity_paint_set_alpha_f(skity_paint paint, float alpha);

/** @brief Set the blend mode applied when this paint draws over the
 *         destination. */
SKITY_C_API void skity_paint_set_blend_mode(skity_paint paint,
                                            skity_blend_mode mode);

/**
 * @brief Request anti-aliased edges. This is a hint and is not guaranteed.
 * @param aa  non-zero to enable
 */
SKITY_C_API void skity_paint_set_anti_alias(skity_paint paint, uint32_t aa);

/** @brief Set the text size. Values <= 0 are ignored. */
SKITY_C_API void skity_paint_set_text_size(skity_paint paint, float size);

/**
 * @brief Attach an effect or typeface. The paint holds a shared reference to
 *        the passed handle, so the handle may be destroyed immediately
 *        afterwards.
 */
SKITY_C_API void skity_paint_set_shader(skity_paint paint, skity_shader shader);
SKITY_C_API void skity_paint_set_color_filter(skity_paint paint,
                                              skity_color_filter filter);
SKITY_C_API void skity_paint_set_image_filter(skity_paint paint,
                                              skity_image_filter filter);
SKITY_C_API void skity_paint_set_mask_filter(skity_paint paint,
                                             skity_mask_filter filter);
SKITY_C_API void skity_paint_set_path_effect(skity_paint paint,
                                             skity_path_effect effect);
SKITY_C_API void skity_paint_set_typeface(skity_paint paint,
                                          skity_typeface typeface);

/** @brief Return the current color as an unpremultiplied ARGB value. */
SKITY_C_API skity_color skity_paint_get_color(skity_paint paint);

/**
 * @brief Return the stroke color as floating-point RGBA components.
 * @param out  receives the four components in RGBA order; may be NULL, in which
 *             case nothing is written
 */
SKITY_C_API void skity_paint_get_stroke_color(skity_paint paint,
                                              skity_vec4* out);

/**
 * @brief Return the fill color as floating-point RGBA components.
 * @param out  receives the four components in RGBA order; may be NULL, in which
 *             case nothing is written
 */
SKITY_C_API void skity_paint_get_fill_color(skity_paint paint, skity_vec4* out);

/** @brief Return the current draw style. */
SKITY_C_API skity_paint_style skity_paint_get_style(skity_paint paint);

/** @brief Return the pen thickness. */
SKITY_C_API float skity_paint_get_stroke_width(skity_paint paint);

/** @brief Return the limit at which a sharp corner is drawn beveled. */
SKITY_C_API float skity_paint_get_stroke_miter(skity_paint paint);

/** @brief Return how the beginnings and ends of open contours are drawn. */
SKITY_C_API skity_paint_cap skity_paint_get_stroke_cap(skity_paint paint);

/** @brief Return how corners are drawn when the shape is stroked. */
SKITY_C_API skity_paint_join skity_paint_get_stroke_join(skity_paint paint);

/** @brief Return the text size. */
SKITY_C_API float skity_paint_get_text_size(skity_paint paint);

/** @brief Return the current blend mode. */
SKITY_C_API skity_blend_mode skity_paint_get_blend_mode(skity_paint paint);

/** @brief Return the alpha component of the color (0..255). */
SKITY_C_API uint8_t skity_paint_get_alpha(skity_paint paint);

/** @brief Return 1 if anti-aliasing is requested, 0 otherwise. */
SKITY_C_API uint32_t skity_paint_is_anti_alias(skity_paint paint);

/**
 * @brief Return the attached shader, or NULL if none is set. The returned
 *        handle is owning (a new reference to the same underlying shader);
 *        release it with @ref skity_shader_destroy.
 */
SKITY_C_API skity_shader skity_paint_get_shader(skity_paint paint);

/** @brief Return the attached color filter, or NULL if none is set. The
 *         returned handle is owning; release it with @ref
 *         skity_color_filter_destroy. */
SKITY_C_API skity_color_filter skity_paint_get_color_filter(skity_paint paint);

/** @brief Return the attached image filter, or NULL if none is set. The
 *         returned handle is owning; release it with @ref
 *         skity_image_filter_destroy. */
SKITY_C_API skity_image_filter skity_paint_get_image_filter(skity_paint paint);

/** @brief Return the attached mask filter, or NULL if none is set. The
 *         returned handle is owning; release it with @ref
 *         skity_mask_filter_destroy. */
SKITY_C_API skity_mask_filter skity_paint_get_mask_filter(skity_paint paint);

/** @brief Return the attached path effect, or NULL if none is set. The
 *         returned handle is owning; release it with @ref
 *         skity_path_effect_destroy. */
SKITY_C_API skity_path_effect skity_paint_get_path_effect(skity_paint paint);

/** @brief Return the attached typeface, or NULL if none is set. The returned
 *         handle is owning; release it with @ref skity_typeface_destroy. */
SKITY_C_API skity_typeface skity_paint_get_typeface(skity_paint paint);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PAINT_H
