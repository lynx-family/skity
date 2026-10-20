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
 * a .hpp extension (skity_c/skity_paint.h ->
 * skity_hpp/graphic/paint.hpp). This file aggregates them; consumers
 * include only <skity_hpp/skity.hpp> or pull in individual domain headers as
 * needed.
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
 * that moves the headers to include/skity/. Do NOT include this header in
 * a TU that also includes
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

// effect
#include <skity_hpp/effect/color_filter.hpp>
// geometry
#include <skity_hpp/geometry/camera.hpp>
// gpu
#include <skity_hpp/gpu/context.hpp>
// graphic
#include <skity_hpp/graphic/bitmap.hpp>
// render
#include <skity_hpp/render/canvas.hpp>
// base / types / bridge
#include <skity_hpp/base.hpp>
#ifdef __OBJC__
#include <skity_hpp/gpu/context_mtl.hpp>
#endif
#include <skity_hpp/effect/image_filter.hpp>
#include <skity_hpp/effect/mask_filter.hpp>
#include <skity_hpp/effect/path_effect.hpp>
#include <skity_hpp/effect/shader.hpp>
#include <skity_hpp/geometry/quaternion.hpp>
#include <skity_hpp/geometry/stroke.hpp>
#include <skity_hpp/gpu/semaphore.hpp>
#include <skity_hpp/gpu/surface.hpp>
#include <skity_hpp/graphic/image.hpp>
#include <skity_hpp/graphic/paint.hpp>
#include <skity_hpp/graphic/path.hpp>
#include <skity_hpp/graphic/path_measure.hpp>
#include <skity_hpp/graphic/path_op.hpp>
// io
#include <skity_hpp/io/data.hpp>
// recorder
#include <skity_hpp/recorder/recorder.hpp>
// text
#include <skity_hpp/text/glyph.hpp>
#ifdef __OBJC__
#include <skity_hpp/gpu/surface_mtl.hpp>
#endif
#include <skity_hpp/gpu/texture.hpp>
#include <skity_hpp/text/text.hpp>
#ifdef __OBJC__
#include <skity_hpp/gpu/texture_mtl.hpp>
#endif
#include <skity_hpp/types.hpp>

// Vulkan-only wrapper headers (context / surface / texture / native-window
// / semaphore sync-fd import, aggregated by skity_vk.hpp) are deliberately
// NOT included here: they pull in <vulkan/vulkan.h>. VK consumers include
// <skity_hpp/skity_vk.hpp> instead. The Metal wrappers are also guarded:
// the Metal C API is Objective-C-only (real id<MTLDevice> handles, no
// void* fallback — see skity_mtl_types.h), so their headers only exist
// under __OBJC__ and plain C++ consumers are unaffected.

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_HPP
