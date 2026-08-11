// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_precompile.h>

#include <skity/graphic/paint.hpp>
#include <skity/render/precompile_context.hpp>

#include "handle.hpp"

namespace {

skity::PrecompileContext* precompile_of(skity_precompile_context handle) {
  auto* w = skity::capi::resolve<skity_precompile_context_s>(
      handle, SKITY_OBJECT_TYPE_PRECOMPILE_CONTEXT);
  return w ? static_cast<skity::PrecompileContext*>(w->impl.get()) : nullptr;
}

skity::Paint* paint_of(skity_paint handle) {
  auto* w =
      skity::capi::resolve<skity_paint_s>(handle, SKITY_OBJECT_TYPE_PAINT);
  return w ? static_cast<skity::Paint*>(w->impl.get()) : nullptr;
}

}  // namespace

extern "C" {

void skity_precompile_context_precompile_default_shaders(
    skity_precompile_context context) {
  auto* pc = precompile_of(context);
  if (pc != nullptr) {
    pc->PrecompileDefaultShaders();
  }
}

void skity_precompile_context_precompile_draw(
    skity_precompile_context context, skity_precompile_draw_type draw_type,
    skity_paint paint) {
  auto* pc = precompile_of(context);
  auto* p = paint_of(paint);
  if (pc != nullptr && p != nullptr) {
    pc->PrecompileDraw(static_cast<skity::PrecompileDrawType>(draw_type), *p);
  }
}

void skity_precompile_context_destroy(skity_precompile_context context) {
  skity::capi::destroy_handle<skity_precompile_context_s>(
      context, SKITY_OBJECT_TYPE_PRECOMPILE_CONTEXT);
}

}  // extern "C"
