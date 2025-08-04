// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GRAPHIC_PAINT_HPP
#define INCLUDE_SKITY_GRAPHIC_PAINT_HPP

#include <cstdint>
#include <memory>
#include <skity/geometry/point.hpp>
#include <skity/geometry/rect.hpp>
#include <skity/graphic/blend_mode.hpp>
#include <skity/graphic/color.hpp>
#include <skity/macros.hpp>
#include <utility>

namespace skity {

class ColorFilter;
class ImageFilter;
class MaskFilter;
class PathEffect;
class Shader;

class Typeface;

/**
 * @class Paint
 * Controls options applied when drawing.
 */
class SKITY_API Paint {
  enum {
    DEFAULT_FONT_FILL_THRESHOLD = 256,
  };

 public:
  Paint();
  Paint(const Paint&) = default;

  ~Paint();

  Paint& operator=(const Paint& paint);

  void Reset();

  enum Style : std::uint8_t {
    kFill_Style,            /// set to fill geometry
    kStroke_Style,          /// set to stroke geometry
    kStrokeAndFill_Style,   /// set to fill then stroke geometry
    kStrokeThenFill_Style,  /// set to stroke then fill geometry
  };
  // may be used to verify that Paint::Style is a legal value
  static constexpr int32_t StyleCount = kStrokeThenFill_Style + 1;

  Style GetStyle() const;

  void SetStyle(Style style);

  /**
   * Set the thickness of the pen used by the paint to outline the shape.
   * @TODO may be support stroke-width of zero as hairline
   * @param width pen thickness
   */
  void SetStrokeWidth(float width);

  float GetStrokeWidth() const;

  float GetStrokeMiter() const;

  /**
   * Set the limit at which a sharp corner is drawn beveled.
   */
  void SetStrokeMiter(float miter);

  /**
   * @enum Paint::Cap
   *
   * Cap draws at the beginning and end of an open path contour.
   */
  enum Cap : std::uint8_t {
    kButt_Cap,                 /// no stroke extension
    kRound_Cap,                /// add circle
    kSquare_Cap,               /// add square
    kLast_Cap = kSquare_Cap,   /// largest Cap value
    kDefault_Cap = kButt_Cap,  /// equivalent to kButt_Cap
  };

  static constexpr std::int32_t kCapCount = kLast_Cap + 1;

  Cap GetStrokeCap() const;

  void SetStrokeCap(Cap cap);

  /**
   * @enum Paint::Join
   *
   * Join specifies how corners are drawn when a shape is stroked.
   */
  enum Join : std::uint8_t {
    kMiter_Join,                  /// extends to miter limit
    kRound_Join,                  /// add circle
    kBevel_Join,                  /// connects outside edges
    kLast_Join = kBevel_Join,     /// equivalent to the largest value for Join
    kDefault_Join = kMiter_Join,  /// equivalent to kMiter_Join
  };

  static constexpr std::int32_t kJoinCount = kLast_Join + 1;

  Join GetStrokeJoin() const;

  void SetStrokeJoin(Join join);

  static constexpr const float kDefaultMiterLimit = 4.f;

  /**
   *  @deprecated, use SetStrokeColor(Color color) instead
   */
  void SetStrokeColor(float r, float g, float b, float a);

  /**
   *  @deprecated, use SetStrokeColor(Color color) instead
   */
  void SetStrokeColor(Vector const& color);

  Vector GetStrokeColor() const;

  /**
   *  @deprecated, use SetFillColor(Color color) instead
   */
  void SetFillColor(float r, float g, float b, float a);

  /**
   *  @deprecated, use SetFillColor(Color color) instead
   */
  void SetFillColor(Vector const& color);

  Vector GetFillColor() const;

  void SetStrokeColor(Color color);

  void SetFillColor(Color color);

  /**
   * Sets alpha and RGB used when stroking and filling. The color is a 32-bit
   * value, unpremultiplied, packing 8-bit components for alpha, red, blue, and
   * green.
   * @param color   unpremultiplied ARGB
   */
  void SetColor(Color color);

  Color GetColor() const;

  Color4f GetColor4f() const;
  /**
   * Requests, but does not require, that edge pixels draw opaque or with
   * partial transparency.
   *
   * @param aa setting for antialiasing
   */
  void SetAntiAlias(bool aa);

  bool IsAntiAlias() const;

  /**
   * @brief Get the paint's text size.
   *
   * @return the paint's text size.
   */
  float GetTextSize() const { return text_size_; }
  /**
   * Set the paint's text size. This value must be > 0
   *
   * @param textSize the paint's text size.
   */
  void SetTextSize(float textSize) {
    if (textSize <= 0.f) {
      return;
    }
    text_size_ = textSize;
  }

  void SetSDFForSmallText(bool sdf_for_small_text) {
    sdf_for_small_text_ = sdf_for_small_text;
  }

  bool IsSDFForSmallText() const { return sdf_for_small_text_; }

  /**
   * @brief Get the Font Threshold object
   *        If font size is larger than this value, the backend renderer may use
   *        path instead of font-texture to draw text.
   *
   * @return float
   */
  float GetFontThreshold() const { return font_fill_threshold_; }

  /**
   * @brief Set the Font Threshold value
   *        This value controls when to fall back to using path to draw text.
   *
   * @param font_size threshold value
   *                  If paint.GetTextSize() >= paint.GetFontThreshold()
   *                  canvas may will fall back to using path in rendering
   */
  void SetFontThreshold(float font_size) { font_fill_threshold_ = font_size; }

  /**
   * Retrieves alpha from the color used when stroking and filling.
   * @return alpha ranging from zero, fully transparent, to 255, fully opaque
   */
  float GetAlphaF() const;
  /**
   * Replaces alpha, used to fill or stroke this paint. alpha is a value from
   * 0.0 to 1.0.
   * @param a alpha component of color
   */
  void SetAlphaF(float a);
  // Helper that scales the alpha by 255.
  uint8_t GetAlpha() const;
  void SetAlpha(uint8_t alpha);

  void SetBlendMode(BlendMode mode) { blend_mode_ = mode; }

  BlendMode GetBlendMode() const { return blend_mode_; }

  void SetPathEffect(std::shared_ptr<PathEffect> pathEffect) {
    path_effect_ = std::move(pathEffect);
  }

  std::shared_ptr<PathEffect> GetPathEffect() const { return path_effect_; }

  void SetShader(std::shared_ptr<Shader> shader) {
    shader_ = std::move(shader);
  }
  const std::shared_ptr<Shader>& GetShader() const { return shader_; }

  void SetTypeface(std::shared_ptr<Typeface> typeface) {
    typeface_ = std::move(typeface);
  }

  std::shared_ptr<Typeface> GetTypeface() const { return typeface_; }

  void SetColorFilter(std::shared_ptr<ColorFilter> colorFilter) {
    color_filter_ = std::move(colorFilter);
  }

  std::shared_ptr<ColorFilter> GetColorFilter() const { return color_filter_; }

  void SetImageFilter(std::shared_ptr<ImageFilter> imageFilter) {
    image_filter_ = std::move(imageFilter);
  }

  std::shared_ptr<ImageFilter> GetImageFilter() const { return image_filter_; }

  void SetMaskFilter(std::shared_ptr<MaskFilter> maskFilter) {
    mask_filter_ = std::move(maskFilter);
  }

  std::shared_ptr<MaskFilter> GetMaskFilter() const { return mask_filter_; }

  // TODO(zhangzhijian.123) Remove it once Clay no longer uses it.
  bool CanComputeFastBounds() const { return true; }

  Rect ComputeFastBounds(const Rect& origin) const;

  void SetAdjustStroke(bool adjust);

  bool IsAdjustStroke() const;

  bool operator==(const Paint& other) const;
  bool operator!=(const Paint& other) const { return !(*this == other); }

 private:
  Cap cap_ = kDefault_Cap;
  Join join_ = kDefault_Join;
  Style style_ = kFill_Style;
  float stroke_width_ = 1.0f;
  float miter_limit_ = kDefaultMiterLimit;
  float text_size_ = 14.f;
  bool sdf_for_small_text_ = false;
  float font_fill_threshold_ = DEFAULT_FONT_FILL_THRESHOLD;
  Vector fill_color_ = Colors::kBlack;
  Vector stroke_color_ = Colors::kBlack;
  bool is_anti_alias_ = false;
  BlendMode blend_mode_ = BlendMode::kDefault;
  std::shared_ptr<PathEffect> path_effect_;
  std::shared_ptr<Shader> shader_;
  std::shared_ptr<Typeface> typeface_ = nullptr;
  std::shared_ptr<ColorFilter> color_filter_;
  std::shared_ptr<ImageFilter> image_filter_;
  std::shared_ptr<MaskFilter> mask_filter_;
  bool is_adjust_stroke_ = false;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GRAPHIC_PAINT_HPP
