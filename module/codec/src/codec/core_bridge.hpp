// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CODEC_SRC_CODEC_CORE_BRIDGE_HPP
#define MODULE_CODEC_SRC_CODEC_CORE_BRIDGE_HPP

// Object bridge between the codec module's C++ interface (which hands around
// std::shared_ptr<Data> / std::shared_ptr<Pixmap>) and the core C ABI.
// Every entry that used to bind a core C++ symbol — the Data factories, the
// Pixmap constructors, Pixmap::SetColorInfo — is expressed here as a C ABI
// call unwrapped back into the shared_ptr surface, so the module keeps its
// internal interface while importing no core C++ symbols. This is what lets
// libskity-codec link against a libskity that exports only the C ABI.
//
// The pixel-addressing and color-conversion helpers are header-inline
// reimplementations of the (non-inline) core methods; their arithmetic is
// kept bit-identical so decoded pixels do not shift by one ULP anywhere.

#include <skity/c/skity_bitmap.h>
#include <skity/c/skity_data.h>

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <skity/graphic/alpha_type.hpp>
#include <skity/graphic/color_type.hpp>
#include <skity/io/data.hpp>
#include <skity/io/pixmap.hpp>
#include <utility>

// capi's handle protocol, reached through module/capi/src on the include path
// like every *_c.cpp consumer of it
#include "handle.hpp"  // NOLINT(build/include_subdir)

namespace skity {
namespace core_bridge {

inline skity_alpha_type to_c_alpha(AlphaType type) {
  return static_cast<skity_alpha_type>(type);
}

inline skity_color_type to_c_color(ColorType type) {
  return static_cast<skity_color_type>(type);
}

//
// Data factories
//

inline void MallocReleaseProc(const void* ptr, void*) {
  std::free(const_cast<void*>(ptr));
}

inline std::shared_ptr<Data> MakeDataWithCopy(const void* data, size_t length) {
  skity_data h = skity_data_make_with_copy(data, length);
  if (h == nullptr) {
    return nullptr;
  }
  auto sp =
      skity::capi::get_impl<skity_data_s, Data>(h, SKITY_OBJECT_TYPE_DATA);
  skity_data_destroy(h);
  return sp;
}

// Ownership replica of Data::MakeFromMalloc: the core takes the malloc'd
// buffer and releases it with free when the last reference goes away.
inline std::shared_ptr<Data> MakeDataFromMalloc(void* data, size_t length) {
  skity_data h =
      skity_data_make_with_proc(data, length, &MallocReleaseProc, nullptr);
  if (h == nullptr) {
    // The release proc was never handed off; the allocation is still ours.
    std::free(data);
    return nullptr;
  }
  auto sp =
      skity::capi::get_impl<skity_data_s, Data>(h, SKITY_OBJECT_TYPE_DATA);
  skity_data_destroy(h);
  return sp;
}

//
// Pixmap factories
//

inline std::shared_ptr<Pixmap> MakePixmap(std::shared_ptr<Data> data,
                                          size_t row_bytes, uint32_t width,
                                          uint32_t height, AlphaType alpha_type,
                                          ColorType color_type) {
  if (data == nullptr) {
    return nullptr;
  }
  skity_data data_handle = skity::capi::alloc_handle<skity_data_s>(
      SKITY_OBJECT_TYPE_DATA, SKITY_HANDLE_OWNING, std::move(data));
  if (data_handle == nullptr) {
    return nullptr;
  }
  skity_pixmap pm_handle =
      skity_pixmap_create(data_handle, row_bytes, width, height,
                          to_c_alpha(alpha_type), to_c_color(color_type));
  // The pixmap holds its own reference to the data buffer.
  skity_data_destroy(data_handle);
  if (pm_handle == nullptr) {
    return nullptr;
  }
  auto sp = skity::capi::get_impl<skity_pixmap_s, Pixmap>(
      pm_handle, SKITY_OBJECT_TYPE_PIXMAP);
  skity_pixmap_destroy(pm_handle);
  return sp;
}

inline std::shared_ptr<Pixmap> MakePixmapAlloc(uint32_t width, uint32_t height,
                                               AlphaType alpha_type,
                                               ColorType color_type) {
  skity_pixmap pm_handle = skity_pixmap_create_alloc(
      width, height, to_c_alpha(alpha_type), to_c_color(color_type));
  if (pm_handle == nullptr) {
    return nullptr;
  }
  auto sp = skity::capi::get_impl<skity_pixmap_s, Pixmap>(
      pm_handle, SKITY_OBJECT_TYPE_PIXMAP);
  skity_pixmap_destroy(pm_handle);
  return sp;
}

// Mirrors Pixmap::SetColorInfo, conversion side effects included: the call
// runs inside the core, so a premul <-> unpremul flip still rewrites the
// wrapped pixels exactly as before.
inline bool SetPixmapColorInfo(Pixmap* pixmap, AlphaType alpha_type,
                               ColorType color_type) {
  if (pixmap == nullptr) {
    return false;
  }
  std::shared_ptr<Pixmap> view(pixmap, [](Pixmap*) {});
  skity_pixmap pm_handle = skity::capi::alloc_handle<skity_pixmap_s>(
      SKITY_OBJECT_TYPE_PIXMAP, 0, std::move(view));
  if (pm_handle == nullptr) {
    return false;
  }
  skity_result result = skity_pixmap_set_color_info(
      pm_handle, to_c_alpha(alpha_type), to_c_color(color_type));
  skity_pixmap_destroy(pm_handle);
  return result == SKITY_SUCCESS;
}

//
// Pixel addressing — inline equivalents of the out-of-line core methods.
// Built exclusively from the header-inline Pixmap accessors.
//

inline size_t BytesPerPixel(ColorType color_type) {
  switch (color_type) {
    case ColorType::kRGBA:
    case ColorType::kBGRA:
      return 4;
    case ColorType::kRGB565:
      return 2;
    case ColorType::kA8:
      return 1;
    case ColorType::kUnknown:
      return 0;
  }
  return 0;
}

inline const uint8_t* PixmapAddr8(const Pixmap* pixmap, uint32_t x,
                                  uint32_t y) {
  if (pixmap == nullptr || x >= pixmap->Width() || y >= pixmap->Height()) {
    return nullptr;
  }
  return static_cast<const uint8_t*>(pixmap->Addr()) +
         static_cast<size_t>(y) * pixmap->RowBytes() +
         static_cast<size_t>(x) * BytesPerPixel(pixmap->GetColorType());
}

inline uint8_t* PixmapWritableAddr8(Pixmap* pixmap, uint32_t x, uint32_t y) {
  return const_cast<uint8_t*>(PixmapAddr8(pixmap, x, y));
}

//
// Color conversions — bit-identical reimplementations of the core
// ColorToPMColor / PMColorToColor, including their rounding.
//

// a*b/255 with the core's two-step rounding (color_priv.hpp
// Mul16ShiftRound(a, b, 8)).
inline uint32_t MulDiv255Round(uint32_t a, uint32_t b) {
  uint32_t prod = a * b + 128u;
  return (prod + (prod >> 8)) >> 8;
}

inline uint32_t ColorToPMColor(uint32_t color) {
  uint32_t a = (color >> 24) & 0xFFu;
  uint32_t r = (color >> 16) & 0xFFu;
  uint32_t g = (color >> 8) & 0xFFu;
  uint32_t b = color & 0xFFu;
  if (a != 255u) {
    r = MulDiv255Round(r, a);
    g = MulDiv255Round(g, a);
    b = MulDiv255Round(b, a);
  }
  return (a << 24) | (r << 16) | (g << 8) | b;
}

inline uint32_t PMColorToColor(uint32_t color) {
  uint32_t a = (color >> 24) & 0xFFu;
  uint32_t r = (color >> 16) & 0xFFu;
  uint32_t g = (color >> 8) & 0xFFu;
  uint32_t b = color & 0xFFu;
  if (a == 0u) {
    return 0u;
  }
  // The core un-premultiply scale is (255 << 24) / a with the product
  // rounded at bit 23 (color_priv.cc UnPreMultiply); recomputing it per
  // channel yields the same bytes as the core lookup table.
  uint32_t scale = (255u << 24) / a;
  auto apply = [scale](uint32_t c) { return (scale * c + (1u << 23)) >> 24; };
  return (a << 24) | (apply(r) << 16) | (apply(g) << 8) | apply(b);
}

}  // namespace core_bridge
}  // namespace skity

#endif  // MODULE_CODEC_SRC_CODEC_CORE_BRIDGE_HPP
