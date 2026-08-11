// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_bitmap.h>

#include <skity/graphic/bitmap.hpp>
#include <skity/io/pixmap.hpp>

#include "handle.hpp"

namespace {

skity::Bitmap* bitmap_of(skity_bitmap handle) {
  auto* w =
      skity::capi::resolve<skity_bitmap_s>(handle, SKITY_OBJECT_TYPE_BITMAP);
  return w ? static_cast<skity::Bitmap*>(w->impl.get()) : nullptr;
}

skity::Pixmap* pixmap_of(skity_pixmap handle) {
  auto* w =
      skity::capi::resolve<skity_pixmap_s>(handle, SKITY_OBJECT_TYPE_PIXMAP);
  return w ? static_cast<skity::Pixmap*>(w->impl.get()) : nullptr;
}

}  // namespace

extern "C" {

skity_bitmap skity_bitmap_create(uint32_t width, uint32_t height,
                                 skity_alpha_type alpha_type,
                                 skity_color_type color_type) {
  if (width == 0 || height == 0) {
    return nullptr;
  }
  auto bp = std::make_shared<skity::Bitmap>(
      width, height, static_cast<skity::AlphaType>(alpha_type),
      static_cast<skity::ColorType>(color_type));
  return skity::capi::alloc_handle<skity_bitmap_s>(
      SKITY_OBJECT_TYPE_BITMAP, SKITY_HANDLE_OWNING, std::move(bp));
}

void skity_bitmap_destroy(skity_bitmap bitmap) {
  skity::capi::destroy_handle<skity_bitmap_s>(bitmap, SKITY_OBJECT_TYPE_BITMAP);
}

uint32_t skity_bitmap_get_width(skity_bitmap bitmap) {
  auto* b = bitmap_of(bitmap);
  return b ? b->Width() : 0;
}

uint32_t skity_bitmap_get_height(skity_bitmap bitmap) {
  auto* b = bitmap_of(bitmap);
  return b ? b->Height() : 0;
}

size_t skity_bitmap_get_row_bytes(skity_bitmap bitmap) {
  auto* b = bitmap_of(bitmap);
  return b ? b->RowBytes() : 0;
}

void* skity_bitmap_get_pixels(skity_bitmap bitmap) {
  auto* b = bitmap_of(bitmap);
  return b ? b->GetPixelAddr() : nullptr;
}

void skity_pixmap_destroy(skity_pixmap pixmap) {
  skity::capi::destroy_handle<skity_pixmap_s>(pixmap, SKITY_OBJECT_TYPE_PIXMAP);
}

uint32_t skity_pixmap_get_width(skity_pixmap pixmap) {
  auto* p = pixmap_of(pixmap);
  return p ? p->Width() : 0;
}

uint32_t skity_pixmap_get_height(skity_pixmap pixmap) {
  auto* p = pixmap_of(pixmap);
  return p ? p->Height() : 0;
}

size_t skity_pixmap_get_row_bytes(skity_pixmap pixmap) {
  auto* p = pixmap_of(pixmap);
  return p ? p->RowBytes() : 0;
}

const void* skity_pixmap_get_pixels(skity_pixmap pixmap) {
  auto* p = pixmap_of(pixmap);
  return p ? p->Addr() : nullptr;
}

skity_pixmap skity_bitmap_get_pixmap(skity_bitmap bitmap) {
  auto* b = bitmap_of(bitmap);
  if (b == nullptr) return nullptr;
  auto pm = b->GetPixmap();
  if (pm == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_pixmap_s>(SKITY_OBJECT_TYPE_PIXMAP,
                                                   SKITY_HANDLE_OWNING, pm);
}

}  // extern "C"
