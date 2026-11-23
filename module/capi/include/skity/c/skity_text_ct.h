// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_TEXT_CT_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_TEXT_CT_H

// CoreText font backend entry points (the C counterparts of
// skity::TypefaceCT). CTFontRef is a plain CoreFoundation C type, so unlike
// the Objective-C-only Metal headers these entries are usable from C, C++
// and Objective-C(++) alike. <CoreText/CoreText.h> only exists on Apple
// platforms, though, so, like the Vulkan and Metal headers, this header is
// not part of the skity.h umbrella. Builds without the CoreText font backend
// (SKITY_CT_FONT=OFF, the default) still link: the entry points return NULL
// at runtime (portable stubs in text_c.cpp).

#if !defined(__APPLE__)
#error "skity's CoreText C API can only be included on Apple platforms"
#endif

#include <CoreText/CoreText.h>
#include <skity/c/skity_text.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Wrap a CoreText font as an owning skity_typeface handle.
 *
 * Mirrors skity::TypefaceCT::TypefaceFromCTFont: the lookup goes through the
 * process-wide CoreText typeface cache, so equal fonts (CFEqual) resolve to
 * the same underlying typeface and repeated calls are cheap. Each call still
 * returns a new owning handle, to be released with skity_typeface_destroy;
 * the typeface itself stays cached for the lifetime of the process. Use
 * skity_typeface_make_from_ct_font_without_cache when the font must be
 * released with its owner instead.
 *
 * The typeface takes exactly one reference on @p ct_font — repeated calls
 * with the same font add none — and keeps it for as long as it stays cached,
 * so the caller may release its own reference once the call returns.
 *
 * @param ct_font  the CoreText font to wrap; NULL returns NULL
 * @return the new typeface, or NULL when @p ct_font is NULL or this skity
 *         build has no CoreText font backend (SKITY_CT_FONT=OFF)
 */
SKITY_C_API skity_typeface skity_typeface_make_from_ct_font(CTFontRef ct_font);

/**
 * @brief Wrap a CoreText font as an owning skity_typeface handle, without
 *        the typeface cache.
 *
 * Mirrors skity::TypefaceCT::TypefaceFromCTFontWithoutCache: every call
 * creates an independent typeface (its own skity_typeface_get_unique_id)
 * that is released once nothing references it any more — neither the handle
 * nor any font / paint / text blob created from it. Nothing is retained in
 * the process-wide cache, which makes this the entry for page-scoped fonts
 * owned by a shorter-lived component.
 *
 * The typeface takes exactly one reference on @p ct_font and drops it
 * together with itself, so the caller may release its own reference once the
 * call returns.
 *
 * @param ct_font  the CoreText font to wrap; NULL returns NULL
 * @return the new typeface, or NULL when @p ct_font is NULL or this skity
 *         build has no CoreText font backend (SKITY_CT_FONT=OFF)
 */
SKITY_C_API skity_typeface
skity_typeface_make_from_ct_font_without_cache(CTFontRef ct_font);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_TEXT_CT_H
