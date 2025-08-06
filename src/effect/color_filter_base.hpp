// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_EFFECT_COLOR_FILTER_BASE_HPP
#define SRC_EFFECT_COLOR_FILTER_BASE_HPP

#include <cstring>
#include <memory>
#include <skity/effect/color_filter.hpp>
#include <skity/geometry/matrix.hpp>
#include <skity/geometry/point.hpp>
#include <skity/graphic/blend_mode.hpp>
#include <skity/graphic/color_type.hpp>
#include <tuple>
#include <vector>

namespace skity {

enum class ColorFilterType : int32_t {
  kBlend = 0,
  kMatrix = 1,
  kLinearToSRGBGamma = 2,
  kSRGBToLinearGamma = 3,
  kCompose = 4,
  kMaxType = kCompose,
};

class ColorFilterBase : public ColorFilter {
 public:
#ifdef SKITY_CPU
  virtual PMColor OnFilterColor(PMColor c) const { return c; }
#endif

  virtual ~ColorFilterBase() = default;

  virtual ColorFilterType GetType() const = 0;

 protected:
  ColorFilterBase() = default;
};

static inline ColorFilterBase* As_CFB(ColorFilter* filter) {
  return static_cast<ColorFilterBase*>(filter);
}

static inline const ColorFilterBase* As_CFB(const ColorFilter* filter) {
  return static_cast<const ColorFilterBase*>(filter);
}

bool operator==(const ColorFilterBase& a, const ColorFilterBase& b);

class BlendColorFilter : public ColorFilterBase {
 public:
#ifdef SKITY_CPU
  PMColor OnFilterColor(PMColor c) const override;
#endif
  BlendColorFilter(Color c, BlendMode m);

  Color GetColor() const { return color_; }
  BlendMode GetBlendMode() const { return mode_; }

  ColorFilterType GetType() const override { return ColorFilterType::kBlend; }

 private:
  Color color_;
  Color pm_color_;
  BlendMode mode_;
};

class MatrixColorFilter : public ColorFilterBase {
 public:
#ifdef SKITY_CPU
  PMColor OnFilterColor(PMColor c) const override;
#endif

  explicit MatrixColorFilter(const float row_major[20]) {
    memcpy(matrix_, row_major, 20 * sizeof(float));

#ifdef SKITY_CPU
    auto p = reinterpret_cast<int16_t*>(matrix_i16_);
    for (int i = 0; i < 20; i++) {
      p[i] = (int16_t)(row_major[i] * 255);
    }
#endif
  }
  explicit MatrixColorFilter(const int16_t row_major[20]) {}

  std::tuple<Matrix, Vec4> GetMatrix() {
    auto& m = matrix_;
    auto matrix_mul = Matrix{
        m[0], m[5], m[10], m[15],  //
        m[1], m[6], m[11], m[16],  //
        m[2], m[7], m[12], m[17],  //
        m[3], m[8], m[13], m[18],  //
    };

    auto matrix_add = Vec4{m[4], m[9], m[14], m[19]};
    return std::make_tuple(matrix_mul, matrix_add);
  }

  ColorFilterType GetType() const override { return ColorFilterType::kMatrix; }

 private:
  float matrix_[20];
#ifdef SKITY_CPU
  int16_t matrix_i16_[4][5];
#endif
};

class SRGBGammaColorFilter : public ColorFilterBase {
 public:
  explicit SRGBGammaColorFilter(ColorFilterType type) : type_(type) {}
#ifdef SKITY_CPU
  PMColor OnFilterColor(PMColor c) const override;
#endif
  ColorFilterType GetType() const override { return type_; }

 private:
  ColorFilterType type_;
};

class ComposeColorFilter : public ColorFilterBase {
 public:
#ifdef SKITY_CPU
  PMColor OnFilterColor(PMColor c) const override;
#endif

  ComposeColorFilter(std::shared_ptr<ColorFilter> outer,
                     std::shared_ptr<ColorFilter> inner)
      : outer_(outer), inner_(inner) {
    ComputeFilters();
  }

  ColorFilterType GetType() const override { return ColorFilterType::kCompose; }

  const std::vector<ColorFilter*>& GetFilters() const { return filters_; }

 private:
  void ComputeFilters() {
    if (!outer_ && !inner_) {
      return;
    }

    if (inner_) {
      auto inner_cfb = static_cast<ColorFilterBase*>(inner_.get());
      if (inner_cfb->GetType() == ColorFilterType::kCompose) {
        auto inner_compose_cf = static_cast<ComposeColorFilter*>(inner_cfb);
        auto inner_filters = inner_compose_cf->GetFilters();
        filters_.insert(filters_.end(), inner_filters.begin(),
                        inner_filters.end());
      } else {
        filters_.push_back(inner_cfb);
      }
    }

    if (outer_) {
      auto outer_cfb = static_cast<ColorFilterBase*>(outer_.get());
      if (outer_cfb->GetType() == ColorFilterType::kCompose) {
        auto outer_compose_cf = static_cast<ComposeColorFilter*>(outer_cfb);
        auto outer_filters = outer_compose_cf->GetFilters();
        filters_.insert(filters_.end(), outer_filters.begin(),
                        outer_filters.end());
      } else {
        filters_.push_back(outer_cfb);
      }
    }
  }

  std::shared_ptr<ColorFilter> outer_;
  std::shared_ptr<ColorFilter> inner_;
  std::vector<ColorFilter*> filters_;
};

}  // namespace skity
#endif  // SRC_EFFECT_COLOR_FILTER_BASE_HPP
