// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/sw/sw_span_brush.hpp"

#include <cstring>
#include <skity/effect/color_filter.hpp>
#include <skity/graphic/bitmap.hpp>

#include "src/base/base_macros.hpp"
#include "src/geometry/geometry.hpp"
#include "src/graphic/color_priv.hpp"
#include "src/tracing.hpp"

#ifdef SKITY_ARM_NEON
#include "src/graphic/color_priv_neon.hpp"
#elif defined(SKITY_X86_SSE2)
#include "src/graphic/color_priv_sse2.hpp"
#endif

namespace skity {

namespace {
float RemapFloatTile(float t, TileMode tile_mode) {
  if (tile_mode == TileMode::kClamp) {
    t = std::clamp(t, 0.0f, 1.0f);
  } else if (tile_mode == TileMode::kRepeat) {
    t = glm::fract(t);
  } else if (tile_mode == TileMode::kMirror) {
    float t1 = t - 1;
    float t2 = t1 - 2 * std::floor(t1 * 0.5) - 1;
    t = std::abs(t2);
  }
  return t;
}

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

    BrushH(x, y, len, span.cover);
  }
  OnPostBrush();
}

void SWSpanBrush::BrushH(int32_t x, int32_t y, int32_t length,
                         uint8_t coverage) {
  const PMColor opacity =
      ColorSetARGB(global_alpha_, global_alpha_, global_alpha_, global_alpha_);

  // Sample once at x for a single pixel or when every pixel has the same color.
  if (length == 1 || PureColor()) {
    PMColor color = CalculateColor(x, y);

    if (global_alpha_ != 255) {
      color = PMColorMul(color, opacity);
    }

    if (color_filter_) {
      color = color_filter_->FilterColor(color);
    }
    render_target_.BlendPixelH(x, y, color, length, blend_, coverage);
  } else {
    std::vector<uint32_t> pm_colors(static_cast<size_t>(length));

    for (int32_t l = 0; l < length; l++) {
      PMColor color = CalculateColor(x + l, y);
      if (global_alpha_ != 255) {
        color = PMColorMul(color, opacity);
      }

      if (color_filter_) {
        color = color_filter_->FilterColor(color);
      }

      pm_colors[l] = color;
    }

    render_target_.BlendPixelH(x, y, pm_colors.data(), length, blend_,
                               coverage);
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
    Vec2 start = Vec2{(info.point[0])};
    Vec2 stop = Vec2{info.point[1]};
    Vec2 ss = stop - start;
    float length = ss.Length();
    float scale = length > 0 ? 1.0f / length : 0;
    Vec2 unit_ss = ss * scale;
    float sine = -unit_ss.y;
    float cosine = unit_ss.x;

    Matrix rotate;
    rotate.SetScaleX(cosine);
    rotate.SetSkewX(-sine);
    rotate.SetSkewY(sine);
    rotate.SetScaleY(cosine);

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
  float length = ss.Length();
  float scale = length > 0 ? 1.0f / length : 0;
  Vec2 unit_ss = ss * scale;
  float sine = -unit_ss.y;
  float cosine = unit_ss.x;

  Matrix rotate;
  rotate.SetScaleX(cosine);
  rotate.SetSkewX(-sine);
  rotate.SetSkewY(sine);
  rotate.SetScaleY(cosine);

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
  return {src.x * m.GetScaleX() + src.y * m.GetSkewX() + m.GetTranslateX(),
          src.x * m.GetSkewY() + src.y * m.GetScaleY() + m.GetTranslateY()};
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

  if (stop_count > 0 && current <= info_.color_offsets[0]) {
    return info_.colors[0];
  }

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

  return info_.colors[start_index] * (1 - mix_value) +
         info_.colors[end_index] * mix_value;
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
    Color4f c = LerpColor(MapPoint(src, points_to_unit_).Length());
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
    delta_center_ = (c1_ - c0_).Length();
    delta_radius_ = std::abs(r1_ - r0_);

    if (r0_ < 0 || r1_ < 0) {
      return;
    }

    radial_ = delta_center_ < kNearlyZero;
    strip_ = delta_radius_ < kNearlyZero;

    if (radial_) {
      if (!strip_) {
        scale_ = 1.0 / delta_radius_;
        scale_sign_ = delta_radius_ < 0 ? -1.f : 1.f;
        bias_ = r0_ / delta_radius_;
      }
    } else if (strip_) {
      c0c1_transform_ = PointsToUnit(c0_, c1_);
    } else {
      swap_01_ = r1_ < kNearlyZero;
      if (swap_01_) {
        std::swap(c0_, c1_);
        std::swap(r0_, r1_);
      }
      f_ = r0_ / (r0_ - r1_);
      cf_ = c0_ * (1.f - f_) + c1_ * f_;
      r1_ = r1_ / (c1_ - cf_).Length();
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
      t = pt.Length() * scale_sign_ - bias_;
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
      if (std::abs(r1_ - 1.f) < kNearlyZero) {
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
                         const Matrix& points_to_unit,
                         const SamplingOptions& sampling, TileMode x_tile_mode,
                         TileMode y_tile_mode,
                         const std::shared_ptr<const ColorSpace>& target_space)
    : SWSpanBrush(spans, bitmap, color_filter, blend, global_alpha),
      texture_(new Bitmap(std::move(pixmap))),
      points_to_unit_(points_to_unit),
#if defined(SKITY_ARM_NEON) || defined(SKITY_X86_SSE2)
      use_fast_nearest_(
          !sampling.UseCubic() && sampling.filter == FilterMode::kNearest &&
          !color_filter && texture_->GetPixelAddr() && texture_->Width() &&
          texture_->Height() &&
          // Each integer index must be exactly representable as a float.
          std::max(texture_->Width(), texture_->Height()) <= (1u << 24) &&
          (texture_->GetColorType() == ColorType::kRGBA ||
           texture_->GetColorType() == ColorType::kBGRA) &&
          ColorSpaceEquals(texture_->GetPixmap()->GetColorSpace(),
                           target_space)),
#endif
      x_tile_mode_(x_tile_mode),
      y_tile_mode_(y_tile_mode),
      bitmap_sampler_(*texture_.get(), sampling, x_tile_mode, y_tile_mode,
                      target_space) {
}

Color PixmapBrush::CalculateColor(int32_t x, int32_t y) {
  SKITY_TRACE_EVENT(PixmapBrush_CalculateColor);

  auto uv = MapPoint(Vec2{x + 0.5f, y + 0.5f}, points_to_unit_);
  Color color = bitmap_sampler_.GetColor(uv);
  if (texture_->GetAlphaType() == kUnpremul_AlphaType) {
    color = ColorToPMColor(color);
  }

  return color;
}

#if defined(SKITY_ARM_NEON) || defined(SKITY_X86_SSE2)
namespace {

#ifdef SKITY_ARM_NEON
constexpr int kNearestBatch = 8;

float32x4_t Floor4(float32x4_t value) {
#if defined(__aarch64__)
  return vrndmq_f32(value);
#else
  const auto truncated = vcvtq_f32_s32(vcvtq_s32_f32(value));
  const auto below = vcltq_f32(value, truncated);
  const auto floor = vsubq_f32(
      truncated, vreinterpretq_f32_u32(vandq_u32(
                     below, vreinterpretq_u32_f32(vdupq_n_f32(1.f)))));
  // Floats of magnitude 2^23 or greater are already integers.
  return vbslq_f32(vcltq_f32(vabsq_f32(value), vdupq_n_f32(0x1p23f)), floor,
                   value);
#endif
}

void NearestIndices(int x, float y, float scale, float skew, float translate,
                    TileMode tile, uint32_t size, uint32_t indices[4]) {
  const int32x4_t lanes = {0, 1, 2, 3};
  const auto px = vaddq_f32(vcvtq_f32_s32(vaddq_s32(vdupq_n_s32(x), lanes)),
                            vdupq_n_f32(0.5f));
  auto t = vaddq_f32(vmulq_n_f32(px, scale), vdupq_n_f32(y * skew));
  t = vaddq_f32(t, vdupq_n_f32(translate));
  if (tile == TileMode::kRepeat) {
    t = vsubq_f32(t, Floor4(t));
  } else if (tile == TileMode::kMirror) {
    // Mirror is symmetric. A positive offset avoids rounding at negative folds.
    const auto offset = vabsq_f32(vsubq_f32(t, vdupq_n_f32(1.f)));
    const auto period = Floor4(vmulq_n_f32(offset, 0.5f));
    t = vabsq_f32(vsubq_f32(vsubq_f32(offset, vmulq_n_f32(period, 2.f)),
                            vdupq_n_f32(1.f)));
  }
  const auto scaled = vmaxq_f32(vmulq_n_f32(t, size), vdupq_n_f32(0.f));
  auto index = vcvtq_u32_f32(vminq_f32(scaled, vdupq_n_f32(size - 1)));
  if (tile == TileMode::kDecal) {
    index = vorrq_u32(index, vorrq_u32(vcltq_f32(t, vdupq_n_f32(0.f)),
                                       vcgeq_f32(t, vdupq_n_f32(1.f))));
  }
  vst1q_u32(indices, index);
}

void PrepareNearestColors(PMColor* colors, const Bitmap& texture,
                          uint8_t alpha) {
  auto* bytes = reinterpret_cast<uint8_t*>(colors);
  auto src = vld4_u8(bytes);
  if (texture.GetAlphaType() == kUnpremul_AlphaType) {
    for (int c = 0; c < 3; ++c) {
      src.val[c] = MulDiv255RoundNeon(src.val[c], src.val[3]);
    }
  }
  if (alpha != 255) {
    for (int c = 0; c < 4; ++c) {
      src.val[c] = MulDiv255RoundNeon(src.val[c], vdup_n_u8(alpha));
    }
  }
  if (texture.GetColorType() == ColorType::kRGBA) {
    std::swap(src.val[0], src.val[2]);
  }
  vst4_u8(bytes, src);
}
#else
constexpr int kNearestBatch = 4;

__m128 Floor4(__m128 value) {
  const auto truncated = _mm_cvtepi32_ps(_mm_cvttps_epi32(value));
  const auto below = _mm_cmplt_ps(value, truncated);
  const auto floor = _mm_sub_ps(truncated, _mm_and_ps(below, _mm_set1_ps(1.f)));
  // Bypass integer conversion for large floats, which have no fractional bits.
  const auto small = _mm_cmplt_ps(_mm_andnot_ps(_mm_set1_ps(-0.f), value),
                                  _mm_set1_ps(0x1p23f));
  return _mm_or_ps(_mm_and_ps(small, floor), _mm_andnot_ps(small, value));
}

void NearestIndices(int x, float y, float scale, float skew, float translate,
                    TileMode tile, uint32_t size, uint32_t indices[4]) {
  const auto positions =
      _mm_add_epi32(_mm_set1_epi32(x), _mm_setr_epi32(0, 1, 2, 3));
  const auto px = _mm_add_ps(_mm_cvtepi32_ps(positions), _mm_set1_ps(0.5f));
  auto t =
      _mm_add_ps(_mm_mul_ps(px, _mm_set1_ps(scale)), _mm_set1_ps(y * skew));
  t = _mm_add_ps(t, _mm_set1_ps(translate));
  if (tile == TileMode::kRepeat) {
    t = _mm_sub_ps(t, Floor4(t));
  } else if (tile == TileMode::kMirror) {
    const auto offset =
        _mm_andnot_ps(_mm_set1_ps(-0.f), _mm_sub_ps(t, _mm_set1_ps(1.f)));
    const auto period = Floor4(_mm_mul_ps(offset, _mm_set1_ps(0.5f)));
    t = _mm_sub_ps(_mm_sub_ps(offset, _mm_mul_ps(period, _mm_set1_ps(2.f))),
                   _mm_set1_ps(1.f));
    t = _mm_andnot_ps(_mm_set1_ps(-0.f), t);
  }
  const auto scaled =
      _mm_max_ps(_mm_mul_ps(t, _mm_set1_ps(size)), _mm_setzero_ps());
  auto index = _mm_cvttps_epi32(_mm_min_ps(scaled, _mm_set1_ps(size - 1)));
  if (tile == TileMode::kDecal) {
    const auto outside = _mm_or_ps(_mm_cmplt_ps(t, _mm_setzero_ps()),
                                   _mm_cmpge_ps(t, _mm_set1_ps(1.f)));
    index = _mm_or_si128(index, _mm_castps_si128(outside));
  }
  _mm_storeu_si128(reinterpret_cast<__m128i*>(indices), index);
}

void PrepareNearestColors(PMColor* colors, const Bitmap& texture,
                          uint8_t alpha) {
  auto* pixels = reinterpret_cast<__m128i*>(colors);
  auto src = _mm_loadu_si128(pixels);
  if (texture.GetAlphaType() == kUnpremul_AlphaType) {
    // Multiply RGB by the pixel alpha and leave alpha itself intact.
    const auto weights =
        _mm_or_si128(sse2::Alpha(src), _mm_set1_epi32(0xff000000));
    src = sse2::MulDiv255Round(src, weights);
  }
  if (alpha != 255) {
    src = sse2::MulDiv255Round(src, _mm_set1_epi8(alpha));
  }
  if (texture.GetColorType() == ColorType::kRGBA) {
    src = sse2::SwapRB(src);
  }
  _mm_storeu_si128(pixels, src);
}
#endif

void SampleNearestFour(int x, int y, const Bitmap& texture, const Matrix& m,
                       TileMode tx, TileMode ty, PMColor* colors) {
  uint32_t xs[4], ys[4];
  NearestIndices(x, y + 0.5f, m.GetScaleX(), m.GetSkewX(), m.GetTranslateX(),
                 tx, texture.Width(), xs);
  NearestIndices(x, y + 0.5f, m.GetSkewY(), m.GetScaleY(), m.GetTranslateY(),
                 ty, texture.Height(), ys);
  const size_t stride = texture.GetPixmap()->RowBytes();
  const auto* pixels = texture.GetPixelAddr();
  for (int i = 0; i < 4; ++i) {
    if (xs[i] == UINT32_MAX || ys[i] == UINT32_MAX) {
      colors[i] = Color_TRANSPARENT;
    } else {
      // A caller-supplied row can start at an unaligned address.
      std::memcpy(colors + i, pixels + ys[i] * stride + xs[i] * sizeof(PMColor),
                  sizeof(PMColor));
    }
  }
}
}  // namespace
#endif

void PixmapBrush::BrushH(int32_t x, int32_t y, int32_t length,
                         uint8_t coverage) {
  SKITY_TRACE_EVENT(PixmapBrush_BrushH);

#if defined(SKITY_ARM_NEON) || defined(SKITY_X86_SSE2)
  if (use_fast_nearest_) {
    PMColor colors[kNearestBatch];
    while (length >= kNearestBatch) {
      for (int i = 0; i < kNearestBatch; i += 4) {
        SampleNearestFour(x + i, y, *texture_, points_to_unit_, x_tile_mode_,
                          y_tile_mode_, colors + i);
      }
      PrepareNearestColors(colors, *texture_, GetGlobalAlpha());
      GetRenderTarget().BlendPixelH(x, y, colors, kNearestBatch, GetBlendMode(),
                                    coverage);
      x += kNearestBatch;
      length -= kNearestBatch;
    }
  }
#endif
  if (length > 0) {
    SWSpanBrush::BrushH(x, y, length, coverage);
  }
}

}  // namespace skity
