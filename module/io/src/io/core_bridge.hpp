// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_IO_SRC_IO_CORE_BRIDGE_HPP
#define MODULE_IO_SRC_IO_CORE_BRIDGE_HPP

// Object bridge between the io module's serialization layer (which hands
// around the legacy std::shared_ptr object graph) and the core C ABI.
// Every entry that used to bind a core legacy symbol is expressed here as a
// C ABI call unwrapped back into the shared_ptr surface, so the module keeps
// its internal types while importing no core legacy symbols.

#include <skity_c/skity_codec.h>
#include <skity_c/skity_color_filter.h>
#include <skity_c/skity_data.h>
#include <skity_c/skity_font.h>
#include <skity_c/skity_image.h>
#include <skity_c/skity_image_filter.h>
#include <skity_c/skity_mask_filter.h>
#include <skity_c/skity_paint.h>
#include <skity_c/skity_path.h>
#include <skity_c/skity_path_effect.h>
#include <skity_c/skity_shader.h>
#include <skity_c/skity_text.h>

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <skity/effect/color_filter.hpp>
#include <skity/effect/image_filter.hpp>
#include <skity/effect/mask_filter.hpp>
#include <skity/effect/path_effect.hpp>
#include <skity/effect/shader.hpp>
#include <skity/graphic/image.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/matrix_query.hpp>
#include <skity/geometry/matrix.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>
#include <skity/io/data.hpp>
#include <skity/text/font.hpp>
#include <skity/text/font_style.hpp>
#include <skity/text/text_blob.hpp>
#include <skity/text/typeface.hpp>
#include <utility>
#include <vector>

#include "src/handle.hpp"

namespace skity {
namespace core_bridge {

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
    std::free(data);
    return nullptr;
  }
  auto sp =
      skity::capi::get_impl<skity_data_s, Data>(h, SKITY_OBJECT_TYPE_DATA);
  skity_data_destroy(h);
  return sp;
}

// Decode an encoded image blob through the codec module's C ABI and wrap the
// result as a raster image. Returns an empty shared_ptr when the bytes are
// not decodable.
inline std::shared_ptr<Image> DecodeDataToImage(const void* bytes,
                                                size_t size) {
  skity_pixmap pm = skity_codec_decode(bytes, size, 0, 0);
  if (pm == nullptr) {
    return nullptr;
  }
  skity_image img = skity_image_create_from_pixmap(pm);
  skity_pixmap_destroy(pm);
  if (img == nullptr) {
    return nullptr;
  }
  auto sp =
      skity::capi::get_impl<skity_image_s, Image>(img, SKITY_OBJECT_TYPE_IMAGE);
  skity_image_destroy(img);
  return sp;
}

// Instantiate a typeface from raw font-file bytes through the default font
// manager's C ABI surface.
inline std::shared_ptr<Typeface> MakeTypefaceFromBytes(const void* bytes,
                                                       size_t size) {
  skity_data h = skity_data_make_with_copy(bytes, size);
  if (h == nullptr) {
    return nullptr;
  }
  skity_font_manager manager = skity_font_manager_ref_default();
  skity_typeface tf = manager != nullptr
                          ? skity_font_manager_make_from_data(manager, h, 0)
                          : nullptr;
  skity_data_destroy(h);
  skity_font_manager_destroy(manager);
  if (tf == nullptr) {
    return nullptr;
  }
  auto sp = skity::capi::get_impl<skity_typeface_s, Typeface>(
      tf, SKITY_OBJECT_TYPE_TYPEFACE);
  skity_typeface_destroy(tf);
  return sp;
}

//
// Effect factories. Each builds the effect through its C ABI factory and
// unwraps the result back into the legacy shared_ptr surface; shared_ptr
// parameters are re-wrapped as (owning) handles for the call.
//

template <typename Wrapper, typename T, typename Handle>
inline std::shared_ptr<T> adopt_handle(Handle handle, void (*destroy)(Handle),
                                       skity_object_type type) {
  if (handle == nullptr) {
    return nullptr;
  }
  auto sp = skity::capi::get_impl<Wrapper, T>(handle, type);
  destroy(handle);
  return sp;
}

template <typename Wrapper, typename Handle>
inline Handle wrap_handle(skity_object_type type,
                          std::shared_ptr<typename Wrapper::value_type> sp) {
  if (sp == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<Wrapper>(type, SKITY_HANDLE_OWNING,
                                            std::move(sp));
}

inline std::shared_ptr<ColorFilter> MakeColorFilterMatrix(const float* matrix) {
  return adopt_handle<skity_color_filter_s, ColorFilter, skity_color_filter>(
      skity_color_filter_create_matrix(matrix), &skity_color_filter_destroy,
      SKITY_OBJECT_TYPE_COLOR_FILTER);
}

inline std::shared_ptr<ColorFilter> MakeColorFilterBlend(uint32_t color,
                                                         BlendMode mode) {
  return adopt_handle<skity_color_filter_s, ColorFilter, skity_color_filter>(
      skity_color_filter_create_blend(color,
                                      static_cast<skity_blend_mode>(mode)),
      &skity_color_filter_destroy, SKITY_OBJECT_TYPE_COLOR_FILTER);
}

inline std::shared_ptr<ColorFilter> MakeColorFilterCompose(
    const std::shared_ptr<ColorFilter>& outer,
    const std::shared_ptr<ColorFilter>& inner) {
  auto oh = wrap_handle<skity_color_filter_s, skity_color_filter>(
      SKITY_OBJECT_TYPE_COLOR_FILTER, outer);
  auto ih = wrap_handle<skity_color_filter_s, skity_color_filter>(
      SKITY_OBJECT_TYPE_COLOR_FILTER, inner);
  auto out = skity_color_filter_create_compose(oh, ih);
  if (oh != nullptr) skity_color_filter_destroy(oh);
  if (ih != nullptr) skity_color_filter_destroy(ih);
  return adopt_handle<skity_color_filter_s, ColorFilter, skity_color_filter>(
      out, &skity_color_filter_destroy, SKITY_OBJECT_TYPE_COLOR_FILTER);
}

inline std::shared_ptr<ColorFilter> MakeColorFilterLinearToSRGB() {
  return adopt_handle<skity_color_filter_s, ColorFilter, skity_color_filter>(
      skity_color_filter_create_linear_to_srgb(), &skity_color_filter_destroy,
      SKITY_OBJECT_TYPE_COLOR_FILTER);
}

inline std::shared_ptr<ColorFilter> MakeColorFilterSRGBToLinear() {
  return adopt_handle<skity_color_filter_s, ColorFilter, skity_color_filter>(
      skity_color_filter_create_srgb_to_linear(), &skity_color_filter_destroy,
      SKITY_OBJECT_TYPE_COLOR_FILTER);
}

inline std::shared_ptr<ImageFilter> MakeImageFilterBlur(float sx, float sy) {
  return adopt_handle<skity_image_filter_s, ImageFilter, skity_image_filter>(
      skity_image_filter_create_blur(sx, sy), &skity_image_filter_destroy,
      SKITY_OBJECT_TYPE_IMAGE_FILTER);
}

inline std::shared_ptr<ImageFilter> MakeImageFilterDilate(float rx, float ry) {
  return adopt_handle<skity_image_filter_s, ImageFilter, skity_image_filter>(
      skity_image_filter_create_dilate(rx, ry), &skity_image_filter_destroy,
      SKITY_OBJECT_TYPE_IMAGE_FILTER);
}

inline std::shared_ptr<ImageFilter> MakeImageFilterErode(float rx, float ry) {
  return adopt_handle<skity_image_filter_s, ImageFilter, skity_image_filter>(
      skity_image_filter_create_erode(rx, ry), &skity_image_filter_destroy,
      SKITY_OBJECT_TYPE_IMAGE_FILTER);
}

inline std::shared_ptr<ImageFilter> MakeImageFilterColorFilter(
    const std::shared_ptr<ColorFilter>& filter) {
  auto fh = wrap_handle<skity_color_filter_s, skity_color_filter>(
      SKITY_OBJECT_TYPE_COLOR_FILTER, filter);
  auto out = skity_image_filter_create_from_color_filter(fh);
  if (fh != nullptr) skity_color_filter_destroy(fh);
  return adopt_handle<skity_image_filter_s, ImageFilter, skity_image_filter>(
      out, &skity_image_filter_destroy, SKITY_OBJECT_TYPE_IMAGE_FILTER);
}

inline std::shared_ptr<ImageFilter> MakeImageFilterCompose(
    const std::shared_ptr<ImageFilter>& outer,
    const std::shared_ptr<ImageFilter>& inner) {
  auto oh = wrap_handle<skity_image_filter_s, skity_image_filter>(
      SKITY_OBJECT_TYPE_IMAGE_FILTER, outer);
  auto ih = wrap_handle<skity_image_filter_s, skity_image_filter>(
      SKITY_OBJECT_TYPE_IMAGE_FILTER, inner);
  auto out = skity_image_filter_create_compose(oh, ih);
  if (oh != nullptr) skity_image_filter_destroy(oh);
  if (ih != nullptr) skity_image_filter_destroy(ih);
  return adopt_handle<skity_image_filter_s, ImageFilter, skity_image_filter>(
      out, &skity_image_filter_destroy, SKITY_OBJECT_TYPE_IMAGE_FILTER);
}

inline std::shared_ptr<ImageFilter> MakeImageFilterMatrixTransform(
    const Matrix& matrix) {
  return adopt_handle<skity_image_filter_s, ImageFilter, skity_image_filter>(
      skity_image_filter_create_matrix_transform(
          reinterpret_cast<const skity_matrix*>(&matrix)),
      &skity_image_filter_destroy, SKITY_OBJECT_TYPE_IMAGE_FILTER);
}

inline std::shared_ptr<ImageFilter> MakeImageFilterLocalMatrix(
    const std::shared_ptr<ImageFilter>& filter, const Matrix& matrix) {
  auto fh = wrap_handle<skity_image_filter_s, skity_image_filter>(
      SKITY_OBJECT_TYPE_IMAGE_FILTER, filter);
  auto out = skity_image_filter_create_local_matrix(
      fh, reinterpret_cast<const skity_matrix*>(&matrix));
  if (fh != nullptr) skity_image_filter_destroy(fh);
  return adopt_handle<skity_image_filter_s, ImageFilter, skity_image_filter>(
      out, &skity_image_filter_destroy, SKITY_OBJECT_TYPE_IMAGE_FILTER);
}

inline std::shared_ptr<ImageFilter> MakeImageFilterDropShadow(
    float dx, float dy, float sx, float sy, uint32_t color,
    const std::shared_ptr<ImageFilter>& input, const Rect* crop) {
  auto ih = wrap_handle<skity_image_filter_s, skity_image_filter>(
      SKITY_OBJECT_TYPE_IMAGE_FILTER, input);
  // A null crop maps to the legacy default (empty Rect), matching the C
  // entry's nullptr handling.
  skity_rect crop_rect{crop ? crop->Left() : 0.f, crop ? crop->Top() : 0.f,
                       crop ? crop->Right() : 0.f, crop ? crop->Bottom() : 0.f};
  auto out = skity_image_filter_create_drop_shadow(
      dx, dy, sx, sy, color, ih, crop != nullptr ? &crop_rect : nullptr);
  if (ih != nullptr) skity_image_filter_destroy(ih);
  return adopt_handle<skity_image_filter_s, ImageFilter, skity_image_filter>(
      out, &skity_image_filter_destroy, SKITY_OBJECT_TYPE_IMAGE_FILTER);
}

inline std::shared_ptr<MaskFilter> MakeMaskFilterBlur(BlurStyle style,
                                                      float radius) {
  return adopt_handle<skity_mask_filter_s, MaskFilter, skity_mask_filter>(
      skity_mask_filter_create_blur(static_cast<skity_blur_style>(style),
                                    radius),
      &skity_mask_filter_destroy, SKITY_OBJECT_TYPE_MASK_FILTER);
}

inline std::shared_ptr<PathEffect> MakePathEffectDiscrete(float seg_length,
                                                          float dev,
                                                          uint32_t seed) {
  return adopt_handle<skity_path_effect_s, PathEffect, skity_path_effect>(
      skity_path_effect_create_discrete(seg_length, dev, seed),
      &skity_path_effect_destroy, SKITY_OBJECT_TYPE_PATH_EFFECT);
}

inline std::shared_ptr<PathEffect> MakePathEffectDash(const float* intervals,
                                                      int32_t count,
                                                      float phase) {
  return adopt_handle<skity_path_effect_s, PathEffect, skity_path_effect>(
      skity_path_effect_create_dash(intervals, count, phase),
      &skity_path_effect_destroy, SKITY_OBJECT_TYPE_PATH_EFFECT);
}

inline std::shared_ptr<Shader> MakeShaderLinear(const Vec4* pts,
                                                const Vec4* colors,
                                                const float* pos, int32_t count,
                                                TileMode tile, int32_t flags) {
  return adopt_handle<skity_shader_s, Shader, skity_shader>(
      skity_shader_create_linear(reinterpret_cast<const skity_point*>(pts),
                                 reinterpret_cast<const skity_color4f*>(colors),
                                 pos, count, static_cast<skity_tile_mode>(tile),
                                 flags),
      &skity_shader_destroy, SKITY_OBJECT_TYPE_SHADER);
}

inline std::shared_ptr<Shader> MakeShaderRadial(const Vec4& center,
                                                float radius,
                                                const Vec4* colors,
                                                const float* pos, int32_t count,
                                                TileMode tile, int32_t flags) {
  return adopt_handle<skity_shader_s, Shader, skity_shader>(
      skity_shader_create_radial(
          reinterpret_cast<const skity_point&>(center), radius,
          reinterpret_cast<const skity_color4f*>(colors), pos, count,
          static_cast<skity_tile_mode>(tile), flags),
      &skity_shader_destroy, SKITY_OBJECT_TYPE_SHADER);
}

inline std::shared_ptr<Shader> MakeShaderSweep(float cx, float cy, float start,
                                               float end, const Vec4* colors,
                                               const float* pos, int32_t count,
                                               TileMode tile, int32_t flags) {
  return adopt_handle<skity_shader_s, Shader, skity_shader>(
      skity_shader_create_sweep(
          cx, cy, start, end, reinterpret_cast<const skity_color4f*>(colors),
          pos, count, static_cast<skity_tile_mode>(tile), flags),
      &skity_shader_destroy, SKITY_OBJECT_TYPE_SHADER);
}

inline std::shared_ptr<Shader> MakeShaderTwoPointConical(
    const Vec4& start, float start_radius, const Vec4& end, float end_radius,
    const Vec4* colors, const float* pos, int32_t count, TileMode tile,
    int32_t flags) {
  return adopt_handle<skity_shader_s, Shader, skity_shader>(
      skity_shader_create_two_point_conical(
          reinterpret_cast<const skity_point&>(start), start_radius,
          reinterpret_cast<const skity_point&>(end), end_radius,
          reinterpret_cast<const skity_color4f*>(colors), pos, count,
          static_cast<skity_tile_mode>(tile), flags),
      &skity_shader_destroy, SKITY_OBJECT_TYPE_SHADER);
}

inline std::shared_ptr<Shader> MakeShaderImage(
    const std::shared_ptr<Image>& image, const SamplingOptions& sampling,
    TileMode tx, TileMode ty, const Matrix& local_matrix) {
  auto ih =
      wrap_handle<skity_image_s, skity_image>(SKITY_OBJECT_TYPE_IMAGE, image);
  skity_sampling_options so{};
  so.filter = static_cast<skity_filter_mode>(sampling.filter);
  so.mipmap = static_cast<skity_mipmap_mode>(sampling.mipmap);
  so.cubic_b = sampling.cubic.B;
  so.cubic_c = sampling.cubic.C;
  auto out = skity_shader_create_image(
      ih, &so, static_cast<skity_tile_mode>(tx),
      static_cast<skity_tile_mode>(ty),
      reinterpret_cast<const skity_matrix*>(&local_matrix));
  if (ih != nullptr) skity_image_destroy(ih);
  return adopt_handle<skity_shader_s, Shader, skity_shader>(
      out, &skity_shader_destroy, SKITY_OBJECT_TYPE_SHADER);
}

inline void SetShaderLocalMatrix(const std::shared_ptr<Shader>& shader,
                                 const Matrix& matrix) {
  auto h = wrap_handle<skity_shader_s, skity_shader>(SKITY_OBJECT_TYPE_SHADER,
                                                     shader);
  if (h != nullptr) {
    skity_shader_set_local_matrix(
        h, reinterpret_cast<const skity_matrix*>(&matrix));
    skity_shader_destroy(h);
  }
}

//
// Paint value bridge. Paints held by the serialization layer are created
// through the C ABI and owned via shared_ptr, so construction / destruction /
// assignment never bind core symbols.
//

inline std::shared_ptr<Paint> MakePaint() {
  skity_paint h = skity_paint_create();
  if (h == nullptr) {
    return nullptr;
  }
  auto sp =
      skity::capi::get_impl<skity_paint_s, Paint>(h, SKITY_OBJECT_TYPE_PAINT);
  skity_paint_destroy(h);
  return sp;
}

inline skity_paint borrow_paint(const std::shared_ptr<Paint>& paint) {
  if (paint == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_paint_s>(SKITY_OBJECT_TYPE_PAINT,
                                                  SKITY_HANDLE_OWNING, paint);
}

inline void SetPaintColor(const std::shared_ptr<Paint>& paint, uint32_t color) {
  skity_paint h = borrow_paint(paint);
  if (h != nullptr) {
    skity_paint_set_color(h, color);
    skity_paint_destroy(h);
  }
}

inline void SetPaintStyle(const std::shared_ptr<Paint>& paint,
                          Paint::Style style) {
  skity_paint h = borrow_paint(paint);
  if (h != nullptr) {
    skity_paint_set_style(h, static_cast<skity_paint_style>(style));
    skity_paint_destroy(h);
  }
}

inline void SetPaintStrokeWidth(const std::shared_ptr<Paint>& paint,
                                float width) {
  skity_paint h = borrow_paint(paint);
  if (h != nullptr) {
    skity_paint_set_stroke_width(h, width);
    skity_paint_destroy(h);
  }
}

inline void SetPaintStrokeMiter(const std::shared_ptr<Paint>& paint,
                                float miter) {
  skity_paint h = borrow_paint(paint);
  if (h != nullptr) {
    skity_paint_set_stroke_miter(h, miter);
    skity_paint_destroy(h);
  }
}

inline void SetPaintStrokeCap(const std::shared_ptr<Paint>& paint,
                              Paint::Cap cap) {
  skity_paint h = borrow_paint(paint);
  if (h != nullptr) {
    skity_paint_set_stroke_cap(h, static_cast<skity_paint_cap>(cap));
    skity_paint_destroy(h);
  }
}

inline void SetPaintStrokeJoin(const std::shared_ptr<Paint>& paint,
                               Paint::Join join) {
  skity_paint h = borrow_paint(paint);
  if (h != nullptr) {
    skity_paint_set_stroke_join(h, static_cast<skity_paint_join>(join));
    skity_paint_destroy(h);
  }
}

inline void SetPaintPathEffect(const std::shared_ptr<Paint>& paint,
                               const std::shared_ptr<PathEffect>& effect) {
  skity_paint h = borrow_paint(paint);
  skity_path_effect eh = wrap_handle<skity_path_effect_s, skity_path_effect>(
      SKITY_OBJECT_TYPE_PATH_EFFECT, effect);
  if (h != nullptr) {
    skity_paint_set_path_effect(h, eh);
    skity_paint_destroy(h);
  }
  if (eh != nullptr) skity_path_effect_destroy(eh);
}

inline void SetPaintShader(const std::shared_ptr<Paint>& paint,
                           const std::shared_ptr<Shader>& shader) {
  skity_paint h = borrow_paint(paint);
  skity_shader sh = wrap_handle<skity_shader_s, skity_shader>(
      SKITY_OBJECT_TYPE_SHADER, shader);
  if (h != nullptr) {
    skity_paint_set_shader(h, sh);
    skity_paint_destroy(h);
  }
  if (sh != nullptr) skity_shader_destroy(sh);
}

inline void SetPaintColorFilter(const std::shared_ptr<Paint>& paint,
                                const std::shared_ptr<ColorFilter>& filter) {
  skity_paint h = borrow_paint(paint);
  skity_color_filter fh = wrap_handle<skity_color_filter_s, skity_color_filter>(
      SKITY_OBJECT_TYPE_COLOR_FILTER, filter);
  if (h != nullptr) {
    skity_paint_set_color_filter(h, fh);
    skity_paint_destroy(h);
  }
  if (fh != nullptr) skity_color_filter_destroy(fh);
}

inline void SetPaintImageFilter(const std::shared_ptr<Paint>& paint,
                                const std::shared_ptr<ImageFilter>& filter) {
  skity_paint h = borrow_paint(paint);
  skity_image_filter fh = wrap_handle<skity_image_filter_s, skity_image_filter>(
      SKITY_OBJECT_TYPE_IMAGE_FILTER, filter);
  if (h != nullptr) {
    skity_paint_set_image_filter(h, fh);
    skity_paint_destroy(h);
  }
  if (fh != nullptr) skity_image_filter_destroy(fh);
}

inline void SetPaintAntiAlias(const std::shared_ptr<Paint>& paint, bool aa) {
  skity_paint h = borrow_paint(paint);
  if (h != nullptr) {
    skity_paint_set_anti_alias(h, aa ? 1u : 0u);
    skity_paint_destroy(h);
  }
}

inline void SetPaintMaskFilter(const std::shared_ptr<Paint>& paint,
                               const std::shared_ptr<MaskFilter>& filter) {
  skity_paint h = borrow_paint(paint);
  skity_mask_filter fh = wrap_handle<skity_mask_filter_s, skity_mask_filter>(
      SKITY_OBJECT_TYPE_MASK_FILTER, filter);
  if (h != nullptr) {
    skity_paint_set_mask_filter(h, fh);
    skity_paint_destroy(h);
  }
  if (fh != nullptr) skity_mask_filter_destroy(fh);
}

//
// Path construction bridge: builds through the C path entries and yields an
// owning shared_ptr<Path>.
//

class PathBuilder {
 public:
  PathBuilder() : path_(skity_path_create()) {}
  ~PathBuilder() { skity_path_destroy(path_); }
  PathBuilder(const PathBuilder&) = delete;
  PathBuilder& operator=(const PathBuilder&) = delete;

  void MoveTo(float x, float y) { skity_path_move_to(path_, x, y); }
  void LineTo(float x, float y) { skity_path_line_to(path_, x, y); }
  void QuadTo(float x1, float y1, float x2, float y2) {
    skity_path_quad_to(path_, x1, y1, x2, y2);
  }
  void ConicTo(float x1, float y1, float x2, float y2, float w) {
    skity_path_conic_to(path_, x1, y1, x2, y2, w);
  }
  void CubicTo(float x1, float y1, float x2, float y2, float x3, float y3) {
    skity_path_cubic_to(path_, x1, y1, x2, y2, x3, y3);
  }
  void Close() { skity_path_close(path_); }
  void ArcTo(float rx, float ry, float x_axis_rotate, Path::ArcSize large_arc,
             Path::Direction sweep, float x, float y) {
    skity_path_arc_to_svg(path_, rx, ry, x_axis_rotate,
                          static_cast<skity_arc_size>(large_arc),
                          static_cast<skity_path_direction>(sweep), x, y);
  }
  void AddRRect(const Rect& rect, const Vec2 radii[4], Path::Direction dir) {
    skity_path_add_round_rect_radii(path_,
                                    reinterpret_cast<const skity_rect*>(&rect),
                                    reinterpret_cast<const float*>(radii),
                                    static_cast<skity_path_direction>(dir));
  }
  void AddRRectStart(const Rect& rect, const Vec2 radii[4], Path::Direction dir,
                     int32_t start) {
    skity_path_add_round_rect_radii_start(
        path_, reinterpret_cast<const skity_rect*>(&rect),
        reinterpret_cast<const float*>(radii),
        static_cast<skity_path_direction>(dir), start);
  }
  void SetFillType(Path::PathFillType type) {
    skity_path_set_fill_type(path_, static_cast<skity_path_fill_type>(type));
  }

  // Borrowed shared_ptr view of the path under construction (the handle
  // stays owned by this builder).
  std::shared_ptr<Path> Peek() const {
    return skity::capi::get_impl<skity_path_s, Path>(path_,
                                                     SKITY_OBJECT_TYPE_PATH);
  }

  std::shared_ptr<Path> Take() {
    if (path_ == nullptr) {
      return nullptr;
    }
    auto sp = skity::capi::get_impl<skity_path_s, Path>(path_,
                                                        SKITY_OBJECT_TYPE_PATH);
    skity_path_destroy(path_);
    path_ = nullptr;
    return sp;
  }

 private:
  skity_path path_;
};

//
// Font / TextBlob reconstruction bridge.
//

inline std::shared_ptr<Font> MakeFont() {
  skity_font h = skity_font_create();
  if (h == nullptr) {
    return nullptr;
  }
  auto sp =
      skity::capi::get_impl<skity_font_s, Font>(h, SKITY_OBJECT_TYPE_FONT);
  skity_font_destroy(h);
  return sp;
}

inline void SetFontSize(const std::shared_ptr<Font>& font, float size) {
  if (font == nullptr) {
    return;
  }
  skity_font h = skity::capi::alloc_handle<skity_font_s>(
      SKITY_OBJECT_TYPE_FONT, SKITY_HANDLE_OWNING, font);
  if (h != nullptr) {
    skity_font_set_size(h, size);
    skity_font_destroy(h);
  }
}

// One deserialized run: font plus parallel glyph / position arrays.
struct TextRunData {
  std::shared_ptr<Font> font;
  std::vector<uint16_t> glyphs;
  std::vector<float> pos_x;
  std::vector<float> pos_y;
};

inline std::shared_ptr<TextBlob> MakeTextBlobFromRuns(
    const std::vector<TextRunData>& runs) {
  if (runs.empty()) {
    return nullptr;
  }
  std::vector<skity_text_run_desc> descs(runs.size());
  std::vector<skity_font> fonts(runs.size(), nullptr);
  for (size_t i = 0; i < runs.size(); i++) {
    fonts[i] =
        runs[i].font != nullptr
            ? skity::capi::alloc_handle<skity_font_s>(
                  SKITY_OBJECT_TYPE_FONT, SKITY_HANDLE_OWNING, runs[i].font)
            : nullptr;
    descs[i].font = fonts[i];
    descs[i].glyphs = runs[i].glyphs.data();
    descs[i].pos_x = runs[i].pos_x.empty() ? nullptr : runs[i].pos_x.data();
    descs[i].pos_y = runs[i].pos_y.empty() ? nullptr : runs[i].pos_y.data();
    descs[i].count = runs[i].glyphs.size();
  }
  skity_text_blob blob =
      skity_text_blob_create_from_runs(descs.data(), descs.size());
  for (auto f : fonts) {
    if (f != nullptr) skity_font_destroy(f);
  }
  if (blob == nullptr) {
    return nullptr;
  }
  auto sp = skity::capi::get_impl<skity_text_blob_s, TextBlob>(
      blob, SKITY_OBJECT_TYPE_TEXT_BLOB);
  skity_text_blob_destroy(blob);
  return sp;
}

//
// Paint readers (write side of the flat layer). Each borrows the paint as
// a handle and reads through the C ABI.
//

inline Color4f GetPaintColor4f(const Paint* paint) {
  Color4f out{};
  if (paint == nullptr) {
    return out;
  }
  std::shared_ptr<Paint> view(const_cast<Paint*>(paint), [](Paint*) {});
  skity_paint h = skity::capi::alloc_handle<skity_paint_s>(
      SKITY_OBJECT_TYPE_PAINT, 0, std::move(view));
  if (h != nullptr) {
    skity_paint_get_color4f(h, reinterpret_cast<skity_color4f*>(&out));
    skity_paint_destroy(h);
  }
  return out;
}

inline Paint::Cap GetPaintStrokeCap(const Paint* paint) {
  std::shared_ptr<Paint> view(const_cast<Paint*>(paint), [](Paint*) {});
  skity_paint h = skity::capi::alloc_handle<skity_paint_s>(
      SKITY_OBJECT_TYPE_PAINT, 0, std::move(view));
  if (h == nullptr) {
    return Paint::Cap::kButt_Cap;
  }
  auto out = static_cast<Paint::Cap>(skity_paint_get_stroke_cap(h));
  skity_paint_destroy(h);
  return out;
}

inline Paint::Join GetPaintStrokeJoin(const Paint* paint) {
  std::shared_ptr<Paint> view(const_cast<Paint*>(paint), [](Paint*) {});
  skity_paint h = skity::capi::alloc_handle<skity_paint_s>(
      SKITY_OBJECT_TYPE_PAINT, 0, std::move(view));
  if (h == nullptr) {
    return Paint::Join::kMiter_Join;
  }
  auto out = static_cast<Paint::Join>(skity_paint_get_stroke_join(h));
  skity_paint_destroy(h);
  return out;
}

inline float GetPaintStrokeMiter(const Paint* paint) {
  std::shared_ptr<Paint> view(const_cast<Paint*>(paint), [](Paint*) {});
  skity_paint h = skity::capi::alloc_handle<skity_paint_s>(
      SKITY_OBJECT_TYPE_PAINT, 0, std::move(view));
  float out = h != nullptr ? skity_paint_get_stroke_miter(h) : 4.f;
  if (h != nullptr) skity_paint_destroy(h);
  return out;
}

inline float GetPaintStrokeWidth(const Paint* paint) {
  std::shared_ptr<Paint> view(const_cast<Paint*>(paint), [](Paint*) {});
  skity_paint h = skity::capi::alloc_handle<skity_paint_s>(
      SKITY_OBJECT_TYPE_PAINT, 0, std::move(view));
  float out = h != nullptr ? skity_paint_get_stroke_width(h) : 0.f;
  if (h != nullptr) skity_paint_destroy(h);
  return out;
}

inline Paint::Style GetPaintStyle(const Paint* paint) {
  std::shared_ptr<Paint> view(const_cast<Paint*>(paint), [](Paint*) {});
  skity_paint h = skity::capi::alloc_handle<skity_paint_s>(
      SKITY_OBJECT_TYPE_PAINT, 0, std::move(view));
  auto out = h != nullptr ? static_cast<Paint::Style>(skity_paint_get_style(h))
                          : Paint::Style::kFill_Style;
  if (h != nullptr) skity_paint_destroy(h);
  return out;
}

inline bool IsPaintAntiAlias(const Paint* paint) {
  std::shared_ptr<Paint> view(const_cast<Paint*>(paint), [](Paint*) {});
  skity_paint h = skity::capi::alloc_handle<skity_paint_s>(
      SKITY_OBJECT_TYPE_PAINT, 0, std::move(view));
  bool out = h != nullptr && skity_paint_is_anti_alias(h) != 0;
  if (h != nullptr) skity_paint_destroy(h);
  return out;
}

inline skity_color GetPaintColor(const Paint* paint) {
  std::shared_ptr<Paint> view(const_cast<Paint*>(paint), [](Paint*) {});
  skity_paint h = skity::capi::alloc_handle<skity_paint_s>(
      SKITY_OBJECT_TYPE_PAINT, 0, std::move(view));
  skity_color out = h != nullptr ? skity_paint_get_color(h) : 0xFF000000u;
  if (h != nullptr) skity_paint_destroy(h);
  return out;
}

inline skity_blend_mode GetPaintBlendMode(const Paint* paint) {
  std::shared_ptr<Paint> view(const_cast<Paint*>(paint), [](Paint*) {});
  skity_paint h = skity::capi::alloc_handle<skity_paint_s>(
      SKITY_OBJECT_TYPE_PAINT, 0, std::move(view));
  auto out =
      h != nullptr ? skity_paint_get_blend_mode(h) : SKITY_BLEND_MODE_SRC_OVER;
  if (h != nullptr) skity_paint_destroy(h);
  return out;
}

//
// TextBlob readers for the flat writer (runs / bounds through the Phase-1
// bulk entries).
//

inline Rect GetTextBlobBounds(const TextBlob* blob) {
  Rect out{};
  if (blob == nullptr) {
    return out;
  }
  std::shared_ptr<TextBlob> view(const_cast<TextBlob*>(blob), [](TextBlob*) {});
  skity_text_blob h = skity::capi::alloc_handle<skity_text_blob_s>(
      SKITY_OBJECT_TYPE_TEXT_BLOB, 0, std::move(view));
  if (h != nullptr) {
    skity_rect r{};
    skity_text_blob_get_bounds(h, &r);
    out = *reinterpret_cast<const Rect*>(&r);
    skity_text_blob_destroy(h);
  }
  return out;
}

inline int32_t GetTextBlobRunCount(const TextBlob* blob) {
  if (blob == nullptr) {
    return 0;
  }
  std::shared_ptr<TextBlob> view(const_cast<TextBlob*>(blob), [](TextBlob*) {});
  skity_text_blob h = skity::capi::alloc_handle<skity_text_blob_s>(
      SKITY_OBJECT_TYPE_TEXT_BLOB, 0, std::move(view));
  int32_t out = h != nullptr ? skity_text_blob_get_run_count(h) : 0;
  if (h != nullptr) skity_text_blob_destroy(h);
  return out;
}

// Borrows the blob as a handle for the duration of @p fn(run_handle).
template <typename Fn>
inline auto WithTextBlobHandle(const TextBlob* blob, Fn&& fn)
    -> decltype(fn(skity_text_blob{})) {
  std::shared_ptr<TextBlob> view(const_cast<TextBlob*>(blob), [](TextBlob*) {});
  skity_text_blob h = skity::capi::alloc_handle<skity_text_blob_s>(
      SKITY_OBJECT_TYPE_TEXT_BLOB, 0, std::move(view));
  auto out = fn(h);
  if (h != nullptr) skity_text_blob_destroy(h);
  return out;
}

//
// Path bounds through the C ABI (the core accessor is out-of-line).
//

inline Rect GetPathBounds(const Path* path) {
  Rect out{};
  if (path == nullptr) {
    return out;
  }
  std::shared_ptr<Path> view(const_cast<Path*>(path), [](Path*) {});
  skity_path h = skity::capi::alloc_handle<skity_path_s>(SKITY_OBJECT_TYPE_PATH,
                                                         0, std::move(view));
  if (h != nullptr) {
    skity_rect r{};
    skity_path_get_bounds(h, &r);
    out = *reinterpret_cast<const Rect*>(&r);
    skity_path_destroy(h);
  }
  return out;
}

//
// Matrix / FontStyle helpers mirroring out-of-line core functions.
//

// Bit-identical to core Matrix::RotateDeg(deg) / RotateDeg(deg, pt): a
// rotation about the z axis wrapped in the pre/post translations.
inline Matrix RotateDegMatrix(float deg, Vec2 pt = Vec2{0.f, 0.f}) {
  const float rad =
      deg * static_cast<float>(0.01745329251994329576923690768489);
  glm::mat4 d1(1.f);
  d1[3][0] = -pt.x;
  d1[3][1] = -pt.y;
  glm::mat4 d2(1.f);
  d2[3][0] = pt.x;
  d2[3][1] = pt.y;
  glm::mat4 rotated = glm::rotate(d2, rad, glm::vec3(0.f, 0.f, 1.f)) * d1;
  Matrix m{};
  std::memcpy(&m, &rotated, sizeof(m));
  return m;
}

// Bit-identical to core Matrix::IsIdentity (glm::isIdentity with the
// default epsilon over the 4x4 word).
inline bool MatrixIsIdentity(const Matrix& m) {
  return glm::isIdentity(*reinterpret_cast<const glm::mat4*>(&m),
                         glm::epsilon<float>());
}

// Bit-identical to core Matrix::HasPersp.
inline bool MatrixHasPersp(const Matrix& m) {
  return m[0][3] != 0 || m[1][3] != 0 || m[2][3] != 0 || m[3][3] != 1;
}

// Bit-identical to core Rect::CenterX / CenterY.
inline float RectCenterX(const Rect& r) {
  return 0.5f * (r.Right() + r.Left());
}

inline float RectCenterY(const Rect& r) {
  return 0.5f * (r.Top() + r.Bottom());
}

// Bit-identical to core FontStyle::FontStyle: each component is clamped to
// its enum range before packing into the single storage word. FontStyle's
// constructor lives in the core library, so the packed word is stored
// directly (the layout is pinned by the static_assert).
inline void StoreFontStyle(FontStyle& out, int weight, int width,
                           FontStyle::Slant slant) {
  static_assert(sizeof(FontStyle) == sizeof(uint32_t),
                "FontStyle must stay a single packed word");
  struct Limits {
    static int clamp(int v, int lo, int hi) {
      return v < lo ? lo : (v > hi ? hi : v);
    }
  };
  uint32_t packed = Limits::clamp(weight, FontStyle::kInvisible_Weight,
                                  FontStyle::kExtraBlack_Weight) +
                    (Limits::clamp(width, FontStyle::kUltraCondensed_Width,
                                   FontStyle::kUltraExpanded_Width)
                     << 16) +
                    (Limits::clamp(static_cast<int>(slant),
                                   static_cast<int>(FontStyle::kUpright_Slant),
                                   static_cast<int>(FontStyle::kOblique_Slant))
                     << 24);
  std::memcpy(&out, &packed, sizeof(packed));
}

namespace font_style_detail {

// Mirror of the core axis-width interpolation tables (font_style.cc).
inline float InterpAxisWidth(float x) {
  static constexpr float kKeys[9] = {50.f,   62.5f, 75.f,  87.5f, 100.f,
                                     112.5f, 125.f, 150.f, 200.f};
  static constexpr float kValues[9] = {1.f, 2.f, 3.f, 4.f, 5.f,
                                       6.f, 7.f, 8.f, 9.f};
  if (x <= kKeys[0]) {
    return kValues[0];
  }
  for (int i = 0; i < 8; i++) {
    if (x <= kKeys[i + 1]) {
      float t = (x - kKeys[i]) / (kKeys[i + 1] - kKeys[i]);
      return kValues[i] + t * (kValues[i + 1] - kValues[i]);
    }
  }
  return kValues[8];
}

}  // namespace font_style_detail

// Bit-identical to core FontStyle::WidthFromAxisWidth.
inline FontStyle::Width WidthFromAxisWidth(float axis_width) {
  float value = font_style_detail::InterpAxisWidth(axis_width);
  return static_cast<FontStyle::Width>(static_cast<int>(std::round(value)));
}

//
// Small value helpers mirroring out-of-line core functions.
//

// Mirrors core Color4fToColor bit for bit: each channel is scaled to [0,
// 255], clamped, and truncated (no rounding).
inline uint32_t Color4fToColor(const Vec4& color) {
  auto to_byte = [](float value) -> uint32_t {
    float scaled = value * 255.f;
    if (scaled < 0.f) {
      scaled = 0.f;
    }
    if (scaled > 255.f) {
      scaled = 255.f;
    }
    return static_cast<uint32_t>(scaled);
  };
  return (to_byte(color.w) << 24) | (to_byte(color.x) << 16) |
         (to_byte(color.y) << 8) | (to_byte(color.z));
}

}  // namespace core_bridge
}  // namespace skity

#endif  // MODULE_IO_SRC_IO_CORE_BRIDGE_HPP
