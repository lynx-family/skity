// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_PAINT_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_PAINT_HPP

// Paint wrapper of the header-only RAII layer: style, color, stroke
// parameters, blend mode, effects. See skity.hpp for the layer's design.

#include <skity_c/skity_paint.h>

#include <cstdint>
#include <skity_hpp/skity_base.hpp>
#include <skity_hpp/skity_color_filter.hpp>
#include <skity_hpp/skity_image_filter.hpp>
#include <skity_hpp/skity_mask_filter.hpp>
#include <skity_hpp/skity_path_effect.hpp>
#include <skity_hpp/skity_shader.hpp>
#include <skity_hpp/skity_types.hpp>

// skity_hpp/skity_text.hpp is intentionally NOT included here: it includes
// this header (TextBlob::Make takes a Paint), so the dependency must stay
// one-directional. The Typeface members therefore use the raw skity_typeface
// handle (available through skity_c/skity_paint.h); wrap results with
// Typeface::Adopt.

namespace skity {
namespace raii {

/** Paint wrapper: style, color, stroke parameters, blend mode, effects. */
class Paint : public detail::OwnHandle<skity_paint, skity_paint_destroy> {
 public:
  enum class Style : uint32_t {
    kFill = SKITY_PAINT_STYLE_FILL,
    kStroke = SKITY_PAINT_STYLE_STROKE,
    kStrokeAndFill = SKITY_PAINT_STYLE_STROKE_AND_FILL,
    kStrokeThenFill = SKITY_PAINT_STYLE_STROKE_THEN_FILL,
  };

  enum class Cap : uint32_t {
    kButt = SKITY_PAINT_CAP_BUTT,
    kRound = SKITY_PAINT_CAP_ROUND,
    kSquare = SKITY_PAINT_CAP_SQUARE,
  };

  enum class Join : uint32_t {
    kMiter = SKITY_PAINT_JOIN_MITER,
    kRound = SKITY_PAINT_JOIN_ROUND,
    kBevel = SKITY_PAINT_JOIN_BEVEL,
  };

  // Legacy-shaped enumerator aliases so dual-API source files can write
  // sk::Paint::kFill_Style and compile against both the legacy C++ API and
  // this wrapper unchanged.
  static constexpr Style kFill_Style{Style::kFill};
  static constexpr Style kStroke_Style{Style::kStroke};
  static constexpr Style kStrokeAndFill_Style{Style::kStrokeAndFill};
  static constexpr Style kStrokeThenFill_Style{Style::kStrokeThenFill};
  static constexpr Cap kButt_Cap{Cap::kButt};
  static constexpr Cap kRound_Cap{Cap::kRound};
  static constexpr Cap kSquare_Cap{Cap::kSquare};
  static constexpr Join kMiter_Join{Join::kMiter};
  static constexpr Join kRound_Join{Join::kRound};
  static constexpr Join kBevel_Join{Join::kBevel};

  Paint() : OwnHandle(skity_paint_create()) {}

  /**
   * Wrap an existing handle. For handles owning a paint reference this
   * adopts it (destroy releases the paint); for non-owning handles borrowed
   * from a DisplayList's storage, skity_paint_destroy only reclaims the
   * wrapper, so adopting those is equally safe.
   */
  static Paint Adopt(skity_paint h) {
    Paint p;
    p.reset(h);
    return p;
  }

  void Reset() { skity_paint_reset(get()); }

  void SetStyle(Style style) {
    skity_paint_set_style(get(), static_cast<skity_paint_style>(style));
  }
  Style GetStyle() const {
    return static_cast<Style>(skity_paint_get_style(get()));
  }

  void SetStrokeWidth(float width) {
    skity_paint_set_stroke_width(get(), width);
  }
  float GetStrokeWidth() const { return skity_paint_get_stroke_width(get()); }

  void SetStrokeCap(Cap cap) {
    skity_paint_set_stroke_cap(get(), static_cast<skity_paint_cap>(cap));
  }
  Cap GetStrokeCap() const {
    return static_cast<Cap>(skity_paint_get_stroke_cap(get()));
  }

  void SetStrokeJoin(Join join) {
    skity_paint_set_stroke_join(get(), static_cast<skity_paint_join>(join));
  }
  Join GetStrokeJoin() const {
    return static_cast<Join>(skity_paint_get_stroke_join(get()));
  }

  void SetStrokeMiter(float miter) {
    skity_paint_set_stroke_miter(get(), miter);
  }
  float GetStrokeMiter() const { return skity_paint_get_stroke_miter(get()); }

  void SetColor(Color color) { skity_paint_set_color(get(), color); }
  Color GetColor() const { return skity_paint_get_color(get()); }

  void SetStrokeColor(Color color) {
    skity_paint_set_stroke_color(get(), color);
  }
  void SetFillColor(Color color) { skity_paint_set_fill_color(get(), color); }

  /**
   * Floating-point RGBA overloads (legacy shapes; each channel in [0, 1]).
   * Packed to 8-bit ARGB before crossing the C ABI.
   */
  void SetFillColor(float r, float g, float b, float a) {
    SetFillColor(ColorPackRGBA(r, g, b, a));
  }
  void SetStrokeColor(float r, float g, float b, float a) {
    SetStrokeColor(ColorPackRGBA(r, g, b, a));
  }

  void SetAlpha(uint8_t alpha) { skity_paint_set_alpha(get(), alpha); }
  void SetAlphaF(float alpha) { skity_paint_set_alpha_f(get(), alpha); }
  uint8_t GetAlpha() const { return skity_paint_get_alpha(get()); }

  void SetBlendMode(BlendMode mode) {
    skity_paint_set_blend_mode(get(), to_c(mode));
  }
  BlendMode GetBlendMode() const {
    return static_cast<BlendMode>(skity_paint_get_blend_mode(get()));
  }

  void SetAntiAlias(bool aa) {
    skity_paint_set_anti_alias(get(), aa ? 1u : 0u);
  }
  bool IsAntiAlias() const { return skity_paint_is_anti_alias(get()) != 0; }

  void SetTextSize(float size) { skity_paint_set_text_size(get(), size); }
  float GetTextSize() const { return skity_paint_get_text_size(get()); }

  /**
   * Attach an effect or typeface (raw-handle overloads). The paint takes a
   * shared reference, so the passed (owning) handle may be destroyed
   * immediately afterwards — passing e.g. a temporary wrapper works:
   * `paint.SetShader(shader.get());`.
   */
  void SetShader(skity_shader shader) { skity_paint_set_shader(get(), shader); }
  void SetColorFilter(skity_color_filter filter) {
    skity_paint_set_color_filter(get(), filter);
  }
  void SetImageFilter(skity_image_filter filter) {
    skity_paint_set_image_filter(get(), filter);
  }
  void SetMaskFilter(skity_mask_filter filter) {
    skity_paint_set_mask_filter(get(), filter);
  }
  void SetPathEffect(skity_path_effect effect) {
    skity_paint_set_path_effect(get(), effect);
  }
  void SetTypeface(skity_typeface typeface) {
    skity_paint_set_typeface(get(), typeface);
  }

  /** Wrapper-object overloads (accept temporaries: the paint shares). */
  void SetShader(const Shader& shader) { SetShader(shader.get()); }
  void SetColorFilter(const ColorFilter& filter) {
    SetColorFilter(filter.get());
  }
  void SetImageFilter(const ImageFilter& filter) {
    SetImageFilter(filter.get());
  }
  void SetMaskFilter(const MaskFilter& filter) { SetMaskFilter(filter.get()); }
  void SetPathEffect(const PathEffect& effect) { SetPathEffect(effect.get()); }

  /**
   * Effect getters: the returned wrappers own a shared reference (adopting
   * the handle the C layer created), so they keep the effect alive even if
   * the paint is destroyed; empty when none is attached.
   */
  Shader GetShader() const {
    return Shader::Adopt(skity_paint_get_shader(get()));
  }
  ColorFilter GetColorFilter() const {
    return ColorFilter::Adopt(skity_paint_get_color_filter(get()));
  }
  ImageFilter GetImageFilter() const {
    return ImageFilter::Adopt(skity_paint_get_image_filter(get()));
  }
  MaskFilter GetMaskFilter() const {
    return MaskFilter::Adopt(skity_paint_get_mask_filter(get()));
  }
  PathEffect GetPathEffect() const {
    return PathEffect::Adopt(skity_paint_get_path_effect(get()));
  }

  /**
   * Typeface access through the raw handle (see the include note above):
   * wrap with Typeface::Adopt. NULL when none is attached.
   */
  skity_typeface GetTypeface() const { return skity_paint_get_typeface(get()); }

  /** Split stroke / fill colors as floating-point RGBA (legacy getters). */
  Color4f GetStrokeColor() const {
    Color4f out{};
    skity_paint_get_stroke_color(get(), &out);
    return out;
  }
  Color4f GetFillColor() const {
    Color4f out{};
    skity_paint_get_fill_color(get(), &out);
    return out;
  }
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_PAINT_HPP
