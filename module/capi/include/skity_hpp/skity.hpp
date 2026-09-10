// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_HPP

/*
 * Header-only RAII layer over the skity C API (the "skity.hpp" of
 * C_API_DESIGN.md section 8, staged here until the Stage 3 include move).
 *
 * Everything here is compiled into the consumer's translation unit: no
 * symbol enters libskity-capi.so and no C++ type crosses the ABI boundary.
 * The classes simply own / borrow C handles and forward to the C entry
 * points inline.
 *
 * FILE LAYOUT: one wrapper header per C domain header, same file name with
 * a .hpp extension (skity_c/skity_paint.h -> skity_hpp/skity_paint.hpp).
 * This file aggregates them; consumers include only <skity_hpp/skity.hpp>
 * or pull in individual domain headers as needed.
 *
 * NAMESPACE: everything lives in skity::raii (the vk::raii precedent), NOT
 * skity:: — this is load-bearing while libskity.dylib still exports the
 * legacy C++ symbols (the SKITY_DLL visibility leak). The wrapper classes
 * intentionally mirror the legacy names and signatures (Canvas::DrawRect,
 * Rect, ...), so at -O0 (or whenever a member call is not inlined) a
 * skity::Canvas::DrawRect call site mangles to the SAME symbol as the
 * legacy class method exported by libskity.dylib, and the dynamic linker
 * binds the call to the legacy implementation. That implementation then
 * runs with the wrapper object as `this` and silently corrupts it
 * (verified: handle_ gets overwritten with call arguments). The nested
 * namespace removes the collision. When Stage 3 hides the C++ symbols,
 * this namespace collapses back to plain skity:: in the same major bump
 * that moves the headers to include/skity/ (see lat.md/capi#Terminal
 * include layout). Do NOT include this header in a TU that also includes
 * the legacy headers under include/skity/.
 *
 * This is the opposite direction of skity_bridge.hpp, which lends existing
 * C++ objects INTO C handles for consumers mid-migration.
 *
 * Ownership follows the C layer exactly:
 *  - classes deriving from detail::OwnHandle own their handle; the
 *    destructor calls the matching skity_*_destroy;
 *  - Canvas is a non-owning view (matching the handle borrowed from
 *    skity_surface_lock_canvas); SoftwareCanvas additionally owns one.
 *
 * Following the coverage discipline of the C API, this layer only wraps
 * existing entry points — it never needs new C functions. Where the legacy
 * C++ surface had helper methods with no C counterpart (Rect / Matrix
 * arithmetic), they are implemented inline instead.
 *
 * No exceptions: creation failures leave the wrapper empty, and the C layer
 * safely ignores NULL handles, so forwarding from an empty wrapper is a
 * no-op rather than a crash.
 *
 * Engineering conventions borrowed from vulkan.hpp: a detail namespace for
 * non-API entities (skity::raii::detail::OwnHandle) and ZERO out-of-line
 * symbols — this layer is pure forwarding and must stay that way.
 */

// The aggregate also pulls in the full C API: not every domain is wrapped
// yet, and the intended mixed usage (wrapper + raw C for unwrapped entry
// points) means #include <skity_hpp/skity.hpp> alone must be sufficient.
// Individual domain headers stay minimal — include them directly when you
// want tighter compile dependencies.
#include <skity_c/skity.h>

#include <skity_hpp/skity_base.hpp>
#include <skity_hpp/skity_canvas.hpp>
#include <skity_hpp/skity_context.hpp>
#include <skity_hpp/skity_paint.hpp>
#include <skity_hpp/skity_path.hpp>
#include <skity_hpp/skity_path_effect.hpp>
#include <skity_hpp/skity_shader.hpp>
#include <skity_hpp/skity_surface.hpp>
#include <skity_hpp/skity_text.hpp>
#include <skity_hpp/skity_types.hpp>

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_HPP
