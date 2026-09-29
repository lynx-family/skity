// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_BRIDGE_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_BRIDGE_HPP

/*
 * C++ <-> C ABI bridge for skity-capi.
 *
 * This header declares entry points for C++ consumers that already hold
 * native skity C++ objects (e.g. a skity::Canvas* obtained through the C++ GPU
 * surface API) and want to drive them through the C API. It is the reverse
 * direction of the eventual Stage 2 RAII wrapper (skity.hpp): here, C++
 * objects are bridged INTO C handles.
 *
 * The handles' ownership follows the native object's model. Canvas is a
 * service owned by its surface, so skity_canvas_from_native borrows: it wraps
 * the object with a no-op deleter, exactly like the skity_canvas returned by
 * skity_surface_lock_canvas, and the caller must keep the underlying object
 * alive for the handle's lifetime. Typeface is a shared_ptr-managed resource,
 * so skity_typeface_from_native takes a strong reference (via
 * enable_shared_from_this) and behaves like every other skity_typeface
 * constructor. In both cases the capi-allocated wrapper struct itself must
 * still be released with the corresponding destroy function; that call only
 * reclaims the wrapper (plus, for owning handles, releases the reference) and
 * never deletes a borrowed object.
 *
 * These entry points are declared here (and not in the public skity_c/
 * headers) because they are meaningful only to C++ callers: they take a
 * type-erased pointer to a native object.
 */

#include <skity_c/skity_canvas.h>  // skity_canvas, SKITY_C_API
#include <skity_c/skity_text.h>    // skity_typeface

extern "C" {

/**
 * @brief Wrap an existing native (C++) Canvas pointer as a non-owning
 *        skity_canvas handle.
 *
 * @warning This is a TEMPORARY bridge API. It exists only to ease the
 *          current C++/capi mixed-usage transition and may be reworked or
 *          removed in a future release. Do not build long-term abstractions on
 *          top of it.
 *
 * @p native MUST be the raw pointer value of a @c skity::Canvas* (for example
 * one obtained from @c GPUSurface::LockCanvas). Because the parameter is a
 * type-erased @c void*, passing a pointer to any other type is undefined
 * behaviour — the capi layer cannot validate the type and will happily
 * reinterpret it.
 *
 * The capi layer does NOT take ownership of @p native; it only borrows it. The
 * caller is solely responsible for keeping the Canvas alive for the entire
 * lifetime of the returned handle (and any handle derived from it).
 *
 * Non-owning does NOT mean the handle is free to drop: the returned
 * @c skity_canvas is still a capi-allocated wrapper and MUST be released with
 * @c skity_canvas_destroy once it is no longer needed, otherwise the wrapper
 * struct leaks (notably so when bridging every frame). That call only reclaims
 * the wrapper — it never deletes the borrowed Canvas (same non-owning
 * semantics as the canvas returned by @c skity_surface_lock_canvas), so it is
 * always safe to call.
 *
 * @param native  raw pointer value of a skity::Canvas*, passed as @c void*;
 *                NULL returns NULL
 * @return a non-owning skity_canvas handle borrowing @p native, or NULL if
 *         @p native is NULL
 */
SKITY_C_API skity_canvas skity_canvas_from_native(void* native);

/**
 * @brief Wrap an existing native (C++) Typeface pointer as an owning
 *        skity_typeface handle.
 *
 * Same TEMPORARY-bridge warning as skity_canvas_from_native, but the
 * ownership differs because the underlying models do: unlike the borrowed
 * canvas, this handle takes a strong reference to @p native through
 * Typeface's enable_shared_from_this, giving it exactly the semantics of
 * every other skity_typeface constructor (skity_typeface_make_from_file
 * etc.):
 *   - the typeface stays alive even after the caller drops their own
 *     shared_ptr, so downstream handles (a Font, a Paint, a text blob) can
 *     safely keep the typeface; and
 *   - skity_typeface_destroy releases that reference in addition to
 *     reclaiming the wrapper struct.
 *
 * @p native MUST be the raw pointer value of a @c skity::Typeface* that is
 * currently owned by at least one shared_ptr. Every public skity API
 * surfaces Typeface as shared_ptr, so this holds for any legitimately
 * obtained pointer; bridging a pointer outside shared ownership (never
 * shared, or bridged after the last shared_ptr was released) is undefined
 * behaviour.
 *
 * @param native  raw pointer value of a skity::Typeface*, passed as @c void*;
 *                NULL returns NULL
 * @return an owning skity_typeface handle holding a strong reference to
 *         @p native, or NULL if @p native is NULL
 */
SKITY_C_API skity_typeface skity_typeface_from_native(void* native);

}  // extern "C"

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_BRIDGE_HPP
