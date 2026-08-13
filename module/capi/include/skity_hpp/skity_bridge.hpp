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
 * The handles produced are non-owning (borrowed): they wrap the object with a
 * no-op deleter, exactly like the skity_canvas returned by
 * skity_surface_lock_canvas. The caller must keep the underlying object alive
 * for the handle's lifetime; skity_canvas_destroy only reclaims the small
 * wrapper struct and never deletes the object.
 *
 * These entry points are declared here (and not in the public skity_c/
 * headers) because they are meaningful only to C++ callers: they take a
 * type-erased pointer to a native object.
 */

#include <skity_c/skity_canvas.h>  // skity_canvas, SKITY_C_API

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

}  // extern "C"

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_BRIDGE_HPP
