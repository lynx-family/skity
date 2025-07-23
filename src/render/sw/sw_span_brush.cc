// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/sw/sw_span_brush.hpp"

#include <skity/effect/color_filter.hpp>
#include <skity/graphic/bitmap.hpp>

#include "src/geometry/geometry.hpp"
#include "src/graphic/color_priv.hpp"
#include "src/tracing.hpp"

#ifdef SKITY_ARM_NEON
#include "src/graphic/color_priv_neon.hpp"
#endif

namespace skity {

namespace {
float RemapFloatTile(float t, TileMode tile_mode) {
  if (tile_mode == TileMode::kClamp) {
    t = glm::clamp(t, 0.0f, 1.0f);
  } else if (tile_mode == TileMode::kRepeat) {
    t = glm::fract(t);
  } else if (tile_mode == TileMode::kMirror) {
    float t1 = t - 1;
    float t2 = t1 - 2 * glm::floor(t1 * 0.5) - 1;
    t = glm::abs(t2);
  }
  return t;
}

#ifdef SKITY_ARM_NEON
inline float32x4_t Floor(float32x4_t input) {
  int32x4_t truncated = vcvtq_s32_f32(input);
  uint32x4_t negative_mask = vcltq_f32(input, vdupq_n_f32(0.0));
  int32x4_t adjustment = vreinterpretq_s32_u32(negative_mask);
  int32x4_t floored = vaddq_s32(truncated, adjustment);
  return vcvtq_f32_s32(floored);
}

float32x4_t RemapFloatTileNeon(float32x4_t t, TileMode tile_mode) {
  if (tile_mode == TileMode::kClamp) {
    t = vmaxq_f32(t, vdupq_n_f32(0.0f));
    t = vminq_f32(t, vdupq_n_f32(1.0f));
  } else if (tile_mode == TileMode::kRepeat) {
    t = vsubq_f32(t, Floor(t));
  } else if (tile_mode == TileMode::kMirror) {
    float32x4_t t1 = vsubq_f32(t, vdupq_n_f32(1.0f));
    float32x4_t half_t1 = vmulq_f32(t1, vdupq_n_f32(0.5f));
    float32x4_t floor_half_t1 = Floor(half_t1);
    float32x4_t double_floor_half_t1 =
        vmulq_f32(vdupq_n_f32(2.0f), floor_half_t1);
    float32x4_t t2 = vsubq_f32(t1, double_floor_half_t1);
    t2 = vsubq_f32(t2, vdupq_n_f32(1.0f));
    t = vabsq_f32(t2);
  }
  return t;
}

#endif

}  // namespace

void SWSpanBrush::Brush() {
  SKITY_TRACE_EVENT(SWSpanBrush_Brush);

  auto i_width = static_cast<int32_t>(bitmap_->Width());
  auto i_height = static_cast<int32_t>(bitmap_->Height());

  OnPreBrush();
  for (size_t i = 0; i < spans_size_; i++) {
    Span const& span = p_spans_[i];

    auto x = span.x;
    auto y = span.y;
    auto len = span.len;

    if (y < 0 || y >= i_height) {
      continue;
    }

    if (x >= i_width || x + len < 0) {
      continue;
    }

    if (x < 0) {
      len += x;
      x = 0;
    }

    if (x + len >= i_width) {
      len = i_width - x;
    }

    if (len <= 0) {
      continue;
    }

    auto u_alpha = static_cast<uint8_t>(span.cover & global_alpha_);

    BrushH(x, y, len, u_alpha);
  }
  OnPostBrush();
}

void SWSpanBrush::BrushH(int32_t x, int32_t y, int32_t length, int32_t alpha) {
  if (length == 1 || PureColor()) {
    PMColor color = CalculateColor(x, y);

    if (alpha != 255) {
      color = AlphaMulQ(color, alpha);
    }

    if (color_filter_) {
      color = color_filter_->FilterColor(color);
    }
    render_target_.BlendPixelH(x, y, color, length, blend_);
  } else {
    std::vector<uint32_t> pm_colors(static_cast<size_t>(length));

    for (int32_t l = 0; l < length; l++) {
      PMColor color = CalculateColor(x + l, y);
      if (alpha != 255) {
        color = AlphaMulQ(color, alpha);
      }

      if (color_filter_) {
        color = color_filter_->FilterColor(color);
      }

      pm_colors[l] = color;
    }

    render_target_.BlendPixelH(x, y, pm_colors.data(), length, blend_);
  }
}

SolidColorBrush::SolidColorBrush(std::vector<Span> const& spans, Bitmap* bitmap,
                                 ColorFilter* color_filter, BlendMode blend,
                                 Color4f color)
    : SWSpanBrush(spans, bitmap, color_filter, blend, 1.f),
      color_(Color4fToColor(color)) {
  color_ = ColorToPMColor(color_);
}

Color SolidColorBrush::CalculateColor(int32_t, int32_t) {
  SKITY_TRACE_EVENT(SolidColorBrush_CalculateColor);

  return color_;
}

Color GradientColorBrush::CalculateColor(int32_t x, int32_t y) {
  // for unsupport gradient type
  Color4f c = Color4f{};
  Color color = Color4fToColor(c);
  color = ColorToPMColor(color);
  return color;
}

namespace {

Matrix PointsToUnit(const Shader::GradientInfo& info,
                    Shader::GradientType type) {
  if (type == Shader::GradientType::kLinear) {
    Vec2 start = FromPoint(info.point[0]);
    Vec2 stop = info.point[1];
    Vec2 ss = stop - start;
    float length = glm::length(ss);
    float scale = length > 0 ? 1.0f / length : 0;
    Vec2 unit_ss = ss * scale;
    float sine = -unit_ss.y;
    float cosine = unit_ss.x;

    Matrix rotate;
    rotate.Set(Matrix::kMScaleX, cosine);
    rotate.Set(Matrix::kMSkewX, -sine);
    rotate.Set(Matrix::kMSkewY, sine);
    rotate.Set(Matrix::kMScaleY, cosine);

    return rotate *                       //
           Matrix::Scale(scale, scale) *  //
           Matrix::Translate(-start.x, -start.y);

  } else if (type == Shader::GradientType::kRadial) {
    float radius = info.radius[0];
    Vec2 center = FromPoint(info.point[0]);
    float scale = radius > 0 ? 1.0f / radius : 0;
    return Matrix::Scale(scale, scale) *
           Matrix::Translate(-center.x, -center.y);
  } else if (type == Shader::GradientType::kSweep) {
    Vec2 center = FromPoint(info.point[0]);
    return Matrix::Translate(-center.x, -center.y);
  }

  return Matrix{};
}

Matrix PointsToUnit(const Point& p0, const Point& p1) {
  Vec2 start = FromPoint(p0);
  Vec2 stop = FromPoint(p1);
  Vec2 ss = stop - start;
  float length = glm::length(ss);
  float scale = length > 0 ? 1.0f / length : 0;
  Vec2 unit_ss = ss * scale;
  float sine = -unit_ss.y;
  float cosine = unit_ss.x;

  Matrix rotate;
  rotate.Set(Matrix::kMScaleX, cosine);
  rotate.Set(Matrix::kMSkewX, -sine);
  rotate.Set(Matrix::kMSkewY, sine);
  rotate.Set(Matrix::kMScaleY, cosine);

  return rotate *                       //
         Matrix::Scale(scale, scale) *  //
         Matrix::Translate(-start.x, -start.y);
}

}  // namespace

GradientColorBrush::GradientColorBrush(
    std::vector<Span> const& spans, Bitmap* bitmap, ColorFilter* color_filter,
    BlendMode blend, Shader::GradientInfo info, Shader::GradientType type)
    : SWSpanBrush(spans, bitmap, color_filter, blend, 1.f),
      info_(std::move(info)),
      type_(type) {}

namespace {

Vec2 inline MapPoint(const Vec2& src, const Matrix& m) {
  return {src.x * m.Get(Matrix::kMScaleX) + src.y * m.Get(Matrix::kMSkewX) +
              m.Get(Matrix::kMTransX),
          src.x * m.Get(Matrix::kMSkewY) + src.y * m.Get(Matrix::kMScaleY) +
              m.Get(Matrix::kMTransY)};
}

}  // namespace

Color4f GradientColorBrush::LerpColor(float current) {
  if (FloatNearlyZero(current)) {
    current = 0.0f;
  } else if (FloatNearlyZero(current - 1.0f)) {
    current = 1.0f;
  }

  if ((info_.tile_mode == TileMode::kDecal &&
       (current < 0.0 || current >= 1.0))) {
    return Colors::kTransparent;
  }

  current = RemapFloatTile(current, info_.tile_mode);

  int32_t color_count = info_.colors.size();
  int32_t stop_count = info_.color_offsets.size();

  int32_t start_index = 0;
  int32_t end_index = 1;

  float step = 1.f / (color_count - 1);

  int32_t i = 0;
  float start = 0.f;
  float end = 0.f;

  for (i = 0; i < color_count - 1; i++) {
    if (stop_count > 0) {
      start = info_.color_offsets[i];
      end = info_.color_offsets[i + 1];
    } else {
      start = step * i;
      end = step * (i + 1);
    }

    if (current >= start && current <= end) {
      start_index = i;
      end_index = i + 1;
      break;
    }
  }

  if (i == color_count - 1 && color_count > 0) {
    return info_.colors[color_count - 1];
  }

  float total = end - start;
  float value = current - start;

  float mix_value = 0.5f;
  if (total > 0) {
    mix_value = value / total;
  }

  return glm::mix(info_.colors[start_index], info_.colors[end_index],
                  mix_value);
}

class LinearGradientColorBrush : public GradientColorBrush {
 public:
  LinearGradientColorBrush(std::vector<Span> const& spans, Bitmap* bitmap,
                           ColorFilter* color_filter, BlendMode blend,
                           Shader::GradientInfo info, Shader::GradientType type,
                           const Matrix& device_to_local)
      : GradientColorBrush(spans, bitmap, color_filter, blend, info, type),
        points_to_unit_(PointsToUnit(info_, type_) * device_to_local) {}
  ~LinearGradientColorBrush() override = default;

 protected:
  Color CalculateColor(int32_t x, int32_t y) override {
    SKITY_TRACE_EVENT(LinearGradientColorBrush_CalculateColor);

    Vec2 src{x + 0.5f, y + 0.5f};
    Color4f c = LerpColor(MapPoint(src, points_to_unit_).x);
    Color color = Color4fToColor(c);
    color = ColorToPMColor(color);
    return color;
  }

 private:
  Matrix points_to_unit_ = {};
};

class SweepGradientColorBrush : public GradientColorBrush {
 public:
  SweepGradientColorBrush(std::vector<Span> const& spans, Bitmap* bitmap,
                          ColorFilter* color_filter, BlendMode blend,
                          Shader::GradientInfo info, Shader::GradientType type,
                          const Matrix& device_to_local)
      : GradientColorBrush(spans, bitmap, color_filter, blend, info, type),
        points_to_unit_(PointsToUnit(info_, type_) * device_to_local) {}
  ~SweepGradientColorBrush() override = default;

 protected:
  Color CalculateColor(int32_t x, int32_t y) override {
    SKITY_TRACE_EVENT(SweepGradientColorBrush_CalculateColor);

    Vec2 src{x + 0.5f, y + 0.5f};
    auto mapped = MapPoint(src, points_to_unit_);

    float angle = std::atan2(-mapped.y, -mapped.x);

    auto bias = info_.radius[0];
    auto scale = info_.radius[1];

    constexpr static float k1Over2Pi = 0.1591549430918;
    float t = (angle * k1Over2Pi + 0.5 + bias) * scale;
    Color4f c = LerpColor(t);
    Color color = Color4fToColor(c);
    color = ColorToPMColor(color);
    return color;
  }

 private:
  Matrix points_to_unit_ = {};
};

class RadialGradientColorBrush : public GradientColorBrush {
 public:
  RadialGradientColorBrush(std::vector<Span> const& spans, Bitmap* bitmap,
                           ColorFilter* color_filter, BlendMode blend,
                           Shader::GradientInfo info, Shader::GradientType type,
                           const Matrix& device_to_local)
      : GradientColorBrush(spans, bitmap, color_filter, blend, info, type),
        points_to_unit_(PointsToUnit(info_, type_) * device_to_local) {}
  ~RadialGradientColorBrush() override = default;

 protected:
  Color CalculateColor(int32_t x, int32_t y) override {
    SKITY_TRACE_EVENT(RadialGradientColorBrush_CalculateColor);

    Vec2 src{x + 0.5f, y + 0.5f};
    Color4f c = LerpColor(glm::length(MapPoint(src, points_to_unit_)));
    Color color = Color4fToColor(c);
    color = ColorToPMColor(color);
    return color;
  }

 private:
  Matrix points_to_unit_ = {};
};

class ConicalGradientColorBrush : public GradientColorBrush {
 public:
  ConicalGradientColorBrush(std::vector<Span> const& spans, Bitmap* bitmap,
                            ColorFilter* color_filter, BlendMode blend,
                            Shader::GradientInfo info,
                            Shader::GradientType type,
                            const Matrix& device_to_local)
      : GradientColorBrush(spans, bitmap, color_filter, blend, info, type),
        device_to_local_(device_to_local),
        points_to_unit_(PointsToUnit(info_, type_)) {}
  ~ConicalGradientColorBrush() override = default;

 protected:
  Color CalculateColor(int32_t x, int32_t y) override {
    SKITY_TRACE_EVENT(ConicalGradientColorBrush_CalculateColor);

    Color4f c = CalculateConical(x, y);
    Color color = Color4fToColor(c);
    color = ColorToPMColor(color);
    return color;
  }

  void OnPreBrush() override {
    SKITY_TRACE_EVENT(ConicalGradientColorBrush_OnPreBrush);

    c0_ = info_.point[0];
    c1_ = info_.point[1];
    r0_ = info_.radius[0];
    r1_ = info_.radius[1];
    delta_center_ = glm::distance(c1_, c0_);
    delta_radius_ = std::abs(r1_ - r0_);

    if (r0_ < 0 || r1_ < 0) {
      return;
    }

    radial_ = delta_center_ < NearlyZero;
    strip_ = delta_radius_ < NearlyZero;

    if (radial_) {
      if (!strip_) {
        scale_ = 1.0 / delta_radius_;
        scale_sign_ = delta_radius_ < 0 ? -1.f : 1.f;
        bias_ = r0_ / delta_radius_;
      }
    } else if (strip_) {
      c0c1_transform_ = PointsToUnit(c0_, c1_);
    } else {
      swap_01_ = r1_ < NearlyZero;
      if (swap_01_) {
        std::swap(c0_, c1_);
        std::swap(r0_, r1_);
      }
      f_ = r0_ / (r0_ - r1_);
      cf_ = c0_ * (1.f - f_) + c1_ * f_;
      r1_ = r1_ / glm::length(c1_ - cf_);
      r1_square_ = r1_ * r1_;

      cfc1_tranform_ = PointsToUnit(cf_, c1_);
    }
  }
  void OnPostBrush() override {}

 private:
  Color4f CalculateConical(int32_t x, int32_t y) {
    if (r0_ < 0 || r1_ < 0) {
      return Colors::kTransparent;
    }

    float t = 0;
    Vec2 p{x + 0.5f, y + 0.5f};
    p = MapPoint(p, device_to_local_);
    if (radial_) {
      // degenerate case 1: codes from shader
      if (strip_) {
        return Colors::kTransparent;
      }
      Vec2 pt = (p - FromPoint(c0_)) * scale_;
      t = length(pt) * scale_sign_ - bias_;
    } else if (strip_) {
      // degenerate case 2: codes from shader
      float r = r0_ / delta_center_;
      float r_2 = r * r;

      p = MapPoint(p, c0c1_transform_);
      t = r_2 - p.y * p.y;
      if (t < 0.0) {
        return Colors::kTransparent;
      }
      t = p.x + sqrt(t);
    } else {
      // general case: codes from https://skia.org/docs/dev/design/conical/
      p = MapPoint(p, cfc1_tranform_);

      float xt = -1.f;
      if (std::abs(r1_ - 1.f) < NearlyZero) {
        xt = (p.x * p.x + p.y * p.y) / 2;
      } else if (r1_ > 1.f) {
        float m = r1_square_ - 1.f;
        float delta = m * p.y * p.y + r1_square_ * p.x * p.x;
        xt = (std::sqrt(delta) - p.x) / m;
      } else {
        float m = r1_square_ - 1.f;
        float delta = m * p.y * p.y + r1_square_ * p.x * p.x;
        if (delta > 0) {
          float xt1 = (std::sqrt(delta) - p.x) / m;
          float xt2 = (-std::sqrt(delta) - p.x) / m;
          xt = 1.f - f_ < 0 ? std::min(xt1, xt2) : std::max(xt1, xt2);
        }
      }

      if (xt < 0) {
        return Colors::kTransparent;
      }

      t = f_ + (1.f - f_) * xt;
      if (swap_01_) {
        t = 1.0 - t;
      }
    }

    return LerpColor(t);
  }

 private:
  Matrix device_to_local_ = {};
  Matrix points_to_unit_ = {};
  // All filed names are from https://skia.org/docs/dev/design/conical/
  Point c0_ = {};
  Point c1_ = {};
  float r0_ = {};
  float r1_ = {};
  float r1_square_ = {};
  float delta_center_ = {};
  float delta_radius_ = {};
  bool radial_ = {};
  bool strip_ = {};
  float scale_ = {};
  float scale_sign_ = {};
  float bias_ = {};
  Matrix c0c1_transform_ = {};
  bool swap_01_ = {};
  float f_ = {};
  Point cf_ = {};
  Matrix cfc1_tranform_ = {};
};

std::unique_ptr<GradientColorBrush> GradientColorBrush::MakeGradientColorBrush(
    std::vector<Span> const& spans, Bitmap* bitmap, ColorFilter* color_filter,
    BlendMode blend, Shader::GradientInfo info, Shader::GradientType type,
    const Matrix& device_to_local) {
  switch (type) {
    case Shader::GradientType::kLinear:
      return std::make_unique<LinearGradientColorBrush>(
          spans, bitmap, color_filter, blend, info, type, device_to_local);
    case Shader::GradientType::kRadial:
      return std::make_unique<RadialGradientColorBrush>(
          spans, bitmap, color_filter, blend, info, type, device_to_local);
    case Shader::GradientType::kConical:
      return std::make_unique<ConicalGradientColorBrush>(
          spans, bitmap, color_filter, blend, info, type, device_to_local);
    case Shader::GradientType::kSweep:
      return std::make_unique<SweepGradientColorBrush>(
          spans, bitmap, color_filter, blend, info, type, device_to_local);
    default:
      return std::make_unique<GradientColorBrush>(spans, bitmap, color_filter,
                                                  blend, info, type);
  }
}

PixmapBrush::PixmapBrush(std::vector<Span> const& spans, Bitmap* bitmap,
                         ColorFilter* color_filter, BlendMode blend,
                         float global_alpha, std::shared_ptr<Pixmap> pixmap,
                         const Matrix& points_to_unit, FilterMode filter_mode,
                         TileMode x_tile_mode, TileMode y_tile_mode)
    : SWSpanBrush(spans, bitmap, color_filter, blend, global_alpha),
      texture_(new Bitmap(std::move(pixmap))),
      points_to_unit_(points_to_unit),
      filter_mode_(filter_mode),
      x_tile_mode_(x_tile_mode),
      y_tile_mode_(y_tile_mode),
      bitmap_sampler_(*texture_.get(),
                      SamplingOptions{filter_mode, MipmapMode::kNone},
                      x_tile_mode, y_tile_mode) {}

Color PixmapBrush::CalculateColor(int32_t x, int32_t y) {
  SKITY_TRACE_EVENT(PixmapBrush_CalculateColor);

  auto uv = MapPoint(Vec2{x + 0.5, y + 0.5}, points_to_unit_);
  Color color = bitmap_sampler_.GetColor(uv);
  if (texture_->GetAlphaType() == kUnpremul_AlphaType) {
    color = ColorToPMColor(color);
  }

  return color;
}

#ifdef SKITY_ARM_NEON
namespace {
void CalculateImageColorsNeon(int32_t p_x, int32_t p_y, int32_t p_alpha,
                              uint32_t p_width, uint32_t p_height,
                              uint32_t p_row_bytes, uint8_t* p_pixel,
                              const Matrix& p_points_to_unit,
                              TileMode x_tile_mode, TileMode y_tile_mode,
                              uint32_t* p_data) {
  float32x4_t offset = {0.5f, 1.5f, 2.5f, 3.5f};
  float32x4_t x = vaddq_f32(vdupq_n_f32(p_x), offset);
  float32x4_t y = vdupq_n_f32(p_y + 0.5f);
  // u = scaleX * x + skewX * y + transX
  float32x4_t u =
      vmulq_n_f32(x, p_points_to_unit.Get(Matrix::kMScaleX));    // scaleX * x
  u = vmlaq_n_f32(u, y, p_points_to_unit.Get(Matrix::kMSkewX));  // + skewX * y
  u = vaddq_f32(
      u,
      vdupq_n_f32(p_points_to_unit.Get(Matrix::kMTransX)));  // + transX
  u = RemapFloatTileNeon(u, x_tile_mode);

  // v = skewY * x + scaleY * y + transY
  float32x4_t v =
      vmulq_n_f32(x, p_points_to_unit.Get(Matrix::kMSkewY));  // skewY * x
  v = vmlaq_n_f32(v, y,
                  p_points_to_unit.Get(Matrix::kMScaleY));  // + scaleY * y
  v = vaddq_f32(
      v,
      vdupq_n_f32(p_points_to_unit.Get(Matrix::kMTransY)));  // + transY
  v = RemapFloatTileNeon(v, y_tile_mode);

  float32x4_t width = vdupq_n_f32(p_width);
  float32x4_t height = vdupq_n_f32(p_height);
  float32x4_t x_indices = vmulq_f32(u, width);   // u * width
  float32x4_t y_indices = vmulq_f32(v, height);  // v * height
  // clamp to [0, width - 1]
  x_indices = vmaxq_f32(x_indices, vdupq_n_f32(0.0f));
  x_indices = vminq_f32(x_indices, vdupq_n_f32(p_width - 1));
  // clamp to [0, height - 1]
  y_indices = vmaxq_f32(y_indices, vdupq_n_f32(0.0f));
  y_indices = vminq_f32(y_indices, vdupq_n_f32(p_height - 1));

  // float -> uint
  uint32x4_t ix = vcvtq_u32_f32(x_indices);
  uint32x4_t iy = vcvtq_u32_f32(y_indices);

  // 2D indices -> 1D indices
  uint32x4_t row_bytes = vdupq_n_u32(p_row_bytes);
  uint32x4_t indices = vmlaq_u32(vmulq_u32(ix, vdupq_n_u32(4)), row_bytes,
                                 iy);  // index = iy * stride + ix * 4

  uint32_t indices_array[4];
  // store indices
  vst1q_u32(indices_array, indices);

  float32_t u_array[4];
  if (x_tile_mode == TileMode::kDecal) {
    vst1q_f32(u_array, u);
  }
  float32_t v_array[4];
  if (y_tile_mode == TileMode::kDecal) {
    vst1q_f32(v_array, v);
  }

  for (int i = 0; i < 4; i++) {
    uint32_t idx = indices_array[i];
    if (x_tile_mode == TileMode::kDecal &&
        (u_array[i] < 0.0f || u_array[i] >= 1.0f)) {
      *(p_data + i) = Color_TRANSPARENT;
    } else if (y_tile_mode == TileMode::kDecal &&
               (v_array[i] < 0.0f || v_array[i] >= 1.0f)) {
      *(p_data + i) = Color_TRANSPARENT;
    } else {
      // sample pixel
      *(p_data + i) = *reinterpret_cast<uint32_t*>(p_pixel + idx);
    }
  }
}

}  // namespace

#endif

void PixmapBrush::BrushH(int32_t x, int32_t y, int32_t length, int32_t alpha) {
  SKITY_TRACE_EVENT(PixmapBrush_BrushH);

#ifdef SKITY_ARM_NEON
  if (filter_mode_ == FilterMode::kLinear) {
    // TODO(zhangzhijian): Accelerate it via neon
    SWSpanBrush::BrushH(x, y, length, alpha);
    return;
  }

  const int32_t N = 8;
  std::vector<PMColor> colors(static_cast<size_t>(N));

  uint32_t iterations = length / N;
  int32_t neon_filled = iterations * N;

  for (uint32_t i = 0; i < iterations; i++) {
    int32_t l = i * N;
    for (int j = 0; j < 2; j++) {
      CalculateImageColorsNeon(
          x + l + j * 4, y, alpha, texture_->Width(), texture_->Height(),
          texture_->RowBytes(), texture_->GetPixelAddr(), points_to_unit_,
          x_tile_mode_, y_tile_mode_, colors.data() + j * 4);
    }

    uint8x8x4_t src = vld4_u8(reinterpret_cast<const uint8_t*>(colors.data()));
    if (texture_->GetAlphaType() == AlphaType::kUnpremul_AlphaType) {
      src.val[0] = MulDiv255RoundNeon(src.val[3], src.val[0]);  // src.a * src.r
      src.val[1] = MulDiv255RoundNeon(src.val[3], src.val[1]);  // src.a * src.g
      src.val[2] = MulDiv255RoundNeon(src.val[3], src.val[2]);  // src.a * src.b
    }

    if (alpha != 255) {
      uint8x8_t a = vdup_n_u8(alpha);
      src.val[0] = MulDiv255RoundNeon(a, src.val[0]);  // a * src.r
      src.val[1] = MulDiv255RoundNeon(a, src.val[1]);  // a * src.g
      src.val[2] = MulDiv255RoundNeon(a, src.val[2]);  // a * src.b
      src.val[3] = MulDiv255RoundNeon(a, src.val[3]);  // a * src.a
    }
    if (texture_->GetColorType() == ColorType::kRGBA) {
      std::swap(src.val[0], src.val[2]);  // RGBA -> BGRA
    }
    vst4_u8(reinterpret_cast<uint8_t*>(colors.data()), src);

    GetRenderTarget().BlendPixelH(x + l, y, colors.data(), N, GetBlendMode());
  }

  if (neon_filled < length) {
    SWSpanBrush::BrushH(x + neon_filled, y, length - neon_filled, alpha);
  }
#else
  SWSpanBrush::BrushH(x, y, length, alpha);
#endif
}

}  // namespace skity
