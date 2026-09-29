// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_canvas.h>
#include <skity_c/skity_text.h>

#include <memory>
#include <skity/text/typeface.hpp>
#include <utility>

#include "handle.hpp"

/*
 * Bridge entry points (declared in include/skity_hpp/skity_bridge.hpp). They
 * wrap existing native C++ objects as C handles and are declared in the
 * bridge header rather than the public skity_c/ headers because they are
 * meaningful only to C++ callers. Ownership follows the native object's
 * model: the canvas bridge borrows (mirroring skity_surface_lock_canvas's
 * no-op deleter), while the typeface bridge takes a strong reference via
 * enable_shared_from_this like every other skity_typeface constructor.
 */

extern "C" {

// SKITY_C_API (visibility default) is applied at the definitions because these
// functions have no public skity_c/ header declarations — without it the
// -fvisibility=hidden build would hide the symbols.
SKITY_C_API skity_canvas skity_canvas_from_native(void* native) {
  if (native == nullptr) {
    return nullptr;
  }
  // Non-owning: the no-op deleter means skity_canvas_destroy only reclaims the
  // wrapper struct and never touches the caller's Canvas.
  std::shared_ptr<skity::Canvas> impl(static_cast<skity::Canvas*>(native),
                                      [](skity::Canvas*) {});
  return skity::capi::alloc_handle<skity_canvas_s>(SKITY_OBJECT_TYPE_CANVAS, 0u,
                                                   std::move(impl));
}

SKITY_C_API skity_typeface skity_typeface_from_native(void* native) {
  if (native == nullptr) {
    return nullptr;
  }
  // Owning, unlike the canvas bridge above: Typeface is a shared_ptr-managed
  // resource (every skity API surfaces it as shared_ptr), so take a strong
  // reference via enable_shared_from_this. The handle then behaves like every
  // other skity_typeface constructor — skity_typeface_destroy releases the
  // reference, and downstream consumers (paint / font / text blob) holding
  // the shared_ptr keep the typeface alive instead of dangling after the
  // caller drops their own reference.
  auto* typeface = static_cast<skity::Typeface*>(native);
  return skity::capi::alloc_handle<skity_typeface_s>(
      SKITY_OBJECT_TYPE_TYPEFACE, SKITY_HANDLE_OWNING,
      typeface->shared_from_this());
}

}  // extern "C"
