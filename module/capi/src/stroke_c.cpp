// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_stroke.h>

#include <skity/geometry/stroke.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>

#include "handle.hpp"

namespace {

skity::Paint* paint_of(skity_paint handle) {
  auto* w =
      skity::capi::resolve<skity_paint_s>(handle, SKITY_OBJECT_TYPE_PAINT);
  return w ? static_cast<skity::Paint*>(w->impl.get()) : nullptr;
}

skity::Path* path_of(skity_path handle) {
  auto* w = skity::capi::resolve<skity_path_s>(handle, SKITY_OBJECT_TYPE_PATH);
  return w ? static_cast<skity::Path*>(w->impl.get()) : nullptr;
}

}  // namespace

extern "C" {

void skity_stroke_stroke_path(skity_paint paint, skity_path src,
                              skity_path dst) {
  auto* p = paint_of(paint);
  auto* s = path_of(src);
  auto* d = path_of(dst);
  if (p == nullptr || s == nullptr || d == nullptr) return;
  skity::Stroke stroke(*p);
  stroke.StrokePath(*s, d);
}

void skity_stroke_quad_path(skity_paint paint, skity_path src, skity_path dst,
                            uint32_t keep_cubic) {
  auto* p = paint_of(paint);
  auto* s = path_of(src);
  auto* d = path_of(dst);
  if (p == nullptr || s == nullptr || d == nullptr) return;
  skity::Stroke stroke(*p);
  stroke.QuadPath(*s, d, keep_cubic != 0);
}

}  // extern "C"
