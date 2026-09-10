// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_TYPES_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_TYPES_HPP

// Value types of the header-only RAII layer: binary-compatible with the C
// PODs (Rect / Matrix inherit skity_rect / skity_matrix, static_assert on
// sizeof) and the scoped enums mirroring the legacy C++ enumerators. Helper
// methods that had only C++ callers live here as inline code (coverage
// discipline: no C functions are added for them).

#include <skity_c/skity_types.h>

#include <cmath>
#include <cstdint>

namespace skity {
namespace raii {

using Color = skity_color;
using Color4f = skity_color4f;
using Point = skity_point;
using Vec2 = skity_vec2;
using Vec4 = skity_vec4;

/** Blend modes, mirroring the legacy skity::BlendMode enumerators. */
enum class BlendMode : uint32_t {
  kClear = SKITY_BLEND_MODE_CLEAR,
  kSrc = SKITY_BLEND_MODE_SRC,
  kDst = SKITY_BLEND_MODE_DST,
  kSrcOver = SKITY_BLEND_MODE_SRC_OVER,
  kDstOver = SKITY_BLEND_MODE_DST_OVER,
  kSrcIn = SKITY_BLEND_MODE_SRC_IN,
  kDstIn = SKITY_BLEND_MODE_DST_IN,
  kSrcOut = SKITY_BLEND_MODE_SRC_OUT,
  kDstOut = SKITY_BLEND_MODE_DST_OUT,
  kSrcATop = SKITY_BLEND_MODE_SRC_A_TOP,
  kDstATop = SKITY_BLEND_MODE_DST_A_TOP,
  kXor = SKITY_BLEND_MODE_XOR,
  kPlus = SKITY_BLEND_MODE_PLUS,
  kModulate = SKITY_BLEND_MODE_MODULATE,
  kScreen = SKITY_BLEND_MODE_SCREEN,
  kOverlay = SKITY_BLEND_MODE_OVERLAY,
  kDarken = SKITY_BLEND_MODE_DARKEN,
  kLighten = SKITY_BLEND_MODE_LIGHTEN,
  kColorDodge = SKITY_BLEND_MODE_COLOR_DODGE,
  kColorBurn = SKITY_BLEND_MODE_COLOR_BURN,
  kHardLight = SKITY_BLEND_MODE_HARD_LIGHT,
  kSoftLight = SKITY_BLEND_MODE_SOFT_LIGHT,
  kDifference = SKITY_BLEND_MODE_DIFFERENCE,
  kExclusion = SKITY_BLEND_MODE_EXCLUSION,
  kMultiply = SKITY_BLEND_MODE_MULTIPLY,
  kHue = SKITY_BLEND_MODE_HUE,
  kSaturation = SKITY_BLEND_MODE_SATURATION,
  kColor = SKITY_BLEND_MODE_COLOR,
  kLuminosity = SKITY_BLEND_MODE_LUMINOSITY,
};

inline constexpr skity_blend_mode to_c(BlendMode mode) {
  return static_cast<skity_blend_mode>(mode);
}

/** Shader tile modes, mirroring the legacy skity::TileMode. */
enum class TileMode : uint32_t {
  kClamp = SKITY_TILE_MODE_CLAMP,
  kRepeat = SKITY_TILE_MODE_REPEAT,
  kMirror = SKITY_TILE_MODE_MIRROR,
  kDecal = SKITY_TILE_MODE_DECAL,
};

inline constexpr skity_tile_mode to_c(TileMode mode) {
  return static_cast<skity_tile_mode>(mode);
}

// Color helpers mirroring the legacy skity:: Color utilities. Colors are
// unpremultiplied ARGB packed as 0xAARRGGBB.
inline constexpr Color ColorSetA(Color c, uint8_t a) {
  return (c & 0x00FFFFFFu) | (static_cast<uint32_t>(a) << 24);
}

inline constexpr uint8_t ColorGetA(Color c) {
  return static_cast<uint8_t>(c >> 24);
}

/** Pack floating-point RGBA (each in [0, 1]) into an ARGB color. */
inline constexpr Color ColorPackRGBA(float r, float g, float b, float a) {
  auto channel = [](float v) -> uint32_t {
    return static_cast<uint32_t>(v * 255.f + 0.5f) & 0xFFu;
  };
  return (channel(a) << 24) | (channel(r) << 16) | (channel(g) << 8) |
         channel(b);
}

inline constexpr Color Color_WHITE = 0xFFFFFFFFu;
inline constexpr Color Color_BLACK = 0xFF000000u;
inline constexpr Color Color_TRANSPARENT = 0x00000000u;
inline constexpr Color Color_RED = 0xFFFF0000u;
inline constexpr Color Color_GREEN = 0xFF00FF00u;
inline constexpr Color Color_BLUE = 0xFF0000FFu;

/**
 * Axis-aligned rectangle. Inherits skity_rect so the layout stays
 * binary-compatible with the C POD: a Rect* is a skity_rect* and crosses
 * the ABI boundary without conversion.
 */
class Rect : public skity_rect {
 public:
  constexpr Rect() : skity_rect{0.f, 0.f, 0.f, 0.f} {}

  constexpr Rect(float l, float t, float r, float b) : skity_rect{l, t, r, b} {}

  constexpr static Rect MakeEmpty() { return Rect(); }

  constexpr static Rect MakeLTRB(float l, float t, float r, float b) {
    return Rect(l, t, r, b);
  }

  constexpr static Rect MakeXYWH(float x, float y, float w, float h) {
    return Rect(x, y, x + w, y + h);
  }

  constexpr static Rect MakeWH(float w, float h) {
    return Rect(0.f, 0.f, w, h);
  }

  constexpr float X() const { return left; }
  constexpr float Y() const { return top; }
  constexpr float Left() const { return left; }
  constexpr float Top() const { return top; }
  constexpr float Right() const { return right; }
  constexpr float Bottom() const { return bottom; }

  constexpr float Width() const { return right - left; }
  constexpr float Height() const { return bottom - top; }
  constexpr float CenterX() const { return (left + right) * 0.5f; }
  constexpr float CenterY() const { return (top + bottom) * 0.5f; }

  constexpr bool IsEmpty() const { return !(left < right && top < bottom); }
  void SetEmpty() { *this = MakeEmpty(); }

  void SetLTRB(float l, float t, float r, float b) { *this = Rect(l, t, r, b); }
  void SetXYWH(float x, float y, float w, float h) {
    *this = MakeXYWH(x, y, w, h);
  }

  void Offset(float dx, float dy) {
    left += dx;
    top += dy;
    right += dx;
    bottom += dy;
  }

  static Rect MakeOffset(const Rect& r, float dx, float dy) {
    Rect out = r;
    out.Offset(dx, dy);
    return out;
  }

  void Inset(float dx, float dy) { Offset(-dx, -dy); }

  constexpr bool operator==(const Rect& o) const {
    return left == o.left && top == o.top && right == o.right &&
           bottom == o.bottom;
  }
  constexpr bool operator!=(const Rect& o) const { return !(*this == o); }
};

static_assert(sizeof(Rect) == sizeof(skity_rect),
              "Rect must stay binary-compatible with skity_rect");

/**
 * Column-major 4x4 matrix, binary-compatible with skity_matrix (same
 * layout as the legacy skity::Matrix, which wraps glm::mat4). Construct /
 * combine helpers are implemented inline per the coverage discipline.
 */
class Matrix : public skity_matrix {
 public:
  constexpr Matrix()
      : Matrix(1.f, 0.f, 0.f, 0.f,  //
               0.f, 1.f, 0.f, 0.f,  //
               0.f, 0.f, 1.f, 0.f,  //
               0.f, 0.f, 0.f, 1.f) {}

  /** Column-major element constructor (column by column, as legacy). */
  constexpr Matrix(float mxx, float myx, float mzx, float mwx,  //
                   float mxy, float myy, float mzy, float mwy,  //
                   float mxz, float myz, float mzz, float mwz,  //
                   float mxt, float myt, float mzt, float mwt)
      : skity_matrix{mxx, myx, mzx, mwx,  //
                     mxy, myy, mzy, mwy,  //
                     mxz, myz, mzz, mwz,  //
                     mxt, myt, mzt, mwt} {}

  /** Affine 2D constructor: (scale_x, skew_x, trans_x, skew_y, scale_y,
   * trans_y, pers_0, pers_1, pers_2), matching the legacy 9-float ctor. */
  constexpr Matrix(float scale_x, float skew_x, float trans_x,  //
                   float skew_y, float scale_y, float trans_y,  //
                   float pers_0, float pers_1, float pers_2)
      : Matrix(scale_x, skew_y, 0.f, pers_0,  //
               skew_x, scale_y, 0.f, pers_1,  //
               0.f, 0.f, 1.f, 0.f,            //
               trans_x, trans_y, 0.f, pers_2) {}

  constexpr static Matrix Translate(float dx, float dy) {
    return Matrix(1.f, 0.f, 0.f, 0.f,  //
                  0.f, 1.f, 0.f, 0.f,  //
                  0.f, 0.f, 1.f, 0.f,  //
                  dx, dy, 0.f, 1.f);
  }

  constexpr static Matrix Scale(float sx, float sy) {
    return Matrix(sx, 0.f, 0.f, 0.f,   //
                  0.f, sy, 0.f, 0.f,   //
                  0.f, 0.f, 1.f, 0.f,  //
                  0.f, 0.f, 0.f, 1.f);
  }

  constexpr static Matrix Skew(float sx, float sy) {
    return Matrix(1.f, sy, 0.f, 0.f,   //
                  sx, 1.f, 0.f, 0.f,   //
                  0.f, 0.f, 1.f, 0.f,  //
                  0.f, 0.f, 0.f, 1.f);
  }

  static Matrix RotateDeg(float deg) { return RotateRad(deg * kDegToRad); }

  static Matrix RotateDeg(float deg, float px, float py) {
    return RotateRad(deg * kDegToRad, px, py);
  }

  /** Rotation about a Vec2 point (legacy overload shape). */
  static Matrix RotateDeg(float deg, Vec2 pt) {
    return RotateRad(deg * kDegToRad, pt.e[0], pt.e[1]);
  }

  /** Uniform diagonal matrix (legacy explicit Matrix(float) shape). */
  constexpr explicit Matrix(float s)
      : Matrix(s, 0.f, 0.f, 0.f,  //
               0.f, s, 0.f, 0.f,  //
               0.f, 0.f, s, 0.f,  //
               0.f, 0.f, 0.f, s) {}

  static Matrix RotateRad(float rad) { return RotateRad(rad, 0.f, 0.f); }

  /** Rotation about (px, py): T(px,py) * R(rad) * T(-px,-py). */
  static Matrix RotateRad(float rad, float px, float py) {
    float c = std::cos(rad);
    float s = std::sin(rad);
    return Matrix(c, s, 0.f, 0.f,      //
                  -s, c, 0.f, 0.f,     //
                  0.f, 0.f, 1.f, 0.f,  //
                  px * (1.f - c) + py * s, py * (1.f - c) - px * s, 0.f, 1.f);
  }

  bool IsIdentity() const { return *this == Matrix(); }

  Matrix& Reset() {
    *this = Matrix();
    return *this;
  }

  constexpr float Get(int row, int col) const { return m[col * 4 + row]; }
  Matrix& Set(int row, int col, float value) {
    m[col * 4 + row] = value;
    return *this;
  }

  constexpr float GetScaleX() const { return m[0]; }
  constexpr float GetScaleY() const { return m[5]; }
  constexpr float GetSkewX() const { return m[4]; }
  constexpr float GetSkewY() const { return m[1]; }
  constexpr float GetTranslateX() const { return m[12]; }
  constexpr float GetTranslateY() const { return m[13]; }

  Matrix& SetScaleX(float v) {
    m[0] = v;
    return *this;
  }
  Matrix& SetScaleY(float v) {
    m[5] = v;
    return *this;
  }
  Matrix& SetSkewX(float v) {
    m[4] = v;
    return *this;
  }
  Matrix& SetSkewY(float v) {
    m[1] = v;
    return *this;
  }
  Matrix& SetTranslateX(float v) {
    m[12] = v;
    return *this;
  }
  Matrix& SetTranslateY(float v) {
    m[13] = v;
    return *this;
  }

  /**
   * 4x4 column-major product: result = a * b. Applying result to a point
   * applies b first, then a (legacy SetConcat semantics).
   */
  static Matrix Concat(const Matrix& a, const Matrix& b) {
    Matrix out;
    for (int col = 0; col < 4; col++) {
      for (int row = 0; row < 4; row++) {
        out.m[col * 4 + row] = a.m[row] * b.m[col * 4] +          //
                               a.m[4 + row] * b.m[col * 4 + 1] +  //
                               a.m[8 + row] * b.m[col * 4 + 2] +  //
                               a.m[12 + row] * b.m[col * 4 + 3];
      }
    }
    return out;
  }

  /** this = this * other (other is applied to points first). */
  Matrix& PreConcat(const Matrix& other) {
    *this = Concat(*this, other);
    return *this;
  }

  /** this = other * this (other is applied to points last). */
  Matrix& PostConcat(const Matrix& other) {
    *this = Concat(other, *this);
    return *this;
  }

  Matrix& PreTranslate(float dx, float dy) {
    return PreConcat(Translate(dx, dy));
  }
  Matrix& PreScale(float sx, float sy) { return PreConcat(Scale(sx, sy)); }
  Matrix& PreRotate(float degrees) { return PreConcat(RotateDeg(degrees)); }
  Matrix& PostTranslate(float dx, float dy) {
    return PostConcat(Translate(dx, dy));
  }
  Matrix& PostScale(float sx, float sy) { return PostConcat(Scale(sx, sy)); }
  Matrix& PostRotate(float degrees) { return PostConcat(RotateDeg(degrees)); }

  /** Apply the matrix to a point (x, y); returns the transformed point. */
  Point MapPoint(float x, float y) const {
    Point out;
    out.e[0] = m[0] * x + m[4] * y + m[12];
    out.e[1] = m[1] * x + m[5] * y + m[13];
    out.e[2] = 0.f;
    out.e[3] = 1.f;
    return out;
  }

  /** Conservative bounding box of the transformed rect corners. */
  Rect MapRect(const Rect& r) const {
    Point q[4] = {
        MapPoint(r.left, r.top),
        MapPoint(r.right, r.top),
        MapPoint(r.right, r.bottom),
        MapPoint(r.left, r.bottom),
    };
    float l = q[0].e[0], t = q[0].e[1], rr = q[0].e[0], b = q[0].e[1];
    for (int i = 1; i < 4; i++) {
      l = q[i].e[0] < l ? q[i].e[0] : l;
      t = q[i].e[1] < t ? q[i].e[1] : t;
      rr = q[i].e[0] > rr ? q[i].e[0] : rr;
      b = q[i].e[1] > b ? q[i].e[1] : b;
    }
    return Rect(l, t, rr, b);
  }

  constexpr bool operator==(const Matrix& o) const {
    for (int i = 0; i < 16; i++) {
      if (m[i] != o.m[i]) {
        return false;
      }
    }
    return true;
  }
  constexpr bool operator!=(const Matrix& o) const { return !(*this == o); }

  static constexpr float kDegToRad = 0.017453292519943295f;
};

static_assert(sizeof(Matrix) == sizeof(skity_matrix),
              "Matrix must stay binary-compatible with skity_matrix");

/**
 * Rounded rectangle value type mirroring the legacy skity::RRect: a bounds
 * rect plus one (x, y) radius pair per corner (TL, TR, BR, BL). Pure value
 * semantics; Canvas::DrawRRect takes it directly.
 */
class RRect {
 public:
  RRect() = default;

  /** Rect with square (zero) corner radii. */
  void SetRect(const Rect& rect) {
    bounds_ = rect;
    for (auto& r : radii_) {
      r = Vec2{0.f, 0.f};
    }
  }

  /** Ellipse inscribed in @p oval: each radius is half the extents. */
  void SetOval(const Rect& oval) {
    bounds_ = oval;
    Vec2 half{oval.Width() * 0.5f, oval.Height() * 0.5f};
    for (auto& r : radii_) {
      r = half;
    }
  }

  /** Rect with per-corner radii; @p radii holds four (x, y) pairs. */
  void SetRectRadii(const Rect& rect, const Vec2 radii[4]) {
    bounds_ = rect;
    for (int i = 0; i < 4; i++) {
      radii_[i] = radii[i];
    }
  }

  void Offset(float dx, float dy) { bounds_.Offset(dx, dy); }

  const Rect& GetBounds() const { return bounds_; }
  /** Corner radii in TL, TR, BR, BL order; matches skity_canvas_draw_rrect. */
  const Vec2* GetRadii() const { return radii_; }

 private:
  Rect bounds_;
  Vec2 radii_[4] = {};
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_TYPES_HPP
