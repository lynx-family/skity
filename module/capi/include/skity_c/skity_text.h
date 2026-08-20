// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_TEXT_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_TEXT_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque typeface, the C handle for skity::Typeface. Describes a
 *         single font face and provides Unicode-code-point-to-glyph mapping
 *         and table access. */
SKITY_C_DEFINE_HANDLE(skity_typeface);

/** @brief Opaque font manager, the C handle for skity::FontManager. Enumerates
 *         installed font families and instantiates typefaces from them or
 *         from font files. */
SKITY_C_DEFINE_HANDLE(skity_font_manager);

/** @brief Forward declaration; skity_data is defined in skity_data.h. */
SKITY_C_DEFINE_HANDLE(skity_data);

/**
 * @brief Return the default system font manager.
 *
 * The handle is reference-counted internally; call
 * skity_font_manager_destroy when it is no longer needed.
 */
SKITY_C_API skity_font_manager skity_font_manager_ref_default(void);

/** @brief Release a font manager handle returned by the manager constructors.
 *         Safe on NULL. */
SKITY_C_API void skity_font_manager_destroy(skity_font_manager manager);

/**
 * @brief Load a typeface from a font file.
 * @param path  filesystem path to the font file
 * @return the new typeface, or NULL on failure
 */
SKITY_C_API skity_typeface skity_typeface_make_from_file(const char* path);

/**
 * @brief Load a typeface from an in-memory font blob.
 *
 * The new typeface takes a reference to @p data; the caller may release the
 * data handle afterwards.
 *
 * @param data  font file bytes loaded into memory (e.g. via skity_data_make_*)
 * @return the new typeface, or NULL on failure
 */
SKITY_C_API skity_typeface skity_typeface_make_from_data(skity_data data);

/** @brief Return the default normal typeface. Never returns NULL. */
SKITY_C_API skity_typeface skity_typeface_get_default(void);

/** @brief Release a typeface handle. Safe on NULL. */
SKITY_C_API void skity_typeface_destroy(skity_typeface typeface);

/**
 * @brief Map an array of Unicode code points to glyph ids.
 *
 * Each code point without a matching glyph is written as 0. The caller must
 * ensure @p glyphs points to at least @p count entries.
 *
 * @param typeface  the typeface to query
 * @param uni       array of @p count Unicode code points
 * @param count     number of code points in @p uni
 * @param glyphs    output array receiving @p count glyph ids
 */
SKITY_C_API void skity_typeface_unichars_to_glyphs(skity_typeface typeface,
                                                   const uint32_t* uni,
                                                   int32_t count,
                                                   uint16_t* glyphs);

/**
 * @brief Convenience wrapper mapping a single code point to a glyph id.
 * @return the glyph id, or 0 if the code point is unmapped
 */
SKITY_C_API uint16_t skity_typeface_unichar_to_glyph(skity_typeface typeface,
                                                     uint32_t unichar);

/** @brief Text blob handles and constructors. */

/** @brief Forward declaration; skity_paint is defined in skity_paint.h. */
SKITY_C_DEFINE_HANDLE(skity_paint);

/** @brief Forward declaration; skity_font is defined in skity_font.h. */
SKITY_C_DEFINE_HANDLE(skity_font);

/** @brief Opaque text blob, the C handle for skity::TextBlob. An immutable
 *         container of pre-laid-out text runs built from a UTF-8 string. */
SKITY_C_DEFINE_HANDLE(skity_text_blob);

/** @brief Opaque typeface delegate, the C handle for skity::TypefaceDelegate.
 *         Decides which typeface renders a code point the paint's typeface
 *         does not cover, enabling multi-font fallback (CJK / emoji mixing). */
SKITY_C_DEFINE_HANDLE(skity_typeface_delegate);

/**
 * @brief Create a fallback delegate over an ordered typeface list.
 *
 * For a code point not covered by the paint's typeface, the first typeface in
 * @p typefaces that contains a glyph for it is used; a code point covered by
 * none of them is dropped. Mirrors
 * skity::TypefaceDelegate::CreateSimpleFallbackDelegate.
 *
 * The delegate holds shared references to the typefaces, so the passed handles
 * may be released immediately afterwards.
 *
 * @param typefaces  array of fallback typefaces, probed in order
 * @param count      number of entries in @p typefaces
 * @return           delegate handle, or NULL when @p typefaces is NULL or
 *                   @p count is 0 (no fallback)
 */
SKITY_C_API skity_typeface_delegate skity_typeface_delegate_create_simple(
    const skity_typeface* typefaces, uint32_t count);

/**
 * @brief Fallback probe invoked when the paint's typeface has no glyph for a
 *        code point.
 *
 * The returned handle must stay valid across the delegate's lifetime (the
 * wrapper only takes a shared reference and never releases the handle);
 * returning NULL drops the code point from the layout.
 *
 * @param userdata   caller-supplied pointer passed back unchanged
 * @param code_point Unicode code point needing a fallback typeface
 * @return           a typeface handle, or NULL if no fallback applies
 */
typedef skity_typeface (*skity_typeface_fallback_fn)(void* userdata,
                                                     uint32_t code_point);

/**
 * @brief Create a fallback delegate driven by a C callback.
 *
 * Text-run splitting follows the built-in policy (same as
 * skity_typeface_delegate_create_simple); only the typeface choice is
 * delegated to @p fallback.
 *
 * @param fallback  callback deciding the fallback typeface; must not be NULL
 * @param userdata  caller-supplied pointer passed back to the callbacks
 * @param release   callback invoked with @p userdata when the delegate is
 *                  destroyed; may be NULL
 * @return          delegate handle, or NULL on invalid arguments
 */
SKITY_C_API skity_typeface_delegate skity_typeface_delegate_create_fallback(
    skity_typeface_fallback_fn fallback, void* userdata,
    void (*release)(void* userdata));

/** @brief Release the delegate handle and its underlying object. Safe on NULL.
 *         Invokes the @p release callback given to
 *         skity_typeface_delegate_create_fallback, if any. */
SKITY_C_API void skity_typeface_delegate_destroy(
    skity_typeface_delegate delegate);

/**
 * @brief Build an immutable text blob from a UTF-8 string using @p delegate
 *        for per-code-point font fallback.
 *
 * The text size and base typeface are taken from @p paint; code points the
 * paint's typeface does not cover are routed through the delegate. Passing a
 * NULL @p delegate is equivalent to skity_text_blob_create (no fallback).
 *
 * @param text      NUL-terminated UTF-8 string to lay out
 * @param paint     paint supplying the text size and base typeface
 * @param delegate  fallback policy, or NULL for none
 * @return          the new text blob, or NULL on invalid arguments (including
 *                  a paint with no typeface set)
 */
SKITY_C_API skity_text_blob skity_text_blob_create_with_delegate(
    const char* text, skity_paint paint, skity_typeface_delegate delegate);

/**
 * @brief Build an immutable text blob from a UTF-8 string.
 *
 * The text size and typeface are taken from @p paint; use
 * skity_paint_set_text_size and skity_paint_set_typeface to control the font.
 *
 * @param text   NUL-terminated UTF-8 string to lay out
 * @param paint  paint supplying the text size and typeface
 * @return the new text blob
 */
SKITY_C_API skity_text_blob skity_text_blob_create(const char* text,
                                                   skity_paint paint);

/** @brief Release a text blob handle. Safe on NULL. */
SKITY_C_API void skity_text_blob_destroy(skity_text_blob blob);

/**
 * @brief Compute the axis-aligned bounding box of a text blob.
 *
 * @param blob  the text blob to measure
 * @param out   receives the bounding rectangle; left untouched on failure
 */
SKITY_C_API void skity_text_blob_get_bounds(skity_text_blob blob,
                                            skity_rect* out);

/**
 * @brief Compute the bounding box of a raw glyph run without building a blob.
 *
 * @p glyphs, @p pos_x and @p pos_y are parallel arrays of length @p count:
 * glyph id @p glyphs[i] is positioned at (pos_x[i], pos_y[i]).
 *
 * @param count   number of glyphs in the run
 * @param glyphs  array of @p count glyph ids
 * @param pos_x   array of @p count glyph origins, X channel
 * @param pos_y   array of @p count glyph origins, Y channel
 * @param font    font describing size, typeface and hinting of the run
 * @param paint   paint supplying stroke / fill metrics
 * @param out     receives the bounding rectangle; left untouched on failure
 */
SKITY_C_API void skity_text_blob_compute_bounds(
    int32_t count, const uint16_t* glyphs, const float* pos_x,
    const float* pos_y, skity_font font, skity_paint paint, skity_rect* out);

/** @brief Font family enumeration and typeface lookup. */

/** @brief Opaque font style set, the C handle for skity::FontStyleSet. Groups
 *         the variants (weight / width / slant) of a single font family. */
SKITY_C_DEFINE_HANDLE(skity_font_style_set);

/** @brief Return the number of font families known to the manager. */
SKITY_C_API int32_t
skity_font_manager_count_families(skity_font_manager manager);

/**
 * @brief Copy the family name at @p index into @p buffer (two-pass idiom).
 *
 * Returns the name length in bytes including the NUL terminator. When @p
 * buffer is NULL or @p buffer_size is too small, nothing is written but the
 * required length is still returned, so the caller can size the buffer and
 * call again.
 *
 * @param manager      the font manager
 * @param index        family index in [0, skity_font_manager_count_families)
 * @param buffer       destination byte buffer, or NULL to query the length
 * @param buffer_size  capacity of @p buffer in bytes
 * @return family-name length in bytes including the NUL terminator
 */
SKITY_C_API int32_t
skity_font_manager_get_family_name(skity_font_manager manager, int32_t index,
                                   char* buffer, int32_t buffer_size);

/** @brief Return the style set for the family at @p index, or NULL on
 *         failure. */
SKITY_C_API skity_font_style_set
skity_font_manager_create_style_set(skity_font_manager manager, int32_t index);

/**
 * @brief Find a font family by name.
 * @param name  NUL-terminated family name
 * @return the matching style set, or NULL if the family is not found
 */
SKITY_C_API skity_font_style_set
skity_font_manager_match_family(skity_font_manager manager, const char* name);

/**
 * @brief Find the typeface within the named family closest to @p style.
 * @param name   NUL-terminated family name
 * @param style  requested font style
 * @return the matching typeface, or NULL if the family is not found
 */
SKITY_C_API skity_typeface skity_font_manager_match_family_style(
    skity_font_manager manager, const char* name, skity_font_style style);

/**
 * @brief Find a typeface in family @p name that matches @p style and provides
 *        a glyph for @p character.
 *
 * @param name         NUL-terminated family name
 * @param style        requested font style
 * @param bcp47        array of BCP-47 language tags, may be NULL
 * @param bcp47_count  number of entries in @p bcp47
 * @param character    the Unicode code point that must be covered
 * @return the matching typeface, or NULL if none is found
 */
SKITY_C_API skity_typeface skity_font_manager_match_family_style_character(
    skity_font_manager manager, const char* name, skity_font_style style,
    const char** bcp47, int32_t bcp47_count, uint32_t character);

/**
 * @brief Load a typeface from a TrueType / OpenType collection file.
 * @param path       filesystem path to the font file
 * @param ttc_index  0-based index of the face within the collection
 * @return the new typeface, or NULL on failure
 */
SKITY_C_API skity_typeface skity_font_manager_make_from_file(
    skity_font_manager manager, const char* path, int32_t ttc_index);

/**
 * @brief Load a typeface from an in-memory font blob.
 *
 * @param manager    the font manager
 * @param data       font file bytes loaded into memory
 * @param ttc_index  0-based index of the face within a TrueType / OpenType
 *                   collection (pass 0 for a plain font)
 * @return the new typeface, or NULL on failure
 */
SKITY_C_API skity_typeface skity_font_manager_make_from_data(
    skity_font_manager manager, skity_data data, int32_t ttc_index);

/**
 * @brief Return the default typeface for @p style.
 * @param style  requested font style
 * @return the default typeface, or NULL if @p manager is invalid
 */
SKITY_C_API skity_typeface skity_font_manager_get_default_typeface(
    skity_font_manager manager, skity_font_style style);

/** @brief Font style set accessors. */

/** @brief Return the number of variants in the set. */
SKITY_C_API int32_t skity_font_style_set_count(skity_font_style_set set);

/**
 * @brief Fetch the style, and optionally its display name, at @p index.
 *
 * @p out_style and @p name_buffer are both optional and may be NULL. When @p
 * name_buffer is non-NULL and @p name_size is positive, the style display
 * name is copied into it and NUL-terminated, truncated to fit @p name_size
 * bytes.
 *
 * @param set          the style set
 * @param index        variant index in [0, skity_font_style_set_count)
 * @param out_style    receives the style, or NULL to ignore
 * @param name_buffer  destination for the style display name, or NULL to ignore
 * @param name_size    capacity of @p name_buffer in bytes
 */
SKITY_C_API void skity_font_style_set_get_style(skity_font_style_set set,
                                                int32_t index,
                                                skity_font_style* out_style,
                                                char* name_buffer,
                                                int32_t name_size);

/** @brief Create the typeface for the variant at @p index, or NULL on
 *         failure. */
SKITY_C_API skity_typeface
skity_font_style_set_create_typeface(skity_font_style_set set, int32_t index);

/**
 * @brief Return the variant in the set closest to @p style.
 * @return the matching typeface, or NULL if the set is empty
 */
SKITY_C_API skity_typeface skity_font_style_set_match_style(
    skity_font_style_set set, skity_font_style style);

/** @brief Release a font style set handle. Safe on NULL. */
SKITY_C_API void skity_font_style_set_destroy(skity_font_style_set set);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_TEXT_H
