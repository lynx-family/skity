// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GEOMETRY_MATRIX_HPP
#define INCLUDE_SKITY_GEOMETRY_MATRIX_HPP

#include <glm/glm.hpp>
#include <skity/geometry/point.hpp>
#include <skity/macros.hpp>

namespace skity {
class Rect;

struct SKITY_API Matrix : public glm::mat4 {
 public:
  static Matrix Translate(float dx, float dy);

  static Matrix Scale(float sx, float sy);

  static Matrix Skew(float sx, float sy);

  static Matrix RotateDeg(float deg);

  static Matrix RotateDeg(float deg, Vec2 pt);

  static Matrix RotateDeg(float deg, Vec3 axis);

  static Matrix RotateRad(float rad);

  static Matrix RotateRad(float deg, Vec2 pt);

  ~Matrix() = default;

  Matrix() : glm::mat4(1.f) {}

  Matrix(float mxx, float myx, float mzx, float mwx, float mxy, float myy,
         float mzy, float mwy, float mxz, float myz, float mzz, float mwz,
         float mxt, float myt, float mzt, float mwt)
      : glm::mat4(mxx, myx, mzx, mwx, mxy, myy, mzy, mwy, mxz, myz, mzz, mwz,
                  mxt, myt, mzt, mwt) {}

  Matrix(float scale_x, float skew_x, float trans_x, float skew_y,
         float scale_y, float trans_y, float pers_0, float pers_1,
         float pers_2);

  explicit Matrix(float s);

  explicit Matrix(const glm::mat4&& m);
  explicit Matrix(const glm::mat4& m);

  Matrix(const Matrix&) = default;

  Matrix& operator=(const Matrix&) = default;

  Matrix& operator=(const glm::mat4&);

  Matrix& Reset() {
    *this = Matrix(1.0);
    return *this;
  }

  bool IsIdentity() const;

  bool IsFinite() const;

  static constexpr float kNearZeroFloat = 1.0f / (1 << 12);
  bool IsSimilarity(float tol = kNearZeroFloat) const;

  bool HasRotation() const;

  bool HasPerspective() const;

  static constexpr int kMScaleX = 0;  //!< horizontal scale factor
  static constexpr int kMSkewX = 1;   //!< horizontal skew factor
  static constexpr int kMTransX = 2;  //!< horizontal translation
  static constexpr int kMSkewY = 3;   //!< vertical skew factor
  static constexpr int kMScaleY = 4;  //!< vertical scale factor
  static constexpr int kMTransY = 5;  //!< vertical translation
  static constexpr int kMPersp0 = 6;  //!< input x perspective factor
  static constexpr int kMPersp1 = 7;  //!< input y perspective factor
  static constexpr int kMPersp2 = 8;  //!< perspective bias

  Matrix& Set(int index, float value);

  Matrix& Set(int row, int column, float value);

  Matrix& Set9(const float buffer[9]);

  float Get(int index) const;

  float Get(int row, int column) const;

  void Get9(float buffer[9]) const;

  float GetScaleX() const { return (*this)[0][0]; }

  float GetScaleY() const { return (*this)[1][1]; }

  float GetSkewX() const { return Get(kMSkewX); }

  float GetSkewY() const { return Get(kMSkewY); }

  float GetTranslateX() const { return Get(kMTransX); }

  float GetTranslateY() const { return Get(kMTransY); }

  bool Invert(Matrix* inverse) const;

  float Determinant() const;

  void Transpose();

  void MapPoints(Vec2 dst[], const Vec2 src[], int count) const;

  void MapPoints(Point dst[], const Point src[], int count) const;

  bool MapRect(Rect* dst, const Rect& src) const;

  Rect MapRect(const Rect& src) const;

  void MapFloats(Vec4* dst, const Vec4& src) const;

  bool RectStaysRect() const;

  Matrix& PreConcat(const Matrix& other);
  Matrix& PreTranslate(float dx, float dy);
  Matrix& PreScale(float sx, float sy);
  Matrix& PreScale(float sx, float sy, float px, float py);
  Matrix& PreRotate(float degrees);
  Matrix& PreRotate(float degrees, float px, float py);

  Matrix& PostConcat(const Matrix& other);
  Matrix& PostTranslate(float dx, float dy);
  Matrix& PostScale(float sx, float sy);
  Matrix& PostRotate(float degrees);
  Matrix& PostRotate(float degrees, float px, float py);
  Matrix& PostSkew(float kx, float ky);

  bool OnlyScaleAndTranslate() const;

  bool OnlyTranslate() const;

  bool OnlyScale() const;

  bool HasPersp() const;

  friend SKITY_API Matrix operator*(const Matrix& a, const Matrix& b);

 private:
  bool InvertNonIdentity(Matrix* inverse) const;

  Matrix& SetConcat(const Matrix& left, const Matrix& right);
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GEOMETRY_MATRIX_HPP
