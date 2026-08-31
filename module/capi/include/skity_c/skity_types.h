// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_TYPES_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_TYPES_H

#include <skity_c/skity_base.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Plain-old-data geometry types. These mirror the memory layout of the
 * corresponding C++ types (skity::Vec2/Vec3/Vec4, Rect, Matrix) exactly, so
 * they can be passed across the ABI boundary by pointer with no conversion.
 */

/** @brief 2-component single-precision vector, matching skity::Vec2. */
typedef struct skity_vec2 {
  float e[2]; /**< vector components */
} skity_vec2;

/** @brief 3-component single-precision vector, matching skity::Vec3. */
typedef struct skity_vec3 {
  float e[3]; /**< vector components */
} skity_vec3;

/** @brief 4-component single-precision vector, matching skity::Vec4. */
typedef struct skity_vec4 {
  float e[4]; /**< vector components */
} skity_vec4;

/** @brief Alias of skity_vec4; skity::Point and skity::Color4f are both
 *         aliases for Vec4. */
typedef skity_vec4 skity_point;
/** @brief Alias of skity_vec4 used for floating-point RGBA color. */
typedef skity_vec4 skity_color4f;

/** @brief Axis-aligned rectangle, matching skity::Rect. */
typedef struct skity_rect {
  float left;   /**< left edge X coordinate */
  float top;    /**< top edge Y coordinate */
  float right;  /**< right edge X coordinate */
  float bottom; /**< bottom edge Y coordinate */
} skity_rect;

/**
 * @brief 4x4 column-major matrix, matching skity::Matrix::m[16]. Indexed as
 *        column-major: element (row, col) = m[col * 4 + row].
 */
typedef struct skity_matrix {
  float m[16]; /**< column-major matrix elements */
} skity_matrix;

/** @brief 32-bit unpremultiplied ARGB color packed as 0xAARRGGBB, same as
 *         skity::Color. */
typedef uint32_t skity_color;

/**
 * @brief Blend modes applied when compositing a source color over a
 *        destination. Values are aligned with skity::BlendMode. With s =
 * source, d = destination, sa = source alpha, da = destination alpha, r =
 * result.
 */
typedef enum {
  SKITY_BLEND_MODE_CLEAR = 0, /**< r = 0 */
  SKITY_BLEND_MODE_SRC,       /**< r = s */
  SKITY_BLEND_MODE_DST,       /**< r = d */
  SKITY_BLEND_MODE_SRC_OVER,  /**< r = s + (1-sa)*d */
  SKITY_BLEND_MODE_DST_OVER,  /**< r = d + (1-da)*s */
  SKITY_BLEND_MODE_SRC_IN,    /**< r = s * da */
  SKITY_BLEND_MODE_DST_IN,    /**< r = d * sa */
  SKITY_BLEND_MODE_SRC_OUT,   /**< r = s * (1-da) */
  SKITY_BLEND_MODE_DST_OUT,   /**< r = d * (1-sa) */
  SKITY_BLEND_MODE_SRC_A_TOP, /**< r = s*da + d*(1-sa) */
  SKITY_BLEND_MODE_DST_A_TOP, /**< r = d*sa + s*(1-da) */
  SKITY_BLEND_MODE_XOR,       /**< r = s*(1-da) + d*(1-sa) */
  SKITY_BLEND_MODE_PLUS,      /**< r = min(s + d, 1) */
  SKITY_BLEND_MODE_MODULATE,  /**< r = s*d */
  SKITY_BLEND_MODE_SCREEN,    /**< r = s + d - s*d */
  SKITY_BLEND_MODE_OVERLAY, /**< multiply or screen, depending on destination */
  SKITY_BLEND_MODE_DARKEN,  /**< rc = s + d - max(s*da, d*sa), ra = SRC_OVER */
  SKITY_BLEND_MODE_LIGHTEN, /**< rc = s + d - min(s*da, d*sa), ra = SRC_OVER */
  SKITY_BLEND_MODE_COLOR_DODGE, /**< brighten destination to reflect source */
  SKITY_BLEND_MODE_COLOR_BURN,  /**< darken destination to reflect source */
  SKITY_BLEND_MODE_HARD_LIGHT,  /**< multiply or screen, depending on source */
  SKITY_BLEND_MODE_SOFT_LIGHT,  /**< lighten or darken, depending on source */
  SKITY_BLEND_MODE_DIFFERENCE,  /**< rc = s + d - 2*(min(s*da, d*sa)), ra =
                                   SRC_OVER */
  SKITY_BLEND_MODE_EXCLUSION,   /**< rc = s + d - 2*(s*d), ra = SRC_OVER */
  SKITY_BLEND_MODE_MULTIPLY,    /**< r = s*(1-da) + d*(1-sa) + s*d */
  SKITY_BLEND_MODE_HUE, /**< hue of source with saturation and luminosity of
                           destination */
  SKITY_BLEND_MODE_SATURATION, /**< saturation of source with hue and luminosity
                                  of destination */
  SKITY_BLEND_MODE_COLOR, /**< hue and saturation of source with luminosity of
                             destination */
  SKITY_BLEND_MODE_LUMINOSITY, /**< luminosity of source with hue and saturation
                                  of destination */
} skity_blend_mode;

/**
 * @brief How a shader behaves outside its defined bounds. Values are aligned
 *        with skity::TileMode.
 */
typedef enum {
  SKITY_TILE_MODE_CLAMP = 0, /**< replicate the edge color outside the bounds */
  SKITY_TILE_MODE_REPEAT, /**< repeat the shader image horizontally/vertically
                           */
  SKITY_TILE_MODE_MIRROR, /**< repeat the shader image, alternating mirrored
                             copies */
  SKITY_TILE_MODE_DECAL,  /**< draw only within the bounds; transparent black
                             outside */
} skity_tile_mode;

/**
 * @brief How to interpret the alpha component of a pixel. Values aligned with
 *        skity::AlphaType.
 */
typedef enum {
  SKITY_ALPHA_TYPE_UNKNOWN = 0, /**< uninitialized / unknown alpha treatment */
  SKITY_ALPHA_TYPE_OPAQUE,      /**< pixels are fully opaque (alpha ignored) */
  SKITY_ALPHA_TYPE_PREMUL,   /**< color components are premultiplied by alpha */
  SKITY_ALPHA_TYPE_UNPREMUL, /**< color components are independent of alpha */
} skity_alpha_type;

/**
 * @brief How pixel bits encode color. Values aligned with skity::ColorType.
 *        Currently only RGBA / BGRA are fully supported.
 */
typedef enum {
  SKITY_COLOR_TYPE_UNKNOWN = 0, /**< uninitialized */
  SKITY_COLOR_TYPE_RGBA,   /**< 8 bits each for red, green, blue, alpha (32-bit
                              word) */
  SKITY_COLOR_TYPE_BGRA,   /**< 8 bits each for blue, green, red, alpha (32-bit
                              word) */
  SKITY_COLOR_TYPE_RGB565, /**< 5/6/5-bit red/green/blue (16-bit word) */
  SKITY_COLOR_TYPE_A8,     /**< 8-bit alpha only (internal use) */
} skity_color_type;

/**
 * @brief Filter mode for image sampling. Values aligned with skity::FilterMode.
 */
typedef enum {
  SKITY_FILTER_MODE_NEAREST = 0, /**< sample the nearest single pixel */
  SKITY_FILTER_MODE_LINEAR,      /**< interpolate linearly between pixels */
} skity_filter_mode;

/**
 * @brief Mipmap mode for image sampling. Values aligned with skity::MipmapMode.
 */
typedef enum {
  SKITY_MIPMAP_MODE_NONE = 0, /**< ignore mipmaps */
  SKITY_MIPMAP_MODE_NEAREST,  /**< sample the nearest mip level */
  SKITY_MIPMAP_MODE_LINEAR,   /**< blend between the two nearest mip levels */
} skity_mipmap_mode;

/**
 * @brief Sampling options for image filtering, combining filter / mipmap modes
 *        with optional cubic coefficients.
 */
typedef struct skity_sampling_options {
  skity_filter_mode filter; /**< min/mag filter mode */
  skity_mipmap_mode mipmap; /**< mipmap sampling mode */
  float cubic_b;            /**< cubic B-spline coefficient (Mitchell b) */
  float cubic_c;            /**< cubic C-spline coefficient (Mitchell c) */
} skity_sampling_options;

/** @brief GPU backend type. Values aligned with skity::GPUBackendType. */
typedef enum {
  SKITY_GPU_BACKEND_TYPE_NONE = 0, /**< no / software backend */
  SKITY_GPU_BACKEND_TYPE_OPENGL,   /**< desktop or embedded OpenGL */
  SKITY_GPU_BACKEND_TYPE_VULKAN,   /**< Vulkan */
  SKITY_GPU_BACKEND_TYPE_WEBGL2,   /**< WebGL 2 */
  SKITY_GPU_BACKEND_TYPE_WEBGPU,   /**< WebGPU */
  SKITY_GPU_BACKEND_TYPE_METAL,    /**< Metal */
} skity_gpu_backend_type;

/**
 * @brief Structure types used in s_type / p_next chains across CreateInfo /
 *        backend info structs (shared by surface and texture).
 */
typedef enum {
  SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO =
      0, /**< base surface creation info */
  SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO_GL, /**< OpenGL-specific surface
                                                  creation info */
  SKITY_STRUCTURE_TYPE_BACKEND_TEXTURE_INFO,   /**< base backend texture info */
  SKITY_STRUCTURE_TYPE_BACKEND_TEXTURE_INFO_GL, /**< OpenGL-specific backend
                                                   texture info */
  SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO_VK,  /**< Vulkan-specific surface
                                                   creation info */
  SKITY_STRUCTURE_TYPE_BACKEND_TEXTURE_INFO_VK, /**< Vulkan-specific backend
                                                   texture info */
} skity_structure_type;

/**
 * @brief Font slant. Values aligned with skity::FontStyle::Slant.
 */
typedef enum {
  SKITY_FONT_SLANT_UPRIGHT = 0, /**< upright / roman glyphs */
  SKITY_FONT_SLANT_ITALIC,      /**< italic glyphs */
  SKITY_FONT_SLANT_OBLIQUE,     /**< oblique glyphs */
} skity_font_slant;

/**
 * @brief Font style (weight / width / slant), matching skity::FontStyle.
 */
typedef struct skity_font_style {
  int32_t weight;         /**< 0..1000 (400 = normal, 700 = bold) */
  int32_t width;          /**< 1..9 (5 = normal) */
  skity_font_slant slant; /**< glyph slant */
} skity_font_style;

/**
 * @brief Font metrics, in pixels. Values follow the standard baseline layout
 *        used by skity::FontMetrics.
 */
typedef struct skity_font_metrics {
  float top;     /**< top extent of the tallest glyph above the baseline */
  float ascent;  /**< recommended distance above the baseline */
  float descent; /**< recommended distance below the baseline (positive) */
  float bottom; /**< bottom extent of the lowest descender below the baseline */
  float leading;             /**< recommended extra line spacing */
  float avg_char_width;      /**< average character advance */
  float max_char_width;      /**< maximum character advance */
  float x_min;               /**< minimum x extent over all glyphs */
  float x_max;               /**< maximum x extent over all glyphs */
  float x_height;            /**< height of the lowercase 'x' */
  float cap_height;          /**< height of an uppercase glyph */
  float underline_thickness; /**< thickness of the underline */
  float underline_position;  /**< position of the underline relative to the
                                baseline */
  float strikeout_thickness; /**< thickness of the strikeout */
  float strikeout_position;  /**< position of the strikeout relative to the
                                baseline */
} skity_font_metrics;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_TYPES_H
