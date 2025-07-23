// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_SW_SW_SPAN_BRUSH_HPP
#define SRC_RENDER_SW_SW_SPAN_BRUSH_HPP

#include <array>
#include <skity/effect/shader.hpp>
#include <skity/geometry/matrix.hpp>
#include <skity/graphic/color.hpp>
#include <skity/graphic/sampling_options.hpp>
#include <skity/graphic/tile_mode.hpp>
#include <vector>

#include "src/graphic/bitmap_sampler.hpp"
#include "src/render/sw/sw_render_target.hpp"
#include "src/render/sw/sw_subpixel.hpp"

namespace skity {

class Bitmap;
class ColorFilter;
enum class BlendMode;

class SWSpanBrush {
 public:
  SWSpanBrush(std::vector<Span> const& spans, Bitmap* bitmap,
              ColorFilter* color_filter, BlendMode blend, float global_alpha)
      : p_spans_(spans.data()),
        spans_size_(spans.size()),
        bitmap_(bitmap),
        color_filter_(color_filter),
        blend_(blend),
        global_alpha_(static_cast<uint8_t>(255 * global_alpha)),
        render_target_(bitmap_) {}

  virtual ~SWSpanBrush() = default;

  void Brush();

 protected:
  // premultiplied color
  virtual Color CalculateColor(int32_t x, int32_t y) = 0;

  virtual bool PureColor() const { return false; }

  const Span* GetSpans() const { return p_spans_; }

  size_t GetSpanSize() const { return spans_size_; }

  Bitmap* CurrentBitmap() const { return bitmap_; }

  BlendMode GetBlendMode() const { return blend_; }

  SWRenderTarget& GetRenderTarget() { return render_target_; }

  virtual void BrushH(int32_t x, int32_t y, int32_t length, int32_t alpha);

  virtual void OnPreBrush() {}
  virtual void OnPostBrush() {}

 private:
  const Span* p_spans_;
  size_t spans_size_;
  Bitmap* bitmap_;
  ColorFilter* color_filter_;
  BlendMode blend_;
  uint8_t global_alpha_;
  SWRenderTarget render_target_;
};

class SolidColorBrush : public SWSpanBrush {
 public:
  SolidColorBrush(std::vector<Span> const& spans, Bitmap* bitmap,
                  ColorFilter* color_filter, BlendMode blend, Color4f color);

  ~SolidColorBrush() override = default;

 protected:
  Color CalculateColor(int32_t x, int32_t y) override;

  bool PureColor() const override { return true; }

 private:
  Color color_;
};

class GradientColorBrush : public SWSpanBrush {
 public:
  static std::unique_ptr<GradientColorBrush> MakeGradientColorBrush(
      std::vector<Span> const& spans, Bitmap* bitmap, ColorFilter* color_filter,
      BlendMode blend, Shader::GradientInfo info, Shader::GradientType type,
      const Matrix& device_to_local);

  GradientColorBrush(std::vector<Span> const& spans, Bitmap* bitmap,
                     ColorFilter* color_filter, BlendMode blend,
                     Shader::GradientInfo info, Shader::GradientType type);

  ~GradientColorBrush() override = default;

 protected:
  Color CalculateColor(int32_t x, int32_t y) override;

  Color4f LerpColor(float current);

  Shader::GradientInfo info_ = {};
  Shader::GradientType type_ = {};
};

class PixmapBrush : public SWSpanBrush {
 public:
  PixmapBrush(std::vector<Span> const& spans, Bitmap* bitmap,
              ColorFilter* color_filter, BlendMode mode, float global_alpha,
              std::shared_ptr<Pixmap> pixmap, const Matrix& points_to_unit,
              FilterMode filter_mode, TileMode x_tile_mode,
              TileMode y_tile_mode);

  ~PixmapBrush() override = default;

 protected:
  Color CalculateColor(int32_t x, int32_t y) override;

  void BrushH(int32_t x, int32_t y, int32_t length, int32_t alpha) override;

 private:
  std::unique_ptr<Bitmap> texture_;
  Matrix points_to_unit_ = {};
  FilterMode filter_mode_ = FilterMode::kNearest;
  TileMode x_tile_mode_ = TileMode::kClamp;
  TileMode y_tile_mode_ = TileMode::kClamp;
  BitmapSampler bitmap_sampler_;
};

}  // namespace skity

#endif  // SRC_RENDER_SW_SW_SPAN_BRUSH_HPP
