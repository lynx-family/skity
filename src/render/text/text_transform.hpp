// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_TEXT_TEXT_TRANSFORM_HPP
#define SRC_RENDER_TEXT_TEXT_TRANSFORM_HPP

#include <cstdint>
#include <skity/geometry/matrix.hpp>
#include <skity/geometry/point.hpp>

#include "src/geometry/math.hpp"

namespace skity {

class Matrix22 final {
 public:
  Matrix22() = default;
  Matrix22(float scale_x, float skew_x, float skew_y, float scale_y)
      : scale_x_(scale_x),
        skew_x_(skew_x),
        skew_y_(skew_y),
        scale_y_(scale_y) {}

  friend inline bool operator==(const Matrix22& lhs, const Matrix22& rhs) {
    return lhs.scale_x_ == rhs.scale_x_ && lhs.skew_x_ == rhs.skew_x_ &&
           lhs.skew_y_ == rhs.skew_y_ && lhs.scale_y_ == rhs.scale_y_;
  }

  friend Matrix22 operator*(const Matrix22& lhs, const Matrix22& rhs) {
    if (lhs.IsIdentity()) {
      return rhs;
    }
    if (rhs.IsIdentity()) {
      return lhs;
    }
    return Matrix22(lhs.scale_x_ * rhs.scale_x_ + lhs.skew_x_ * rhs.skew_y_,
                    lhs.scale_x_ * rhs.skew_x_ + lhs.skew_x_ * rhs.scale_y_,
                    lhs.skew_y_ * rhs.scale_x_ + lhs.scale_y_ * rhs.skew_y_,
                    lhs.skew_y_ * rhs.skew_x_ + lhs.scale_y_ * rhs.scale_y_);
  }

  friend Vec2 operator*(const Matrix22& lhs, const Vec2& p) {
    return Vec2(lhs.scale_x_ * p.x + lhs.skew_x_ * p.y,
                lhs.skew_y_ * p.x + lhs.scale_y_ * p.y);
  }

  void QRDecompose(Matrix22* q, Matrix22* r) const;

  float Det() const { return scale_x_ * scale_y_ - skew_x_ * skew_y_; }

  bool IsIdentity() const {
    return scale_x_ == 1.f && skew_x_ == 0.f && skew_y_ == 0.f &&
           scale_y_ == 1.f;
  }

  bool Identity(const Matrix22& m) const {
    return FloatNearlyZero(scale_x_ - m.scale_x_) &&
           FloatNearlyZero(skew_x_ - m.skew_x_) &&
           FloatNearlyZero(skew_y_ - m.skew_y_) &&
           FloatNearlyZero(scale_y_ - m.scale_y_);
  }

  void MapPoints(Vec2* dst, const Vec2* src, int count) const;

  float GetScaleX() const { return scale_x_; }
  float GetSkewX() const { return skew_x_; }
  float GetSkewY() const { return skew_y_; }
  float GetScaleY() const { return scale_y_; }

  Matrix ToMatrix() const {
    Matrix matrix;
    matrix.Set(Matrix::kMScaleX, scale_x_);
    matrix.Set(Matrix::kMSkewX, skew_x_);
    matrix.Set(Matrix::kMSkewY, skew_y_);
    matrix.Set(Matrix::kMScaleY, scale_y_);

    return matrix;
  }

  Matrix22 Inverse() const {
    float det = Det();
    if (FloatNearlyZero(det)) {
      return Matrix22{};
    }
    return Matrix22{scale_y_ / det, -skew_x_ / det, -skew_y_ / det,
                    scale_x_ / det};
  }

 private:
  float scale_x_ = 1.f;
  float skew_x_ = 0.f;
  float skew_y_ = 0.f;
  float scale_y_ = 1.f;

  friend class ScalerContextDesc;
};

}  // namespace skity

#endif  // SRC_RENDER_TEXT_TEXT_TRANSFORM_HPP
