// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GRAPHIC_PAINT_HPP
#define MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GRAPHIC_PAINT_HPP

#include <skity/c/skity_paint.h>

#include <cstdint>
#include <memory>
#include <skity/base.hpp>
#include <skity/effect/color_filter.hpp>
#include <skity/effect/image_filter.hpp>
#include <skity/effect/mask_filter.hpp>
#include <skity/effect/path_effect.hpp>
#include <skity/effect/shader.hpp>
#include <skity/types.hpp>

// text/text.hpp is intentionally NOT included here: it includes this header
// (TextBlob::Make takes a Paint), so the dependency must stay
// one-directional. Typeface is therefore only forward-declared and its
// Paint accessors are defined out-of-line in text/text.hpp — include that
// header when using them.

namespace skity {

class Typeface;

/**
 * @class Paint
 *
 * Style, color, stroke parameters, blend mode, effects.
 *
 */
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

  // Legacy-shaped enumerator aliases so code written against the legacy C++
  // API keeps compiling when it is moved onto the wrapper (the unified shape
  // has no dual-API TUs — legacy headers and the wrapper cannot coexist).
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
   * Copy semantics of the value-type skity::Paint: parameters are
   * duplicated while attached effects / typeface are shared (the same
   * objects, referenced twice) — identical to copying a skity::Paint.
   * Move stays cheap.
   */
  Paint(const Paint& other) : OwnHandle(skity_paint_clone(other.get())) {}

  Paint& operator=(const Paint& other) {
    if (this != &other) reset(skity_paint_clone(other.get()));
    return *this;
  }

  Paint(Paint&&) = default;
  Paint& operator=(Paint&&) = default;

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

  /** Color as float RGBA (linear, unpremultiplied). */
  Color4f GetColor4f() const {
    Color4f out{};
    skity_paint_get_color4f(get(), &out);
    return out;
  }

  /** Alpha as a float in [0, 1]. */
  float GetAlphaF() const { return skity_paint_get_alpha_f(get()); }

  /** SDF rasterization for text below the font threshold. */
  void SetSDFForSmallText(bool enable) {
    skity_paint_set_sdf_for_small_text(get(), enable ? 1u : 0u);
  }
  bool IsSDFForSmallText() const {
    return skity_paint_is_sdf_for_small_text(get()) != 0u;
  }

  /** Text-size threshold for fill vs SDF rendering (default 256). */
  void SetFontThreshold(float font_size) {
    skity_paint_set_font_threshold(get(), font_size);
  }
  float GetFontThreshold() const {
    return skity_paint_get_font_threshold(get());
  }

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
   * Effect / typeface attach — the legacy shared_ptr signatures. The paint
   * takes a shared reference, so passing a factory temporary works
   * (`paint.SetShader(Shader::MakeLinear(...))`) and a null pointer
   * detaches the effect. Handles obtained through the raw C API are
   * wrapped with the class's Adopt first.
   */
  void SetShader(std::shared_ptr<Shader> shader) {
    skity_paint_set_shader(get(), shader ? shader->get() : nullptr);
  }
  void SetColorFilter(std::shared_ptr<ColorFilter> filter) {
    skity_paint_set_color_filter(get(), filter ? filter->get() : nullptr);
  }
  void SetImageFilter(std::shared_ptr<ImageFilter> filter) {
    skity_paint_set_image_filter(get(), filter ? filter->get() : nullptr);
  }
  void SetMaskFilter(std::shared_ptr<MaskFilter> filter) {
    skity_paint_set_mask_filter(get(), filter ? filter->get() : nullptr);
  }
  void SetPathEffect(std::shared_ptr<PathEffect> effect) {
    skity_paint_set_path_effect(get(), effect ? effect->get() : nullptr);
  }
  /** Defined in text/text.hpp (Typeface is forward-declared here). */
  void SetTypeface(std::shared_ptr<Typeface> typeface);
  std::shared_ptr<Typeface> GetTypeface() const;

  /**
   * Effect getters, the legacy shared_ptr shape: the returned wrapper owns
   * a shared reference, so it keeps the effect alive even if the paint is
   * destroyed; a null pointer when none is attached. (Each call adopts a
   * fresh shared reference, so identity comparison between two calls is not
   * meaningful — unlike the legacy member-cached shared_ptr.)
   */
  std::shared_ptr<Shader> GetShader() const {
    skity_shader h = skity_paint_get_shader(get());
    return h != nullptr ? std::shared_ptr<Shader>(new Shader(h)) : nullptr;
  }
  std::shared_ptr<ColorFilter> GetColorFilter() const {
    skity_color_filter h = skity_paint_get_color_filter(get());
    return h != nullptr ? std::shared_ptr<ColorFilter>(new ColorFilter(h))
                        : nullptr;
  }
  std::shared_ptr<ImageFilter> GetImageFilter() const {
    skity_image_filter h = skity_paint_get_image_filter(get());
    return h != nullptr ? std::shared_ptr<ImageFilter>(new ImageFilter(h))
                        : nullptr;
  }
  std::shared_ptr<MaskFilter> GetMaskFilter() const {
    skity_mask_filter h = skity_paint_get_mask_filter(get());
    return h != nullptr ? std::shared_ptr<MaskFilter>(new MaskFilter(h))
                        : nullptr;
  }
  std::shared_ptr<PathEffect> GetPathEffect() const {
    skity_path_effect h = skity_paint_get_path_effect(get());
    return h != nullptr ? std::shared_ptr<PathEffect>(new PathEffect(h))
                        : nullptr;
  }

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

}  // namespace skity

#endif  // MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GRAPHIC_PAINT_HPP
