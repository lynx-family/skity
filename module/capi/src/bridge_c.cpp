// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_canvas.h>

#include <memory>
#include <utility>

#include "handle.hpp"

/*
 * Bridge entry points (declared in include/skity_hpp/skity_bridge.hpp). They
 * wrap existing native C++ objects as non-owning C handles, mirroring the
 * borrow semantics of skity_surface_lock_canvas (no-op deleter). Declared in
 * the bridge header rather than the public skity_c/ headers because they are
 * meaningful only to C++ callers.
 */

extern "C" {

// SKITY_C_API (visibility default) is applied at the definition because this
// function has no public skity_c/ header declaration — without it the
// -fvisibility=hidden build would hide the symbol.
SKITY_C_API skity_canvas skity_canvas_from_native(void* native) {
  if (native == nullptr) {
    return nullptr;
  }
  // Non-owning: the no-op deleter means skity_canvas_destroy only reclaims the
  // wrapper struct and never touches the caller's Canvas.
  std::shared_ptr<void> impl(native, [](void*) {});
  return skity::capi::alloc_handle<skity_canvas_s>(SKITY_OBJECT_TYPE_CANVAS, 0u,
                                                   std::move(impl));
}

}  // extern "C"
