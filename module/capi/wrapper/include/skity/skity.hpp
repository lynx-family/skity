// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_WRAPPER_INCLUDE_SKITY_SKITY_HPP
#define MODULE_CAPI_WRAPPER_INCLUDE_SKITY_SKITY_HPP

/*
 * Header-only RAII layer over the skity C API — the "skity.hpp" of
 * C_API_DESIGN.md section 8, and the C++ surface of the SKITY_UNIFIED_LIB
 * shape.
 *
 * Everything here is compiled into the consumer's translation unit: no
 * symbol enters libskity and no C++ type crosses the ABI boundary. The
 * classes simply own / borrow C handles and forward to the C entry points
 * inline.
 *
 * FILE LAYOUT: one wrapper header per C domain header, same file name with
 * a .hpp extension (skity/c/skity_paint.h ->
 * skity/graphic/paint.hpp). This file aggregates them; consumers
 * include only <skity/skity.hpp> or pull in individual domain headers as
 * needed.
 *
 * NAMESPACE: plain skity::. That is only sound because this layer ships
 * exclusively with the unified library, which exports nothing but the C
 * ABI. The wrapper classes intentionally mirror the legacy names and
 * signatures (Canvas::DrawRect, Rect, ...), so against a normal libskity —
 * which still exports the legacy C++ API — an uninlined
 * skity::Canvas::DrawRect call site mangles to the SAME symbol as the
 * legacy class method, the dynamic linker binds the call to the legacy
 * implementation, and that implementation then runs with the wrapper
 * object as `this`, silently corrupting it (verified: handle_ gets
 * overwritten with call arguments). base.hpp therefore refuses to compile
 * unless SKITY_UNIFIED_LIB is defined — the unified build exports that
 * define through the skity target's usage requirements, and non-CMake
 * consumers define it themselves. Do NOT include this header in a TU that
 * also includes the legacy headers under include/skity/: that is a
 * redefinition, and the legacy surface does not link in this shape anyway.
 *
 * bridge.hpp is the opposite direction: it lends existing C++ objects INTO
 * C handles, and is only meaningful in the normal (legacy-C++) build.
 *
 * Ownership follows the C layer exactly:
 *  - classes deriving from detail::OwnHandle own their handle; the
 *    destructor calls the matching skity_*_destroy;
 *  - Canvas is a non-owning view (matching the handle borrowed from
 *    skity_surface_lock_canvas); Canvas::MakeSoftwareCanvas owns one.
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
 * non-API entities (skity::detail::OwnHandle) and ZERO out-of-line
 * symbols — this layer is pure forwarding and must stay that way.
 */

// The aggregate also pulls in the full C API: not every domain is wrapped
// yet, and the intended mixed usage (wrapper + raw C for unwrapped entry
// points) means #include <skity/skity.hpp> alone must be sufficient.
// Individual domain headers stay minimal — include them directly when you
// want tighter compile dependencies.
#include <skity/c/skity.h>

// effect
#include <skity/effect/color_filter.hpp>
// geometry
#include <skity/geometry/camera.hpp>
// gpu
#include <skity/gpu/context.hpp>
// graphic
#include <skity/graphic/bitmap.hpp>
// render
#include <skity/render/canvas.hpp>
// base / types / bridge
#include <skity/base.hpp>
#ifdef __OBJC__
#include <skity/gpu/context_mtl.hpp>
#endif
#include <skity/effect/image_filter.hpp>
#include <skity/effect/mask_filter.hpp>
#include <skity/effect/path_effect.hpp>
#include <skity/effect/shader.hpp>
#include <skity/geometry/quaternion.hpp>
#include <skity/geometry/stroke.hpp>
#include <skity/gpu/semaphore.hpp>
#include <skity/gpu/surface.hpp>
#include <skity/graphic/image.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>
#include <skity/graphic/path_measure.hpp>
#include <skity/graphic/path_op.hpp>
// io
#include <skity/io/data.hpp>
// recorder
#include <skity/recorder/recorder.hpp>
// text
#include <skity/text/glyph.hpp>
#ifdef __OBJC__
#include <skity/gpu/surface_mtl.hpp>
#endif
#include <skity/gpu/texture.hpp>
#include <skity/text/text.hpp>
#ifdef __OBJC__
#include <skity/gpu/texture_mtl.hpp>
#endif
#include <skity/types.hpp>

// Vulkan-only wrapper headers (context / surface / texture / native-window
// / semaphore sync-fd import, aggregated by skity_vk.hpp) are deliberately
// NOT included here: they pull in <vulkan/vulkan.h>. VK consumers include
// <skity/skity_vk.hpp> instead. The Metal wrappers are also guarded:
// the Metal C API is Objective-C-only (real id<MTLDevice> handles, no
// void* fallback — see skity_mtl_types.h), so their headers only exist
// under __OBJC__ and plain C++ consumers are unaffected.

#endif  // MODULE_CAPI_WRAPPER_INCLUDE_SKITY_SKITY_HPP
