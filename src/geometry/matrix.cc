// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtx/matrix_query.hpp>
#include <memory>
#include <skity/geometry/matrix.hpp>
#include <skity/geometry/rect.hpp>

#include "src/geometry/math.hpp"

namespace skity {

static inline glm::mat4* As_Mat4(Matrix* m) {
  return static_cast<glm::mat4*>(m);
}

static inline const glm::mat4& As_Mat4(const Matrix& m) {
  return *static_cast<const glm::mat4*>(&m);
}

static inline glm::mat4& As_Mat4(Matrix& m) {
  return *static_cast<glm::mat4*>(&m);
}

static inline float& GetkMScaleX(glm::mat4& m) { return m[0][0]; }
static inline const float& GetkMScaleX(const glm::mat4& m) { return m[0][0]; }
static inline float& GetkMSkewX(glm::mat4& m) { return m[1][0]; }
static inline const float& GetkMSkewX(const glm::mat4& m) { return m[1][0]; }
static inline float& GetkMTransX(glm::mat4& m) { return m[3][0]; }
static inline const float& GetkMTransX(const glm::mat4& m) { return m[3][0]; }
static inline float& GetkMSkewY(glm::mat4& m) { return m[0][1]; }
static inline const float& GetkMSkewY(const glm::mat4& m) { return m[0][1]; }
static inline float& GetkMScaleY(glm::mat4& m) { return m[1][1]; }
static inline const float& GetkMScaleY(const glm::mat4& m) { return m[1][1]; }
static inline float& GetkMTransY(glm::mat4& m) { return m[3][1]; }
static inline const float& GetkMTransY(const glm::mat4& m) { return m[3][1]; }
static inline float& GetkMPersp0(glm::mat4& m) { return m[0][3]; }
static inline const float& GetkMPersp0(const glm::mat4& m) { return m[0][3]; }
static inline float& GetkMPersp1(glm::mat4& m) { return m[1][3]; }
static inline const float& GetkMPersp1(const glm::mat4& m) { return m[1][3]; }
static inline float& GetkMPersp2(glm::mat4& m) { return m[3][3]; }
static inline const float& GetkMPersp2(const glm::mat4& m) { return m[3][3]; }

// static
Matrix Matrix::Translate(float dx, float dy) {
  Matrix m(1.f);
  GetkMTransX(m) = dx;
  GetkMTransY(m) = dy;
  return m;
}

// static
Matrix Matrix::Scale(float sx, float sy) {
  Matrix m(1.f);
  GetkMScaleX(m) = sx;
  GetkMScaleY(m) = sy;
  return m;
}

// static
Matrix Matrix::Skew(float sx, float sy) {
  Matrix m(1.f);
  GetkMSkewX(m) = sx;
  GetkMSkewY(m) = sy;
  return m;
}

// static
Matrix Matrix::RotateDeg(float deg) {
  return Matrix::RotateRad(glm::radians(deg), {0, 0});
}

// static
Matrix Matrix::RotateDeg(float deg, Vec2 pt) {
  return RotateRad(glm::radians(deg), pt);
}

// static
Matrix Matrix::RotateDeg(float deg, Vec3 axis) {
  return Matrix(glm::rotate(glm::mat4(1.f), glm::radians(deg), axis));
}

// static
Matrix Matrix::RotateRad(float rad) {
  return Matrix::RotateRad(rad, Vec2{0, 0});
}

// static
Matrix Matrix::RotateRad(float rad, Vec2 pt) {
  glm::mat4 d1(1.f);
  GetkMTransX(d1) = -pt.x;
  GetkMTransY(d1) = -pt.y;
  glm::mat4 d2(1.f);
  GetkMTransX(d2) = pt.x;
  GetkMTransY(d2) = pt.y;
  return Matrix(glm::rotate(d2, rad, Vec3(0, 0, 1)) * d1);
}

Matrix::Matrix(float scale_x, float skew_x, float trans_x, float skew_y,
               float scale_y, float trans_y, float pers_0, float pers_1,
               float pers_2) {
  Matrix& m = *this;
  m[0][0] = scale_x;
  m[1][0] = skew_x;
  m[2][0] = 0;
  m[3][0] = trans_x;
  m[0][1] = skew_y;
  m[1][1] = scale_y;
  m[2][1] = 0;
  m[3][1] = trans_y;
  m[0][2] = 0;
  m[1][2] = 0;
  m[2][2] = 1;
  m[3][2] = 0;
  m[0][3] = pers_0;
  m[1][3] = pers_1;
  m[2][3] = 0;
  m[3][3] = pers_2;
}

Matrix::Matrix(float s) : glm::mat4(1.f) { (*this)[0][0] = (*this)[1][1] = s; }

Matrix::Matrix(const glm::mat4&& m) { *As_Mat4(this) = std::move(m); }

Matrix::Matrix(const glm::mat4& m) { *As_Mat4(this) = m; }

Matrix& Matrix::operator=(const glm::mat4& m) {
  As_Mat4(this)->operator=(m);
  return *this;
}

bool Matrix::IsIdentity() const {
  return glm::isIdentity(*this, glm::epsilon<float>());
}

bool Matrix::IsFinite() const {
  const Matrix& m = *this;
  float accum = 0;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      accum *= m[i][j];
    }
  }

  return !FloatIsNan(accum);
}

bool Matrix::IsSimilarity(float tol) const {
  if (HasPerspective()) {
    return false;
  }

  float mx = Get(kMScaleX);
  float my = Get(kMScaleY);
  // if no skew, can just compare scale factors
  if (Get(kMSkewX) == 0 && Get(kMSkewY) == 0) {
    return !FloatNearlyZero(mx) && FloatNearlyZero(mx - my);
  }
  float sx = Get(kMSkewX);
  float sy = Get(kMSkewY);

  float perp_dot = mx * my - sx * sy;
  if (FloatNearlyZero(perp_dot, NearlyZero * NearlyZero)) {
    return false;
  }

  // upper 2x2 is rotation/reflection + uniform scale if basis vectors
  // are 90 degree rotations of each other
  return (FloatNearlyZero(mx - my, tol) && FloatNearlyZero(sx + sy, tol)) ||
         (FloatNearlyZero(mx + my, tol) && FloatNearlyZero(sx - sy, tol));
}

bool Matrix::HasRotation() const {
  return GetkMPersp0(*this) != 0.f || GetkMPersp1(*this) != 0.f ||
         GetkMPersp2(*this) != 1.f || GetkMSkewX(*this) != 0.f ||
         GetkMSkewY(*this) != 0.f;
}

bool Matrix::HasPerspective() const {
  return GetkMPersp0(*this) != 0.f || GetkMPersp1(*this) != 0.f ||
         GetkMPersp2(*this) != 1.f;
}

Matrix& Matrix::Set(int index, float value) {
  // TODO(qinlin): Fix kMPersp2
#define CASE(X)            \
  case X:                  \
    Get##X(*this) = value; \
    break;

  switch (index) {
    CASE(kMScaleX)
    CASE(kMSkewX)
    CASE(kMTransX)
    CASE(kMSkewY)
    CASE(kMScaleY)
    CASE(kMTransY)
    CASE(kMPersp0)
    CASE(kMPersp1)
    CASE(kMPersp2)
  }

#undef CASE

  return *this;
}

Matrix& Matrix::Set(int row, int column, float value) {
  if (row < 0 || row > 3 || column < 0 || column > 3) {
    return *this;
  }
  Matrix& m = *this;
  m[column][row] = value;
  return *this;
}

Matrix& Matrix::Set9(const float buffer[9]) {
  // buffer is in row major, glm::mat4 is in column major

  Matrix& m = *this;

#define ASSIGN(X) Get##X(m) = buffer[X]
  ASSIGN(kMScaleX);
  ASSIGN(kMSkewX);
  ASSIGN(kMTransX);
  ASSIGN(kMSkewY);
  ASSIGN(kMScaleY);
  ASSIGN(kMTransY);
  ASSIGN(kMPersp0);
  ASSIGN(kMPersp1);
  ASSIGN(kMPersp2);
#undef ASSIGN

  m[0][2] = m[2][0] = m[1][2] = m[2][1] = m[3][2] = m[2][3] = 0.f;
  m[2][2] = 1.f;
  return *this;
}

float Matrix::Get(int index) const {
#define CASE(X) \
  case X:       \
    return Get##X(*this);

  switch (index) {
    CASE(kMScaleX)
    CASE(kMSkewX)
    CASE(kMTransX)
    CASE(kMSkewY)
    CASE(kMScaleY)
    CASE(kMTransY)
    CASE(kMPersp0)
    CASE(kMPersp1)
    CASE(kMPersp2)
    default:
      return 0.f;
  }

#undef CASE
}

float Matrix::Get(int row, int column) const {
  if (row < 0 || row > 3 || column < 0 || column > 3) {
    return 0.f;
  }
  const Matrix& m = *this;
  return m[column][row];
}

void Matrix::Get9(float buffer[9]) const {
  const Matrix& m = *this;
#define ASSIGN(X) buffer[X] = Get##X(m)
  ASSIGN(kMScaleX);
  ASSIGN(kMSkewX);
  ASSIGN(kMTransX);
  ASSIGN(kMSkewY);
  ASSIGN(kMScaleY);
  ASSIGN(kMTransY);
  ASSIGN(kMPersp0);
  ASSIGN(kMPersp1);
  ASSIGN(kMPersp2);
#undef ASSIGN
}

bool Matrix::Invert(Matrix* inverse) const {
  if (IsIdentity()) {
    if (inverse != nullptr) {
      *inverse = Matrix(1.0);
    }
    return true;
  }
  return this->InvertNonIdentity(inverse);
}

float Matrix::Determinant() const {
  if (IsIdentity()) {
    return 1.f;
  }
  const Matrix& m = *this;
  if (OnlyScaleAndTranslate()) {
    return m[0][0] * m[1][1] * m[2][2] * m[3][3];
  }
  return glm::determinant(*this);
}

void Matrix::Transpose() { *this = glm::transpose(*this); }

bool Matrix::OnlyScaleAndTranslate() const {
  auto& m = As_Mat4(*this);
  // No z
  if (m[0][2] != 0 || m[1][2] != 0 || m[2][2] != 1 || m[3][2] != 0 ||
      m[2][0] != 0 || m[2][1] != 0) {
    return false;
  }
  // no persp
  if (m[0][3] != 0 || m[1][3] != 0 || m[2][3] != 0 || m[3][3] != 1) {
    return false;
  }
  // no skew
  if (m[0][1] != 0 || m[1][0] != 0) {
    return false;
  }
  return true;
}

bool Matrix::OnlyTranslate() const {
  auto& m = As_Mat4(*this);
  // No Scale
  if (m[0][0] != 1.f || m[1][1] != 1.f) {
    return false;
  }
  // No z
  if (m[0][2] != 0 || m[1][2] != 0 || m[2][2] != 1 || m[3][2] != 0 ||
      m[2][0] != 0 || m[2][1] != 0) {
    return false;
  }
  // no persp
  if (m[0][3] != 0 || m[1][3] != 0 || m[2][3] != 0 || m[3][3] != 1) {
    return false;
  }
  // no skew
  if (m[0][1] != 0 || m[1][0] != 0) {
    return false;
  }
  return true;
}

bool Matrix::OnlyScale() const {
  auto& m = As_Mat4(*this);
  // No translate
  if (m[3][0] != 0 || m[3][1] != 0) {
    return false;
  }
  // No z
  if (m[0][2] != 0 || m[1][2] != 0 || m[2][2] != 1 || m[3][2] != 0 ||
      m[2][0] != 0 || m[2][1] != 0) {
    return false;
  }
  // no persp
  if (m[0][3] != 0 || m[1][3] != 0 || m[2][3] != 0 || m[3][3] != 1) {
    return false;
  }
  // no skew
  if (m[0][1] != 0 || m[1][0] != 0) {
    return false;
  }
  return true;
}

bool Matrix::HasPersp() const {
  auto& m = As_Mat4(*this);
  if (m[0][3] != 0 || m[1][3] != 0 || m[2][3] != 0 || m[3][3] != 1) {
    return true;
  }
  return false;
}

bool Matrix::InvertNonIdentity(Matrix* inverse) const {
  glm::mat4 temp_inverse;
  glm::mat4* p_inverse =
      (inverse == nullptr || inverse == this) ? &temp_inverse : inverse;

  // short path
  if (OnlyScaleAndTranslate()) {
    static_cast<Matrix*>(p_inverse)->Reset();
    if (GetkMScaleX(*this) != 1.f || GetkMScaleY(*this) != 1.f) {
      float inverse_x = SkityIEEEFloatDivided(1.f, Get(kMScaleX));
      float inverse_y = SkityIEEEFloatDivided(1.f, Get(kMScaleY));
      if (FloatInfinity == inverse_x || FloatInfinity == inverse_y) {
        return false;
      }
      GetkMScaleX(*p_inverse) = inverse_x;
      GetkMScaleY(*p_inverse) = inverse_y;
      GetkMTransX(*p_inverse) = -Get(kMTransX) * inverse_x;
      GetkMTransY(*p_inverse) = -Get(kMTransY) * inverse_y;
    } else {
      GetkMTransX(*p_inverse) = -Get(kMTransX);
      GetkMTransY(*p_inverse) = -Get(kMTransY);
    }
    if (inverse == this) {
      *inverse = *p_inverse;
    }
    return true;
  }

  if (FloatNearlyZero(glm::determinant(*this))) {
    return false;
  }

  *p_inverse = glm::inverse(*this);
  if (inverse == this) {
    *inverse = *p_inverse;
  }
  return true;
}

void Matrix::MapPoints(Vec2 dst[], const Vec2 src[], int count) const {
  if (dst == nullptr || src == nullptr || count <= 0) {
    return;
  }
  for (int i = 0; i < count; ++i) {
    auto v = Vec4(src[i].x, src[i].y, 0.f, 1.f);
    auto r = (*this) * v;
    dst[i].x = r.x;
    dst[i].y = r.y;
  }
}

void Matrix::MapPoints(Point dst[], const Point src[], int count) const {
  if (dst == nullptr || src == nullptr || count <= 0) {
    return;
  }
  for (int i = 0; i < count; ++i) {
    dst[i] = (*this) * src[i];
  }
}

bool Matrix::MapRect(Rect* dst, const Rect& src) const {
  if (dst == nullptr) {
    return false;
  }

  Vec4 src_quad[4] = {{src.Left(), src.Top(), 0.f, 1.f},
                      {src.Right(), src.Top(), 0.f, 1.f},
                      {src.Right(), src.Bottom(), 0.f, 1.f},
                      {src.Left(), src.Bottom(), 0.f, 1.f}};
  Vec4 dst_quad[4];
  for (size_t i = 0; i < 4; ++i) {
    dst_quad[i] = (*this) * src_quad[i];
    float w = dst_quad[i].w;
    if (w != 0) {
      w = 1.0f / w;
    }
    dst_quad[i] = dst_quad[i] * w;
  }
  float left = dst_quad[0].x;
  float right = dst_quad[0].x;
  float top = dst_quad[0].y;
  float bottom = dst_quad[0].y;
  for (size_t i = 1; i < 4; i++) {
    left = std::min(dst_quad[i].x, left);
    right = std::max(dst_quad[i].x, right);
    top = std::min(dst_quad[i].y, top);
    bottom = std::max(dst_quad[i].y, bottom);
  }
  dst->SetLTRB(left, top, right, bottom);

  return RectStaysRect();
}

Rect Matrix::MapRect(const Rect& src) const {
  Rect dst;
  MapRect(&dst, src);
  return dst;
}

void Matrix::MapFloats(Vec4* dst, const Vec4& src) const {
  if (dst == nullptr) {
    return;
  }
  *dst = (*this) * src;
}

bool Matrix::RectStaysRect() const {
  auto& m = As_Mat4(*this);
  if (m[0][2] != 0 || m[1][2] != 0 || m[2][2] != 1 || m[3][2] != 0 ||
      m[0][3] != 0 || m[1][3] != 0 || m[2][3] != 0 || m[3][3] != 1) {
    return false;
  }

  if (m[0][1] != 0 || m[1][0] != 0) {
    return m[0][0] == 0 && m[1][1] == 0 && m[1][0] != 0 && m[0][1] != 0;
  } else {
    return m[0][0] != 0 && m[1][1] != 0;
  }
}

Matrix& Matrix::PreConcat(const Matrix& other) {
  if (!other.IsIdentity()) {
    SetConcat(*this, other);
  }
  return *this;
}

Matrix& Matrix::PostConcat(const Matrix& other) {
  if (!other.IsIdentity()) {
    SetConcat(other, *this);
  }
  return *this;
}

Matrix& Matrix::PreTranslate(float dx, float dy) {
  Matrix m = Matrix::Translate(dx, dy);
  SetConcat(*this, m);
  return *this;
}

Matrix& Matrix::PostTranslate(float dx, float dy) {
  Matrix m = Matrix::Translate(dx, dy);
  SetConcat(m, *this);
  return *this;
}

Matrix& Matrix::PreScale(float sx, float sy) {
  Matrix m = Matrix::Scale(sx, sy);
  SetConcat(*this, m);
  return *this;
}

Matrix& Matrix::PostScale(float sx, float sy) {
  Matrix m = Matrix::Scale(sx, sy);
  SetConcat(m, *this);
  return *this;
}

Matrix& Matrix::PreScale(float sx, float sy, float px, float py) {
  glm::mat4 d1 = glm::mat4(1.f);
  GetkMTransX(d1) = -px;
  GetkMTransY(d1) = -py;
  glm::mat4 s = glm::mat4(1.f);
  GetkMScaleX(s) = sx;
  GetkMScaleY(s) = sy;
  glm::mat4 d2 = glm::mat4(1.f);
  GetkMTransX(d2) = px;
  GetkMTransY(d2) = py;
  As_Mat4(*this) = As_Mat4(*this) * d2 * s * d1;
  return *this;
}

Matrix& Matrix::PreRotate(float degrees) {
  return this->PreRotate(degrees, 0, 0);
}

Matrix& Matrix::PostRotate(float degrees) {
  return this->PostRotate(degrees, 0, 0);
}

Matrix& Matrix::PreRotate(float degrees, float px, float py) {
  auto r = Matrix::RotateDeg(degrees, Vec2(px, py));
  return this->PreConcat(r);
}

Matrix& Matrix::PostRotate(float degrees, float px, float py) {
  auto r = Matrix::RotateDeg(degrees, Vec2(px, py));
  return this->PostConcat(r);
}

Matrix& Matrix::PostSkew(float kx, float ky) {
  Matrix m = Matrix::Skew(kx, ky);
  return this->PostConcat(m);
}

// Notice: we ignore element related z.
Matrix& Matrix::SetConcat(const Matrix& left, const Matrix& right) {
  if (left.IsIdentity()) {
    *this = right;
  } else if (right.IsIdentity()) {
    *this = left;
  } else if (left.OnlyScaleAndTranslate() && right.OnlyScaleAndTranslate()) {
    float scale_x = left.Get(kMScaleX) * right.Get(kMScaleX);
    float scale_y = left.Get(kMScaleY) * right.Get(kMScaleY);
    float tranlate_x =
        left.Get(kMScaleX) * right.Get(kMTransX) + left.Get(kMTransX);
    float tranlate_y =
        left.Get(kMScaleY) * right.Get(kMTransY) + left.Get(kMTransY);
    this->Set(kMScaleX, scale_x);
    this->Set(kMScaleY, scale_y);
    this->Set(kMTransX, tranlate_x);
    this->Set(kMTransY, tranlate_y);
  } else {
    *this = left * right;
  }
  return *this;
}

SKITY_API Matrix operator*(const Matrix& a, const Matrix& b) {
  if (a.IsIdentity()) {
    return b;
  }
  if (b.IsIdentity()) {
    return a;
  }
  return Matrix(As_Mat4(a) * As_Mat4(b));
}

}  // namespace skity
