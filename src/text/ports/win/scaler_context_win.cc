/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/ports/win/scaler_context_win.hpp"

// clang-format off
#include "src/text/ports/win/dwrite_version.hpp"
#include "src/base/platform/win/handle_result.hpp"
#include "src/text/ports/win/dwrite_utils.hpp"
#include "src/text/ports/win/glyph_data_win_access.hpp"
// clang-format on

#include <d2d1.h>
#include <dwrite.h>
#include <dwrite_2.h>
#include <dwrite_3.h>
#include <png.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <skity/effect/shader.hpp>
#include <skity/geometry/stroke.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/image.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/graphic/sampling_options.hpp>
#include <skity/render/canvas.hpp>
#include <utility>
#include <vector>

#include "src/logging.hpp"
#include "src/render/auto_canvas.hpp"

namespace skity {
namespace {

static constexpr float kSkiaMaskGammaContrast = 128.f / 255.f;

static float ComputeFakeBoldScale(float text_size) {
  if (text_size <= 9.0f) {
    return 1.0f / 24.0f;
  }
  if (text_size >= 36.0f) {
    return 1.0f / 32.0f;
  }

  float ratio = (text_size - 9.0f) / 27.0f;
  return (1.0f - ratio) / 24.0f + ratio / 32.0f;
}

/** Reverse all 4 bytes in a 32bit value.
  e.g. 0x12345678 -> 0x78563412
*/
static constexpr uint32_t EndianSwap32(uint32_t value) {
  return ((value & 0xFF) << 24) | ((value & 0xFF00) << 8) |
         ((value & 0xFF0000) >> 8) | (value >> 24);
}

int16_t ReadBigEndianInt16(const uint8_t* data) {
  return static_cast<int16_t>((static_cast<uint16_t>(data[0]) << 8) |
                              static_cast<uint16_t>(data[1]));
}

float ScaleDWriteMetric(int32_t value, float text_size, float upem) {
  return text_size * static_cast<float>(value) / upem;
}

uint8_t DWriteColorChannelToByte(float value) {
  if (value <= 0.0f) {
    return 0;
  }
  if (value >= 1.0f) {
    return 255;
  }
  return static_cast<uint8_t>(std::round(value * 255.0f));
}

Color DWriteColorToColor(const DWRITE_COLOR_F& color) {
  return ColorSetARGB(
      DWriteColorChannelToByte(color.a), DWriteColorChannelToByte(color.r),
      DWriteColorChannelToByte(color.g), DWriteColorChannelToByte(color.b));
}

#ifdef DWRITE_GLYPH_IMAGE_FORMATS_COLR_PAINT_TREE_DEFINED
DWRITE_COLOR_F ColorToDWriteColor(Color color) {
  DWRITE_COLOR_F dwrite_color;
  dwrite_color.r = static_cast<float>(ColorGetR(color)) / 255.0f;
  dwrite_color.g = static_cast<float>(ColorGetG(color)) / 255.0f;
  dwrite_color.b = static_cast<float>(ColorGetB(color)) / 255.0f;
  dwrite_color.a = static_cast<float>(ColorGetA(color)) / 255.0f;
  return dwrite_color;
}

TileMode DWriteExtendModeToTileMode(D2D1_EXTEND_MODE mode) {
  switch (mode) {
    case D2D1_EXTEND_MODE_WRAP:
      return TileMode::kRepeat;
    case D2D1_EXTEND_MODE_MIRROR:
      return TileMode::kMirror;
    case D2D1_EXTEND_MODE_CLAMP:
    default:
      return TileMode::kClamp;
  }
}

BlendMode DWriteCompositeModeToBlendMode(DWRITE_COLOR_COMPOSITE_MODE mode) {
  switch (mode) {
    case DWRITE_COLOR_COMPOSITE_CLEAR:
      return BlendMode::kClear;
    case DWRITE_COLOR_COMPOSITE_SRC:
      return BlendMode::kSrc;
    case DWRITE_COLOR_COMPOSITE_DEST:
      return BlendMode::kDst;
    case DWRITE_COLOR_COMPOSITE_SRC_OVER:
      return BlendMode::kSrcOver;
    case DWRITE_COLOR_COMPOSITE_DEST_OVER:
      return BlendMode::kDstOver;
    case DWRITE_COLOR_COMPOSITE_SRC_IN:
      return BlendMode::kSrcIn;
    case DWRITE_COLOR_COMPOSITE_DEST_IN:
      return BlendMode::kDstIn;
    case DWRITE_COLOR_COMPOSITE_SRC_OUT:
      return BlendMode::kSrcOut;
    case DWRITE_COLOR_COMPOSITE_DEST_OUT:
      return BlendMode::kDstOut;
    case DWRITE_COLOR_COMPOSITE_SRC_ATOP:
      return BlendMode::kSrcATop;
    case DWRITE_COLOR_COMPOSITE_DEST_ATOP:
      return BlendMode::kDstATop;
    case DWRITE_COLOR_COMPOSITE_XOR:
      return BlendMode::kXor;
    case DWRITE_COLOR_COMPOSITE_PLUS:
      return BlendMode::kPlus;
    case DWRITE_COLOR_COMPOSITE_SCREEN:
      return BlendMode::kScreen;
    case DWRITE_COLOR_COMPOSITE_OVERLAY:
      return BlendMode::kOverlay;
    case DWRITE_COLOR_COMPOSITE_DARKEN:
      return BlendMode::kDarken;
    case DWRITE_COLOR_COMPOSITE_LIGHTEN:
      return BlendMode::kLighten;
    case DWRITE_COLOR_COMPOSITE_COLOR_DODGE:
      return BlendMode::kColorDodge;
    case DWRITE_COLOR_COMPOSITE_COLOR_BURN:
      return BlendMode::kColorBurn;
    case DWRITE_COLOR_COMPOSITE_HARD_LIGHT:
      return BlendMode::kHardLight;
    case DWRITE_COLOR_COMPOSITE_SOFT_LIGHT:
      return BlendMode::kSoftLight;
    case DWRITE_COLOR_COMPOSITE_DIFFERENCE:
      return BlendMode::kDifference;
    case DWRITE_COLOR_COMPOSITE_EXCLUSION:
      return BlendMode::kExclusion;
    case DWRITE_COLOR_COMPOSITE_MULTIPLY:
      return BlendMode::kMultiply;
    case DWRITE_COLOR_COMPOSITE_HSL_HUE:
      return BlendMode::kHue;
    case DWRITE_COLOR_COMPOSITE_HSL_SATURATION:
      return BlendMode::kSaturation;
    case DWRITE_COLOR_COMPOSITE_HSL_COLOR:
      return BlendMode::kColor;
    case DWRITE_COLOR_COMPOSITE_HSL_LUMINOSITY:
      return BlendMode::kLuminosity;
    default:
      return BlendMode::kDst;
  }
}

float Vec2Cross(const Vec2& lhs, const Vec2& rhs) {
  return lhs.x * rhs.y - lhs.y * rhs.x;
}

Vec2 Vec2Projection(const Vec2& lhs, const Vec2& rhs) {
  float length = std::sqrt(rhs.x * rhs.x + rhs.y * rhs.y);
  if (length == 0.0f) {
    return Vec2{};
  }

  Vec2 normalized = {rhs.x / length, rhs.y / length};
  float scale = (lhs.x * rhs.x + lhs.y * rhs.y) / length;
  return {normalized.x * scale, normalized.y * scale};
}

bool Color4fHasAlpha(const Color4f& color) { return color.a > 0.0f; }
#endif

bool DWriteRectIsEmpty(const D2D_RECT_F& rect) {
  return rect.left >= rect.right || rect.top >= rect.bottom;
}

Rect DWriteRectToRect(const D2D_RECT_F& rect) {
  return Rect::MakeLTRB(rect.left, rect.top, rect.right, rect.bottom);
}

Matrix DWriteMatrixToMatrix(const DWRITE_MATRIX& matrix) {
  return Matrix{matrix.m11, matrix.m21, matrix.dx,  //
                matrix.m12, matrix.m22, matrix.dy,  //
                0.0f,       0.0f,       1.0f};
}

class DWritePathSink final : public IDWriteGeometrySink {
 public:
  explicit DWritePathSink(Path* path) : path_(path) {}

  SK_STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
    if (!object) {
      return E_POINTER;
    }

    if (riid == __uuidof(IUnknown) || riid == __uuidof(IDWriteGeometrySink)) {
      *object = static_cast<IDWriteGeometrySink*>(this);
      AddRef();
      return S_OK;
    }

    *object = nullptr;
    return E_NOINTERFACE;
  }

  SK_STDMETHODIMP_(ULONG) AddRef() override {
    return static_cast<ULONG>(InterlockedIncrement(&ref_count_));
  }

  SK_STDMETHODIMP_(ULONG) Release() override {
    ULONG result = static_cast<ULONG>(InterlockedDecrement(&ref_count_));
    if (result == 0) {
      delete this;
    }
    return result;
  }

  SK_STDMETHODIMP_(void) SetFillMode(D2D1_FILL_MODE fill_mode) override {
    switch (fill_mode) {
      case D2D1_FILL_MODE_ALTERNATE:
        path_->SetFillType(Path::PathFillType::kEvenOdd);
        break;
      case D2D1_FILL_MODE_WINDING:
        path_->SetFillType(Path::PathFillType::kWinding);
        break;
      default:
        break;
    }
  }

  SK_STDMETHODIMP_(void)
  SetSegmentFlags(D2D1_PATH_SEGMENT segment_flags) override {}

  SK_STDMETHODIMP_(void)
  BeginFigure(D2D1_POINT_2F start_point,
              D2D1_FIGURE_BEGIN figure_begin) override {
    started_ = false;
    current_ = start_point;
  }

  SK_STDMETHODIMP_(void)
  AddLines(const D2D1_POINT_2F* points, UINT points_count) override {
    for (UINT i = 0; i < points_count; i++) {
      if (!CurrentIsNot(points[i])) {
        continue;
      }
      GoingTo(points[i]);
      path_->LineTo(points[i].x, points[i].y);
    }
  }

  SK_STDMETHODIMP_(void)
  AddBeziers(const D2D1_BEZIER_SEGMENT* beziers, UINT beziers_count) override {
    for (UINT i = 0; i < beziers_count; i++) {
      const D2D1_BEZIER_SEGMENT& bezier = beziers[i];
      if (!CurrentIsNot(bezier.point1) && !CurrentIsNot(bezier.point2) &&
          !CurrentIsNot(bezier.point3)) {
        continue;
      }

      D2D1_POINT_2F start_point = current_;
      D2D1_POINT_2F quadratic_control;
      const bool is_quadratic =
          CheckQuadratic(start_point, bezier, &quadratic_control);

      GoingTo(bezier.point3);
      if (is_quadratic) {
        path_->QuadTo(quadratic_control.x, quadratic_control.y, bezier.point3.x,
                      bezier.point3.y);
      } else {
        path_->CubicTo(bezier.point1.x, bezier.point1.y, bezier.point2.x,
                       bezier.point2.y, bezier.point3.x, bezier.point3.y);
      }
    }
  }

  SK_STDMETHODIMP_(void) EndFigure(D2D1_FIGURE_END figure_end) override {
    if (started_) {
      path_->Close();
    }
  }

  SK_STDMETHODIMP Close() override { return S_OK; }

 private:
  void GoingTo(D2D1_POINT_2F point) {
    if (!started_) {
      started_ = true;
      path_->MoveTo(current_.x, current_.y);
    }
    current_ = point;
  }

  bool CurrentIsNot(D2D1_POINT_2F point) const {
    return current_.x != point.x || current_.y != point.y;
  }

  static bool ApproximatelyEqual(float lhs, float rhs) {
    if (lhs == rhs) {
      return true;
    }
    const float tolerance =
        std::max(1.0f, std::max(std::fabs(lhs), std::fabs(rhs))) * 1e-5f;
    return std::fabs(lhs - rhs) <= tolerance;
  }

  static bool CheckQuadratic(D2D1_POINT_2F start_point,
                             const D2D1_BEZIER_SEGMENT& bezier,
                             D2D1_POINT_2F* quadratic_control) {
    float dx10 = bezier.point1.x - start_point.x;
    float dx23 = bezier.point2.x - bezier.point3.x;
    float mid_x = start_point.x + dx10 * 1.5f;
    if (!ApproximatelyEqual(mid_x, dx23 * 1.5f + bezier.point3.x)) {
      return false;
    }

    float dy10 = bezier.point1.y - start_point.y;
    float dy23 = bezier.point2.y - bezier.point3.y;
    float mid_y = start_point.y + dy10 * 1.5f;
    if (!ApproximatelyEqual(mid_y, dy23 * 1.5f + bezier.point3.y)) {
      return false;
    }

    quadratic_control->x = mid_x;
    quadratic_control->y = mid_y;
    return true;
  }

  LONG ref_count_ = 1;
  Path* path_ = nullptr;
  bool started_ = false;
  D2D1_POINT_2F current_ = {0.0f, 0.0f};
};

static uint8_t Expand3BitLuminance(uint8_t value) {
  value <<= 5;
  return value | (value >> 3) | (value >> 6);
}

static uint8_t SkiaCanonicalColorChannel(uint8_t value) {
  return Expand3BitLuminance(static_cast<uint8_t>(value >> 5));
}

static uint8_t SkiaA8LuminanceTableIndex(Color foreground_color) {
  uint8_t r = SkiaCanonicalColorChannel(ColorGetR(foreground_color));
  uint8_t g = SkiaCanonicalColorChannel(ColorGetG(foreground_color));
  uint8_t b = SkiaCanonicalColorChannel(ColorGetB(foreground_color));
  uint8_t luminance = static_cast<uint8_t>((r * 54 + g * 183 + b * 19) >> 8);
  return static_cast<uint8_t>(luminance >> 5);
}

static uint8_t FloatToByte(float value) {
  return static_cast<uint8_t>(
      std::clamp(static_cast<int>(std::lround(value)), 0, 255));
}

static float SrgbToLinear(float luminance) {
  if (luminance <= 0.04045f) {
    return luminance / 12.92f;
  }
  return std::pow((luminance + 0.055f) / 1.055f, 2.4f);
}

static float LinearToSrgb(float luma) {
  if (luma <= 0.0031308f) {
    return luma * 12.92f;
  }
  return 1.055f * std::pow(luma, 1.f / 2.4f) - 0.055f;
}

static uint8_t PremultiplyByte(uint8_t value, uint8_t alpha) {
  return static_cast<uint8_t>((static_cast<uint16_t>(value) * alpha + 127) /
                              255);
}

static void PremultiplyBGRA(uint8_t* pixels, size_t pixel_count) {
  if (!pixels) {
    return;
  }

  for (size_t i = 0; i < pixel_count; i++) {
    uint8_t* pixel = pixels + i * 4;
    uint8_t alpha = pixel[3];
    if (alpha == 255) {
      continue;
    }
    if (alpha == 0) {
      pixel[0] = 0;
      pixel[1] = 0;
      pixel[2] = 0;
      continue;
    }
    pixel[0] = PremultiplyByte(pixel[0], alpha);
    pixel[1] = PremultiplyByte(pixel[1], alpha);
    pixel[2] = PremultiplyByte(pixel[2], alpha);
  }
}

static bool CheckedImageByteSize(uint32_t width, uint32_t height,
                                 size_t* byte_size) {
  if (!byte_size || width == 0 || height == 0) {
    return false;
  }

  constexpr size_t kBytesPerPixel = 4;
  size_t w = static_cast<size_t>(width);
  size_t h = static_cast<size_t>(height);
  if (w > std::numeric_limits<size_t>::max() / h ||
      w * h > std::numeric_limits<size_t>::max() / kBytesPerPixel) {
    return false;
  }

  *byte_size = w * h * kBytesPerPixel;
  return true;
}

static bool ScaleBGRANearest(const uint8_t* src, uint32_t src_width,
                             uint32_t src_height, uint8_t* dst,
                             uint32_t dst_width, uint32_t dst_height) {
  if (!src || !dst || src_width == 0 || src_height == 0 || dst_width == 0 ||
      dst_height == 0) {
    return false;
  }

  for (uint32_t y = 0; y < dst_height; y++) {
    uint32_t src_y =
        std::min((static_cast<uint64_t>(y) * src_height) / dst_height,
                 static_cast<uint64_t>(src_height - 1));
    for (uint32_t x = 0; x < dst_width; x++) {
      uint32_t src_x =
          std::min((static_cast<uint64_t>(x) * src_width) / dst_width,
                   static_cast<uint64_t>(src_width - 1));
      std::memcpy(dst + (static_cast<size_t>(y) * dst_width + x) * 4,
                  src + (static_cast<size_t>(src_y) * src_width + src_x) * 4,
                  4);
    }
  }

  return true;
}

static bool DecodePngGlyphImageToBGRA(const void* data, size_t size,
                                      uint32_t target_width,
                                      uint32_t target_height,
                                      GlyphBitmapData* image) {
  if (!data || size == 0 || !image) {
    return false;
  }

  png_image png_image_info = {};
  png_image_info.version = PNG_IMAGE_VERSION;
  if (!png_image_begin_read_from_memory(&png_image_info, data, size)) {
    return false;
  }

  png_image_info.format = PNG_FORMAT_BGRA;
  const uint32_t src_width = png_image_info.width;
  const uint32_t src_height = png_image_info.height;
  if (src_width == 0 || src_height == 0) {
    png_image_free(&png_image_info);
    return false;
  }
  if (target_width == 0) {
    target_width = src_width;
  }
  if (target_height == 0) {
    target_height = src_height;
  }

  size_t src_byte_size = PNG_IMAGE_SIZE(png_image_info);
  if (src_byte_size == 0) {
    png_image_free(&png_image_info);
    return false;
  }
  auto* src_pixels = static_cast<uint8_t*>(std::malloc(src_byte_size));
  if (!src_pixels) {
    png_image_free(&png_image_info);
    return false;
  }

  bool ok = png_image_finish_read(&png_image_info, nullptr, src_pixels, 0,
                                  nullptr) != 0;
  png_image_free(&png_image_info);
  if (!ok) {
    std::free(src_pixels);
    return false;
  }

  PremultiplyBGRA(src_pixels, static_cast<size_t>(src_width) *
                                  static_cast<size_t>(src_height));

  if (src_width == target_width && src_height == target_height) {
    image->buffer = src_pixels;
    image->width = static_cast<float>(src_width);
    image->height = static_cast<float>(src_height);
    image->format = BitmapFormat::kBGRA8;
    image->need_free = true;
    return true;
  }

  size_t dst_byte_size = 0;
  if (!CheckedImageByteSize(target_width, target_height, &dst_byte_size)) {
    std::free(src_pixels);
    return false;
  }

  auto* dst_pixels = static_cast<uint8_t*>(std::malloc(dst_byte_size));
  if (!dst_pixels) {
    std::free(src_pixels);
    return false;
  }

  ok = ScaleBGRANearest(src_pixels, src_width, src_height, dst_pixels,
                        target_width, target_height);
  std::free(src_pixels);
  if (!ok) {
    std::free(dst_pixels);
    return false;
  }

  image->buffer = dst_pixels;
  image->width = static_cast<float>(target_width);
  image->height = static_cast<float>(target_height);
  image->format = BitmapFormat::kBGRA8;
  image->need_free = true;
  return true;
}

static std::array<std::array<uint8_t, 256>, 8> BuildSkiaMaskGammaTables() {
  std::array<std::array<uint8_t, 256>, 8> tables{};

  for (size_t table_index = 0; table_index < tables.size(); table_index++) {
    float src = Expand3BitLuminance(static_cast<uint8_t>(table_index)) / 255.f;
    float lin_src = SrgbToLinear(src);
    float dst = 1.f - src;
    float lin_dst = SrgbToLinear(dst);
    float adjusted_contrast = kSkiaMaskGammaContrast * lin_dst;

    for (size_t alpha = 0; alpha < tables[table_index].size(); alpha++) {
      float raw_src_alpha = static_cast<float>(alpha) / 255.f;
      float src_alpha = raw_src_alpha + ((1.f - raw_src_alpha) *
                                         adjusted_contrast * raw_src_alpha);
      if (std::fabs(src - dst) < (1.f / 256.f)) {
        tables[table_index][alpha] = FloatToByte(src_alpha * 255.f);
        continue;
      }

      float dst_alpha = 1.f - src_alpha;
      float lin_out = lin_src * src_alpha + dst_alpha * lin_dst;
      float out = LinearToSrgb(lin_out);
      float result = (out - dst) / (src - dst);
      tables[table_index][alpha] = FloatToByte(result * 255.f);
    }
  }

  return tables;
}

static uint8_t ApplySkiaMaskGammaToAlpha(uint8_t alpha,
                                         Color foreground_color) {
  static const auto tables = BuildSkiaMaskGammaTables();
  uint8_t table_index = SkiaA8LuminanceTableIndex(foreground_color);
  return tables[table_index][alpha];
}

float SkiaRelaxMatrixScalar(float value) {
  static constexpr float kScale = 1024.f;
  return std::round(value * kScale) / kScale;
}

Matrix22 SkiaRelaxMatrix(const Matrix22& matrix) {
  return Matrix22{SkiaRelaxMatrixScalar(matrix.GetScaleX()),
                  SkiaRelaxMatrixScalar(matrix.GetSkewX()),
                  SkiaRelaxMatrixScalar(matrix.GetSkewY()),
                  SkiaRelaxMatrixScalar(matrix.GetScaleY())};
}

class ScalerContextDWrite : public ScalerContext {
 public:
  ScalerContextDWrite(std::shared_ptr<Typeface> typeface,
                      IDWriteFactory* factory, IDWriteFontFace* font_face,
                      const ScalerContextDesc* desc)
      : ScalerContext(std::move(typeface), desc),
        factory_(RefComPtr(factory)),
        font_face_(RefComPtr(font_face)) {}

 protected:
  void GenerateMetrics(GlyphData* glyph) override {
    if (!glyph) {
      return;
    }

    if (GenerateDWriteMetrics(glyph)) {
      return;
    }

    glyph->ZeroMetrics();
    GlyphDataWinAccess::SetFormat(glyph, GlyphFormat::A8);
  }

  void GenerateImage(GlyphData* glyph, const StrokeDesc& stroke_desc) override {
    if (CanGenerateDWriteColorImage(glyph, stroke_desc) &&
        GenerateDWriteColorImage(glyph)) {
      return;
    }

    if (CanGenerateDWriteGrayImage(glyph, stroke_desc) &&
        GenerateDWriteImage(glyph)) {
      return;
    }

    (void)GenerateDWritePathImageInfo(glyph, stroke_desc, true);
  }

  void GenerateImageInfo(GlyphData* glyph,
                         const StrokeDesc& stroke_desc) override {
    if (CanGenerateDWriteColorImage(glyph, stroke_desc) &&
        GenerateDWriteColorImageInfo(glyph)) {
      return;
    }

    if (CanGenerateDWriteGrayImage(glyph, stroke_desc) &&
        GenerateDWriteImageInfo(glyph)) {
      return;
    }

    (void)GenerateDWritePathImageInfo(glyph, stroke_desc, false);
  }

  bool GeneratePath(GlyphData* glyph) override {
    if (!glyph) {
      return false;
    }

    if (font_face_->GetGlyphCount() <= glyph->Id()) {
      GlyphDataWinAccess::SetPath(glyph, Path{});
      return false;
    }

    if (GenerateDWritePath(glyph)) {
      return true;
    }

    return false;
  }

  void GenerateFontMetrics(FontMetrics* metrics) override {
    if (!metrics) {
      return;
    }

    std::memset(metrics, 0, sizeof(*metrics));

    DWRITE_FONT_METRICS dw_metrics;
    font_face_->GetMetrics(&dw_metrics);
    if (dw_metrics.designUnitsPerEm == 0) {
      return;
    }

    float scale_x = 1.0f;
    float text_size = desc_.text_size;
    Matrix22 transform;
    desc_.DecomposeMatrix(PortScaleType::kVertical, &scale_x, &text_size,
                          &transform);

    float upem = static_cast<float>(dw_metrics.designUnitsPerEm);
    metrics->ascent_ = -ScaleDWriteMetric(dw_metrics.ascent, text_size, upem);
    metrics->descent_ = ScaleDWriteMetric(dw_metrics.descent, text_size, upem);
    metrics->leading_ = ScaleDWriteMetric(dw_metrics.lineGap, text_size, upem);
    metrics->x_height_ = ScaleDWriteMetric(dw_metrics.xHeight, text_size, upem);
    metrics->cap_height_ =
        ScaleDWriteMetric(dw_metrics.capHeight, text_size, upem);
    metrics->underline_thickness_ =
        ScaleDWriteMetric(dw_metrics.underlineThickness, text_size, upem);
    metrics->underline_position_ =
        -ScaleDWriteMetric(dw_metrics.underlinePosition, text_size, upem);
    metrics->strikeout_thickness_ =
        ScaleDWriteMetric(dw_metrics.strikethroughThickness, text_size, upem);
    metrics->strikeout_position_ =
        -ScaleDWriteMetric(dw_metrics.strikethroughPosition, text_size, upem);

    ScopedComPtr<IDWriteFontFace1> font_face1;
    if (SUCCEEDED(font_face_->QueryInterface(&font_face1))) {
      DWRITE_FONT_METRICS1 dw_metrics1;
      font_face1->GetMetrics(&dw_metrics1);
      metrics->top_ =
          -ScaleDWriteMetric(dw_metrics1.glyphBoxTop, text_size, upem);
      metrics->bottom_ =
          -ScaleDWriteMetric(dw_metrics1.glyphBoxBottom, text_size, upem);
      metrics->x_min_ =
          ScaleDWriteMetric(dw_metrics1.glyphBoxLeft, text_size, upem);
      metrics->x_max_ =
          ScaleDWriteMetric(dw_metrics1.glyphBoxRight, text_size, upem);
      metrics->max_char_width_ = metrics->x_max_ - metrics->x_min_;
      return;
    }

    static constexpr FontTableTag kHeadTag = SetFourByteTag('h', 'e', 'a', 'd');
    static constexpr size_t kHeadXMinOffset = 36;
    static constexpr size_t kHeadYMinOffset = 38;
    static constexpr size_t kHeadXMaxOffset = 40;
    static constexpr size_t kHeadYMaxOffset = 42;
    static constexpr size_t kRequiredHeadSize = kHeadYMaxOffset + 2;
    AutoDWriteTable head_table(font_face_.get(), EndianSwap32(kHeadTag));
    if (head_table.exists && head_table.size >= kRequiredHeadSize) {
      int16_t x_min = ReadBigEndianInt16(head_table.data + kHeadXMinOffset);
      int16_t y_min = ReadBigEndianInt16(head_table.data + kHeadYMinOffset);
      int16_t x_max = ReadBigEndianInt16(head_table.data + kHeadXMaxOffset);
      int16_t y_max = ReadBigEndianInt16(head_table.data + kHeadYMaxOffset);
      metrics->top_ = -ScaleDWriteMetric(y_max, text_size, upem);
      metrics->bottom_ = -ScaleDWriteMetric(y_min, text_size, upem);
      metrics->x_min_ = ScaleDWriteMetric(x_min, text_size, upem);
      metrics->x_max_ = ScaleDWriteMetric(x_max, text_size, upem);
      metrics->max_char_width_ = metrics->x_max_ - metrics->x_min_;
      return;
    }

    metrics->top_ = metrics->ascent_;
    metrics->bottom_ = metrics->descent_;
  }

  uint16_t OnGetFixedSize() override { return 0; }

 private:
  bool CanGenerateDWriteColorImage(const GlyphData* glyph,
                                   const StrokeDesc& stroke_desc) {
    if (!glyph || stroke_desc.is_stroke) {
      return false;
    }

    auto format = glyph->GetFormat();
    if (desc_.fake_bold) {
      return format && *format == GlyphFormat::BGRA32;
    }
    return !format || *format == GlyphFormat::BGRA32;
  }

  bool CanGenerateDWriteGrayImage(const GlyphData* glyph,
                                  const StrokeDesc& stroke_desc) {
    if (!glyph || stroke_desc.is_stroke || desc_.fake_bold) {
      return false;
    }

    auto format = glyph->GetFormat();
    return !format || *format == GlyphFormat::A8;
  }

  void ApplyImageBoundsMetadata(GlyphBitmapData* image, const Rect& bounds) {
    if (!image || bounds.IsEmpty()) {
      return;
    }

    image->origin_x = bounds.Left();
    image->origin_y = bounds.Top();
    image->origin_x_for_raster = image->origin_x;
    image->origin_y_for_raster = image->origin_y;
    image->width = bounds.Width();
    image->height = bounds.Height();
  }

  void ApplyDWritePathImageBoundsMetadata(GlyphBitmapData* image,
                                          const Rect& bounds) {
    if (!image || bounds.IsEmpty()) {
      return;
    }

    image->origin_x = bounds.Left() / desc_.context_scale;
    image->origin_y = -bounds.Top() / desc_.context_scale;
    image->origin_x_for_raster = image->origin_x;
    image->origin_y_for_raster = bounds.Top() / desc_.context_scale;
    image->width = bounds.Width();
    image->height = bounds.Height();
  }

  void RepackGrayImageToBounds(GlyphBitmapData* image, const Rect& bounds) {
    if (!image || image->format != BitmapFormat::kGray8 ||
        image->buffer == nullptr || bounds.IsEmpty()) {
      return;
    }

    const int old_width = static_cast<int>(image->width);
    const int old_height = static_cast<int>(image->height);
    const int new_width = static_cast<int>(bounds.Width());
    const int new_height = static_cast<int>(bounds.Height());
    if (old_width <= 0 || old_height <= 0 || new_width <= 0 ||
        new_height <= 0) {
      return;
    }

    const size_t new_byte_size =
        static_cast<size_t>(new_width) * static_cast<size_t>(new_height);
    auto* buffer = static_cast<uint8_t*>(std::calloc(new_byte_size, 1));
    if (!buffer) {
      return;
    }

    const int old_left = static_cast<int>(std::lround(image->origin_x));
    const int old_top = static_cast<int>(std::lround(image->origin_y));
    const int new_left = static_cast<int>(std::lround(bounds.Left()));
    const int new_top = static_cast<int>(std::lround(bounds.Top()));

    for (int y = 0; y < old_height; y++) {
      const int target_y = old_top + y - new_top;
      if (target_y < 0 || target_y >= new_height) {
        continue;
      }
      for (int x = 0; x < old_width; x++) {
        const int target_x = old_left + x - new_left;
        if (target_x < 0 || target_x >= new_width) {
          continue;
        }
        buffer[target_y * new_width + target_x] =
            image->buffer[y * old_width + x];
      }
    }

    if (image->need_free) {
      std::free(image->buffer);
    }
    image->buffer = buffer;
    image->need_free = true;
    ApplyImageBoundsMetadata(image, bounds);
  }

  size_t BytesPerPixel(BitmapFormat format) const {
    switch (format) {
      case BitmapFormat::kGray8:
        return 1;
      case BitmapFormat::kBGRA8:
      case BitmapFormat::kRGBA8:
        return 4;
      case BitmapFormat::kUnknown:
        return 0;
    }
    return 0;
  }

  bool AllocateImageBuffer(GlyphBitmapData* image) const {
    if (!image || image->buffer != nullptr) {
      return image && image->buffer != nullptr;
    }

    const size_t bytes_per_pixel = BytesPerPixel(image->format);
    const size_t width =
        image->width > 0.0f ? static_cast<size_t>(image->width) : 0;
    const size_t height =
        image->height > 0.0f ? static_cast<size_t>(image->height) : 0;
    if (bytes_per_pixel == 0 || width == 0 || height == 0) {
      return false;
    }

    if (width > std::numeric_limits<size_t>::max() / height ||
        width * height > std::numeric_limits<size_t>::max() / bytes_per_pixel) {
      return false;
    }

    const size_t byte_size = width * height * bytes_per_pixel;
    auto* buffer = static_cast<uint8_t*>(std::calloc(byte_size, 1));
    if (!buffer) {
      return false;
    }

    image->buffer = buffer;
    image->need_free = true;
    return true;
  }

  bool GenerateDWriteStrokePath(const Path& path, const StrokeDesc& stroke_desc,
                                float stroke_width, Path* stroke_path) {
    if (!stroke_path || path.IsEmpty()) {
      return false;
    }

    Paint paint;
    paint.SetStyle(Paint::kStroke_Style);
    paint.SetStrokeWidth(stroke_width);
    paint.SetStrokeCap(stroke_desc.cap);
    paint.SetStrokeJoin(stroke_desc.join);
    paint.SetStrokeMiter(stroke_desc.miter_limit);

    Path quad_path;
    Stroke stroke(paint);
    stroke.QuadPath(path, &quad_path);
    stroke.StrokePath(quad_path, stroke_path);
    return !stroke_path->IsEmpty();
  }

  bool RasterDWritePathImage(GlyphBitmapData* image, const Rect& bounds,
                             const Path* fill_path, const Path* stroke_path) {
    if (!image || bounds.IsEmpty()) {
      return false;
    }

    const uint32_t width = static_cast<uint32_t>(image->width);
    const uint32_t height = static_cast<uint32_t>(image->height);
    const size_t byte_size = static_cast<size_t>(width) * height;
    if (width == 0 || height == 0 || byte_size == 0) {
      return false;
    }

    Bitmap bitmap(width, height, AlphaType::kPremul_AlphaType, ColorType::kA8);
    auto canvas = Canvas::MakeSoftwareCanvas(&bitmap);
    if (!canvas || !bitmap.GetPixelAddr()) {
      return false;
    }

    Paint paint;
    paint.SetAntiAlias(true);
    paint.SetStyle(Paint::kFill_Style);
    paint.SetFillColor(Color_WHITE);

    canvas->Translate(-bounds.Left(), -bounds.Top());
    if (fill_path && !fill_path->IsEmpty()) {
      canvas->DrawPath(*fill_path, paint);
    }
    if (stroke_path && !stroke_path->IsEmpty()) {
      canvas->DrawPath(*stroke_path, paint);
    }

    uint8_t* buffer = reinterpret_cast<uint8_t*>(std::malloc(byte_size));
    if (!buffer) {
      return false;
    }

    if (bitmap.RowBytes() == width) {
      std::memcpy(buffer, bitmap.GetPixelAddr(), byte_size);
    } else {
      for (uint32_t y = 0; y < height; y++) {
        std::memcpy(
            buffer + static_cast<size_t>(y) * width,
            bitmap.GetPixelAddr() + static_cast<size_t>(y) * bitmap.RowBytes(),
            width);
      }
    }

    image->buffer = buffer;
    image->need_free = true;
    return true;
  }

  bool GenerateDWritePathImageInfo(GlyphData* glyph,
                                   const StrokeDesc& stroke_desc,
                                   bool allocate_buffer) {
    if (!glyph || (!stroke_desc.is_stroke && !desc_.fake_bold)) {
      return false;
    }

    float text_size = desc_.text_size;
    Matrix22 transform;
    if (!GetDWriteImageTransform(&text_size, &transform)) {
      return false;
    }

    Rect bounds;
    Path fill_path;
    Path stroke_path;
    if (desc_.fake_bold) {
      if (!GenerateDWriteOutlinePath(glyph->Id(), text_size, transform,
                                     &fill_path) ||
          fill_path.IsEmpty()) {
        return false;
      }

      StrokeDesc fake_bold_stroke = stroke_desc;
      float stroke_width = text_size * ComputeFakeBoldScale(text_size);
      if (stroke_desc.is_stroke) {
        float stroke_scale =
            desc_.text_size > 0.0f ? text_size / desc_.text_size : 1.0f;
        stroke_width = stroke_desc.stroke_width * stroke_scale;
      } else {
        fake_bold_stroke.cap = Paint::Cap::kDefault_Cap;
        fake_bold_stroke.join = Paint::Join::kDefault_Join;
        fake_bold_stroke.miter_limit = Paint::kDefaultMiterLimit;
      }
      if (!GenerateDWriteStrokePath(fill_path, fake_bold_stroke, stroke_width,
                                    &stroke_path)) {
        return false;
      }

      bounds = stroke_path.GetBounds();
      if (!stroke_desc.is_stroke) {
        bounds.Join(fill_path.GetBounds());
      }
      bounds.SetLTRB(std::floor(bounds.Left()), std::floor(bounds.Top()),
                     std::ceil(bounds.Right()), std::ceil(bounds.Bottom()));
      bounds.SetBottom(bounds.Bottom() + 1.0f);
    } else {
      if (!GenerateDWriteOutlinePath(glyph->Id(), text_size, transform,
                                     &fill_path) ||
          fill_path.IsEmpty()) {
        return false;
      }

      float stroke_scale =
          desc_.text_size > 0.0f ? text_size / desc_.text_size : 1.0f;
      if (!GenerateDWriteStrokePath(fill_path, stroke_desc,
                                    stroke_desc.stroke_width * stroke_scale,
                                    &stroke_path)) {
        return false;
      }

      bounds = stroke_path.GetBounds();
      bounds.SetLTRB(std::floor(bounds.Left()), std::floor(bounds.Top()),
                     std::ceil(bounds.Right()), std::ceil(bounds.Bottom()));
    }

    GlyphBitmapData image;
    image.format = BitmapFormat::kGray8;
    ApplyDWritePathImageBoundsMetadata(&image, bounds);
    const Path* raster_fill_path =
        desc_.fake_bold && !stroke_desc.is_stroke ? &fill_path : nullptr;
    if (allocate_buffer &&
        !RasterDWritePathImage(&image, bounds, raster_fill_path,
                               &stroke_path)) {
      return false;
    }
    GlyphDataWinAccess::SetImage(glyph, image);
    return true;
  }

  DWRITE_MATRIX ToDWriteMatrix(const Matrix22& transform) {
    DWRITE_MATRIX matrix;
    matrix.m11 = transform.GetScaleX();
    matrix.m12 = transform.GetSkewY();
    matrix.m21 = transform.GetSkewX();
    matrix.m22 = transform.GetScaleY();
    matrix.dx = 0.0f;
    matrix.dy = 0.0f;
    return matrix;
  }

  void DecomposeDWriteImageMatrix(float* scale_x, float* scale_y,
                                  Matrix22* transform) {
    Matrix22 total =
        SkiaRelaxMatrix(desc_.GetTransformMatrix()) * desc_.GetLocalMatrix();

    bool only_scale =
        FloatNearlyZero(total.GetSkewX()) && FloatNearlyZero(total.GetSkewY());
    if (only_scale) {
      if (FloatNearlyZero(total.GetScaleX()) ||
          FloatNearlyZero(total.GetScaleY())) {
        *scale_x = 1.f;
        *scale_y = 1.f;
        *transform = Matrix22{0.f, 0.f, 0.f, 0.f};
        return;
      }
      if (!FloatNearlyZero(total.GetScaleX() - total.GetScaleY())) {
        *scale_y = std::fabs(total.GetScaleY());
        *scale_x = *scale_y;
        *transform = Matrix22{total.GetScaleX() / *scale_x, 0.f, 0.f,
                              total.GetScaleY() < 0.f ? -1.f : 1.f};
      } else {
        *scale_x = std::fabs(total.GetScaleX());
        *scale_y = std::fabs(total.GetScaleY());
        *transform = Matrix22{total.GetScaleX() < 0.f ? -1.f : 1.f, 0.f, 0.f,
                              total.GetScaleY() < 0.f ? -1.f : 1.f};
      }
      return;
    }

    Matrix22 q, r;
    total.QRDecompose(&q, &r);
    if (FloatNearlyZero(r.GetScaleX()) || FloatNearlyZero(r.GetScaleY())) {
      *scale_x = 1.f;
      *scale_y = 1.f;
      *transform = Matrix22{0.f, 0.f, 0.f, 0.f};
      return;
    }
    *scale_y = std::fabs(r.GetScaleY());
    *scale_x = *scale_y;
    *transform = total * Matrix22(1 / *scale_x, 0, 0, 1 / *scale_y);
  }

  bool GetDWriteImageTransform(float* text_size, Matrix22* transform) {
    float scale_x = 1.0f;
    DecomposeDWriteImageMatrix(&scale_x, text_size, transform);
    *text_size *= desc_.context_scale;
    return *text_size > 0.0f;
  }

  bool CreateDWriteGrayGlyphRunAnalysis(
      UINT16 glyph_id, float text_size, const Matrix22& transform,
      IDWriteGlyphRunAnalysis** glyph_run_analysis) {
    DWRITE_MATRIX matrix = ToDWriteMatrix(transform);

    FLOAT advance = 0.0f;
    DWRITE_GLYPH_OFFSET offset;
    offset.advanceOffset = 0.0f;
    offset.ascenderOffset = 0.0f;

    DWRITE_GLYPH_RUN run;
    run.fontFace = font_face_.get();
    run.fontEmSize = text_size;
    run.glyphCount = 1;
    run.glyphIndices = &glyph_id;
    run.glyphAdvances = &advance;
    run.glyphOffsets = &offset;
    run.isSideways = FALSE;
    run.bidiLevel = 0;

    ScopedComPtr<IDWriteFactory2> factory2;
    ScopedComPtr<IDWriteFontFace2> font_face2;
    if (SUCCEEDED(factory_->QueryInterface(&factory2)) &&
        SUCCEEDED(font_face_->QueryInterface(&font_face2))) {
      return SUCCEEDED(factory2->CreateGlyphRunAnalysis(
          &run, &matrix, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
          DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_ENABLED,
          DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 0.0f, 0.0f,
          glyph_run_analysis));
    }

    return SUCCEEDED(factory_->CreateGlyphRunAnalysis(
        &run, 1.0f, &matrix, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
        DWRITE_MEASURING_MODE_NATURAL, 0.0f, 0.0f, glyph_run_analysis));
  }

  bool CreateDWriteColorGlyphRunEnumerator(
      UINT16 glyph_id, float text_size, const Matrix22& transform,
      IDWriteColorGlyphRunEnumerator** color_layers) {
    ScopedComPtr<IDWriteFactory2> factory2;
    if (FAILED(factory_->QueryInterface(&factory2))) {
      return false;
    }

    DWRITE_MATRIX matrix = ToDWriteMatrix(transform);

    FLOAT advance = 0.0f;
    DWRITE_GLYPH_OFFSET offset;
    offset.advanceOffset = 0.0f;
    offset.ascenderOffset = 0.0f;

    DWRITE_GLYPH_RUN run;
    run.fontFace = font_face_.get();
    run.fontEmSize = text_size;
    run.glyphCount = 1;
    run.glyphIndices = &glyph_id;
    run.glyphAdvances = &advance;
    run.glyphOffsets = &offset;
    run.isSideways = FALSE;
    run.bidiLevel = 0;

    HRESULT hr = factory2->TranslateColorGlyphRun(0.0f, 0.0f, &run, nullptr,
                                                  DWRITE_MEASURING_MODE_NATURAL,
                                                  &matrix, 0, color_layers);
    return hr != DWRITE_E_NOCOLOR && SUCCEEDED(hr) && *color_layers;
  }

  bool GenerateDWriteColorLayerPath(const DWRITE_COLOR_GLYPH_RUN* color_run,
                                    Path* path) {
    if (!color_run || !path) {
      return false;
    }

    const DWRITE_GLYPH_RUN& run = color_run->glyphRun;
    ScopedComPtr<IDWriteGeometrySink> geometry_sink(new DWritePathSink(path));
    HRESULT hr = run.fontFace->GetGlyphRunOutline(
        run.fontEmSize, run.glyphIndices, run.glyphAdvances, run.glyphOffsets,
        run.glyphCount, run.isSideways, run.bidiLevel % 2, geometry_sink.get());
    return SUCCEEDED(hr);
  }

  bool GenerateDWriteColorLayerBounds(UINT16 glyph_id, float text_size,
                                      const Matrix22& transform, Rect* bounds) {
    ScopedComPtr<IDWriteColorGlyphRunEnumerator> color_layers;
    if (!CreateDWriteColorGlyphRunEnumerator(glyph_id, text_size, transform,
                                             &color_layers)) {
      return false;
    }

    bounds->SetEmpty();
    BOOL has_color_layer = FALSE;
    while (SUCCEEDED(color_layers->MoveNext(&has_color_layer)) &&
           has_color_layer) {
      const DWRITE_COLOR_GLYPH_RUN* color_run = nullptr;
      if (FAILED(color_layers->GetCurrentRun(&color_run))) {
        return false;
      }

      Path path;
      if (!GenerateDWriteColorLayerPath(color_run, &path)) {
        return false;
      }
      bounds->Join(path.GetBounds());
    }

    if (bounds->IsEmpty()) {
      return false;
    }

    if (!transform.IsIdentity()) {
      *bounds = transform.ToMatrix().MapRect(*bounds);
    }

    return !bounds->IsEmpty();
  }

  bool GenerateDWritePaintGlyphBounds(UINT32 glyph_index, float text_size,
                                      const Matrix& ctm, Rect* bounds) {
    if (glyph_index > std::numeric_limits<UINT16>::max()) {
      return false;
    }

    UINT16 glyph_id = static_cast<UINT16>(glyph_index);
    Path path;
    ScopedComPtr<IDWriteGeometrySink> geometry_sink(new DWritePathSink(&path));
    if (FAILED(font_face_->GetGlyphRunOutline(text_size, &glyph_id, nullptr,
                                              nullptr, 1, FALSE, FALSE,
                                              geometry_sink.get()))) {
      return false;
    }

    Matrix path_matrix = ctm;
    path_matrix.PreScale(1.0f / text_size, 1.0f / text_size);
    if (!path_matrix.IsIdentity()) {
      path = path.CopyWithMatrix(path_matrix);
    }
    bounds->Join(path.GetBounds());
    return true;
  }

  bool SetDWriteColorImageInfoFromBounds(GlyphData* glyph, const Rect& bounds) {
    const float left = std::floor(bounds.Left());
    const float top = std::floor(bounds.Top());
    const float right = std::ceil(bounds.Right());
    const float bottom = std::ceil(bounds.Bottom());
    if (left >= right || top >= bottom) {
      return false;
    }

    GlyphBitmapData image;
    image.origin_x = left / desc_.context_scale;
    image.origin_y = -top / desc_.context_scale;
    image.origin_x_for_raster = image.origin_x;
    image.origin_y_for_raster = top / desc_.context_scale;
    image.width = right - left;
    image.height = bottom - top;
    image.format = BitmapFormat::kBGRA8;
    GlyphDataWinAccess::SetImage(glyph, image);
    return true;
  }

#ifdef DWRITE_GLYPH_IMAGE_FORMATS_COLR_PAINT_TREE_DEFINED
  bool GenerateDWriteColorV1PaintBounds(IDWritePaintReader* paint_reader,
                                        const DWRITE_PAINT_ELEMENT& element,
                                        float text_size, Matrix* ctm,
                                        Rect* bounds) {
    auto bound_children = [&](UINT32 child_count) {
      if (child_count == 0) {
        return true;
      }

      DWRITE_PAINT_ELEMENT child;
      if (FAILED(paint_reader->MoveToFirstChild(&child))) {
        return false;
      }

      bool ok = GenerateDWriteColorV1PaintBounds(paint_reader, child, text_size,
                                                 ctm, bounds);
      for (UINT32 i = 1; ok && i < child_count; ++i) {
        if (FAILED(paint_reader->MoveToNextSibling(&child))) {
          ok = false;
          break;
        }
        ok = GenerateDWriteColorV1PaintBounds(paint_reader, child, text_size,
                                              ctm, bounds);
      }

      return SUCCEEDED(paint_reader->MoveToParent()) && ok;
    };

    Matrix restore_matrix = *ctm;
    switch (element.paintType) {
      case DWRITE_PAINT_TYPE_NONE:
        return false;
      case DWRITE_PAINT_TYPE_LAYERS:
        return bound_children(element.paint.layers.childCount);
      case DWRITE_PAINT_TYPE_SOLID_GLYPH:
        return GenerateDWritePaintGlyphBounds(
            element.paint.solidGlyph.glyphIndex, text_size, *ctm, bounds);
      case DWRITE_PAINT_TYPE_SOLID:
      case DWRITE_PAINT_TYPE_LINEAR_GRADIENT:
      case DWRITE_PAINT_TYPE_RADIAL_GRADIENT:
      case DWRITE_PAINT_TYPE_SWEEP_GRADIENT:
        return true;
      case DWRITE_PAINT_TYPE_GLYPH:
        return GenerateDWritePaintGlyphBounds(element.paint.glyph.glyphIndex,
                                              text_size, *ctm, bounds);
      case DWRITE_PAINT_TYPE_COLOR_GLYPH:
        if (!DWriteRectIsEmpty(element.paint.colorGlyph.clipBox)) {
          Rect clip_bounds =
              ctm->MapRect(DWriteRectToRect(element.paint.colorGlyph.clipBox));
          bounds->Join(clip_bounds);
          return true;
        }
        return bound_children(1);
      case DWRITE_PAINT_TYPE_TRANSFORM:
        ctm->PreConcat(DWriteMatrixToMatrix(element.paint.transform));
        {
          bool ok = bound_children(1);
          *ctm = restore_matrix;
          return ok;
        }
      case DWRITE_PAINT_TYPE_COMPOSITE:
        return bound_children(2);
      default:
        return false;
    }
  }
#endif

  bool GenerateDWriteColorV1Bounds(UINT16 glyph_id, float text_size,
                                   const Matrix22& transform, Rect* bounds) {
#ifdef DWRITE_GLYPH_IMAGE_FORMATS_COLR_PAINT_TREE_DEFINED
    ScopedComPtr<IDWriteFontFace7> font_face7;
    if (FAILED(font_face_->QueryInterface(&font_face7))) {
      return false;
    }

    ScopedComPtr<IDWritePaintReader> paint_reader;
    if (FAILED(font_face7->CreatePaintReader(
            DWRITE_GLYPH_IMAGE_FORMATS_COLR_PAINT_TREE,
            DWRITE_PAINT_FEATURE_LEVEL_COLR_V1, &paint_reader))) {
      return false;
    }

    DWRITE_PAINT_ELEMENT paint_element;
    D2D_RECT_F clip_box;
    DWRITE_PAINT_ATTRIBUTES attributes;
    if (FAILED(paint_reader->SetCurrentGlyph(glyph_id, &paint_element,
                                             &clip_box, &attributes)) ||
        paint_element.paintType == DWRITE_PAINT_TYPE_NONE) {
      return false;
    }

    Matrix matrix = transform.ToMatrix();
    matrix.PreScale(text_size, text_size);

    if (!DWriteRectIsEmpty(clip_box)) {
      *bounds = matrix.MapRect(DWriteRectToRect(clip_box));
    } else if (!GenerateDWriteColorV1PaintBounds(paint_reader.get(),
                                                 paint_element, text_size,
                                                 &matrix, bounds)) {
      return false;
    }

    return !bounds->IsEmpty();
#else
    return false;
#endif
  }

  bool GenerateDWritePaintGlyphPath(UINT32 glyph_index, float text_size,
                                    Path* path) {
    if (!path || glyph_index > std::numeric_limits<UINT16>::max()) {
      return false;
    }

    UINT16 glyph_id = static_cast<UINT16>(glyph_index);
    ScopedComPtr<IDWriteGeometrySink> geometry_sink(new DWritePathSink(path));
    if (FAILED(font_face_->GetGlyphRunOutline(text_size, &glyph_id, nullptr,
                                              nullptr, 1, FALSE, FALSE,
                                              geometry_sink.get()))) {
      return false;
    }

    Matrix path_matrix;
    path_matrix.PreScale(1.0f / text_size, 1.0f / text_size);
    *path = path->CopyWithMatrix(path_matrix);
    return true;
  }

#ifdef DWRITE_GLYPH_IMAGE_FORMATS_COLR_PAINT_TREE_DEFINED
  bool FetchDWriteGradientStops(IDWritePaintReader* paint_reader,
                                UINT32 stop_count, std::vector<Color4f>* colors,
                                std::vector<float>* positions) {
    if (!paint_reader || !colors || !positions || stop_count == 0) {
      return false;
    }

    std::vector<D2D1_GRADIENT_STOP> stops(stop_count);
    if (FAILED(paint_reader->GetGradientStops(0, stop_count, stops.data()))) {
      return false;
    }

    colors->clear();
    positions->clear();
    colors->reserve(stop_count);
    positions->reserve(stop_count);
    for (const auto& stop : stops) {
      colors->push_back(
          {stop.color.r, stop.color.g, stop.color.b, stop.color.a});
      positions->push_back(stop.position);
    }
    return true;
  }

  bool ConfigureDWriteLinearGradientPaint(IDWritePaintReader* paint_reader,
                                          const DWRITE_PAINT_ELEMENT& element,
                                          Paint* paint, bool* draws_content) {
    *draws_content = false;
    const auto& gradient = element.paint.linearGradient;
    std::vector<Color4f> colors;
    std::vector<float> positions;
    if (!FetchDWriteGradientStops(paint_reader, gradient.gradientStopCount,
                                  &colors, &positions)) {
      return false;
    }

    if (colors.size() == 1) {
      paint->SetColor(Color4fToColor(colors.front()));
      *draws_content = Color4fHasAlpha(colors.front());
      return true;
    }

    Vec2 p0{gradient.x0, gradient.y0};
    Vec2 p1{gradient.x1, gradient.y1};
    Vec2 p2{gradient.x2, gradient.y2};
    if (p1 == p0 || p2 == p0 || Vec2Cross(p1 - p0, p2 - p0) == 0.0f) {
      paint->SetColor(Color4fToColor(colors.front()));
      *draws_content = Color4fHasAlpha(colors.front());
      return true;
    }

    Vec2 p0p2 = p2 - p0;
    Vec2 perpendicular{p0p2.y, -p0p2.x};
    Vec2 p3 = p0 + Vec2Projection(p1 - p0, perpendicular);

    float stop_range = positions.back() - positions.front();
    if (stop_range == 0.0f) {
      if (gradient.extendMode != D2D1_EXTEND_MODE_CLAMP) {
        paint->SetColor(Color_TRANSPARENT);
        return true;
      }
      positions.push_back(positions.back() + 1.0f);
      colors.push_back(colors.back());
      stop_range = 1.0f;
    }

    if (stop_range != 1.0f || positions.front() != 0.0f) {
      Vec2 start_to_end = p3 - p0;
      Vec2 start_offset = start_to_end;
      start_offset.x *= positions.front();
      start_offset.y *= positions.front();
      Vec2 end_offset = start_to_end;
      end_offset.x *= positions.back();
      end_offset.y *= positions.back();
      Vec2 start = p0 + start_offset;
      Vec2 end = p0 + end_offset;
      float stop_start = positions.front();
      float scale = 1.0f / stop_range;
      p0 = start;
      p3 = end;
      for (auto& position : positions) {
        position = (position - stop_start) * scale;
      }
    }

    Point line_points[2] = {Point(p0.x, p0.y, 0, 1), Point(p3.x, p3.y, 0, 1)};
    auto shader = Shader::MakeLinear(
        line_points, colors.data(), positions.data(),
        static_cast<int>(positions.size()),
        DWriteExtendModeToTileMode(
            static_cast<D2D1_EXTEND_MODE>(gradient.extendMode)));
    if (!shader) {
      return false;
    }
    paint->SetColor(Color_BLACK);
    paint->SetShader(std::move(shader));
    *draws_content = true;
    return true;
  }

  bool ConfigureDWriteRadialGradientPaint(IDWritePaintReader* paint_reader,
                                          const DWRITE_PAINT_ELEMENT& element,
                                          Paint* paint, bool* draws_content) {
    *draws_content = false;
    const auto& gradient = element.paint.radialGradient;
    std::vector<Color4f> colors;
    std::vector<float> positions;
    if (!FetchDWriteGradientStops(paint_reader, gradient.gradientStopCount,
                                  &colors, &positions)) {
      return false;
    }

    if (colors.size() == 1) {
      paint->SetColor(Color4fToColor(colors.front()));
      *draws_content = Color4fHasAlpha(colors.front());
      return true;
    }

    Point start{gradient.x0, gradient.y0, 0, 1};
    Point end{gradient.x1, gradient.y1, 0, 1};
    float start_radius = gradient.radius0;
    float end_radius = gradient.radius1;
    float stop_range = positions.back() - positions.front();
    if (stop_range == 0.0f) {
      if (gradient.extendMode != D2D1_EXTEND_MODE_CLAMP) {
        paint->SetColor(Color_TRANSPARENT);
        return true;
      }
      positions.push_back(positions.back() + 1.0f);
      colors.push_back(colors.back());
      stop_range = 1.0f;
    }

    if (stop_range != 1.0f || positions.front() != 0.0f) {
      Vec2 start_to_end = Vec2{end - start};
      float radius_diff = end_radius - start_radius;
      Vec2 start_offset = start_to_end;
      start_offset.x *= positions.front();
      start_offset.y *= positions.front();
      Vec2 end_offset = start_to_end;
      end_offset.x *= positions.back();
      end_offset.y *= positions.back();

      end = Point(start.x + end_offset.x, start.y + end_offset.y, 0, 1);
      start = Point(start.x + start_offset.x, start.y + start_offset.y, 0, 1);
      end_radius = start_radius + radius_diff * positions.back();
      start_radius = start_radius + radius_diff * positions.front();

      float stop_start = positions.front();
      float scale = 1.0f / stop_range;
      for (auto& position : positions) {
        position = (position - stop_start) * scale;
      }
    }

    if (start_radius < 0.0f || end_radius < 0.0f) {
      paint->SetColor(Color_TRANSPARENT);
      return true;
    }

    auto shader = Shader::MakeTwoPointConical(
        start, start_radius, end, end_radius, colors.data(), positions.data(),
        static_cast<int>(positions.size()),
        DWriteExtendModeToTileMode(
            static_cast<D2D1_EXTEND_MODE>(gradient.extendMode)));
    if (!shader) {
      return false;
    }
    paint->SetColor(Color_BLACK);
    paint->SetShader(std::move(shader));
    *draws_content = true;
    return true;
  }

  bool ConfigureDWriteSweepGradientPaint(IDWritePaintReader* paint_reader,
                                         const DWRITE_PAINT_ELEMENT& element,
                                         Paint* paint, bool* draws_content) {
    *draws_content = false;
    const auto& gradient = element.paint.sweepGradient;
    std::vector<Color4f> colors;
    std::vector<float> positions;
    if (!FetchDWriteGradientStops(paint_reader, gradient.gradientStopCount,
                                  &colors, &positions)) {
      return false;
    }

    if (colors.size() == 1) {
      paint->SetColor(Color4fToColor(colors.front()));
      *draws_content = Color4fHasAlpha(colors.front());
      return true;
    }

    float start_angle = gradient.startAngle;
    float end_angle = gradient.endAngle;
    float sector_angle = end_angle - start_angle;
    if (sector_angle == 0.0f && gradient.extendMode != D2D1_EXTEND_MODE_CLAMP) {
      paint->SetColor(Color_TRANSPARENT);
      return true;
    }

    float scaled_start = start_angle + sector_angle * positions.front();
    float scaled_end = start_angle + sector_angle * positions.back();
    float stop_range = positions.back() - positions.front();
    if (stop_range == 0.0f) {
      if (gradient.extendMode != D2D1_EXTEND_MODE_CLAMP) {
        paint->SetColor(Color_TRANSPARENT);
        return true;
      }
      positions.push_back(positions.back() + 1.0f);
      colors.push_back(colors.back());
      stop_range = 1.0f;
    }

    float stop_start = positions.front();
    float scale = 1.0f / stop_range;
    for (auto& position : positions) {
      position = (position - stop_start) * scale;
    }

    scaled_start = 360.0f - scaled_start;
    scaled_end = 360.0f - scaled_end;
    if (scaled_start >= scaled_end) {
      std::swap(scaled_start, scaled_end);
      std::reverse(colors.begin(), colors.end());
      std::reverse(positions.begin(), positions.end());
      for (auto& position : positions) {
        position = 1.0f - position;
      }
    }

    auto shader = Shader::MakeSweep(
        gradient.centerX, gradient.centerY, scaled_start, scaled_end,
        colors.data(), positions.data(), static_cast<int>(positions.size()),
        DWriteExtendModeToTileMode(
            static_cast<D2D1_EXTEND_MODE>(gradient.extendMode)));
    if (!shader) {
      return false;
    }
    paint->SetColor(Color_BLACK);
    paint->SetShader(std::move(shader));
    *draws_content = true;
    return true;
  }

  bool DrawDWriteColorV1Children(IDWritePaintReader* paint_reader,
                                 UINT32 child_count, float text_size,
                                 Canvas* canvas, bool* did_draw) {
    if (child_count == 0) {
      return true;
    }

    DWRITE_PAINT_ELEMENT child;
    if (FAILED(paint_reader->MoveToFirstChild(&child))) {
      return false;
    }

    bool ok = DrawDWriteColorV1Paint(paint_reader, child, text_size, canvas,
                                     did_draw);
    for (UINT32 i = 1; ok && i < child_count; ++i) {
      if (FAILED(paint_reader->MoveToNextSibling(&child))) {
        ok = false;
        break;
      }
      ok = DrawDWriteColorV1Paint(paint_reader, child, text_size, canvas,
                                  did_draw);
    }

    return SUCCEEDED(paint_reader->MoveToParent()) && ok;
  }

  bool DrawDWriteColorV1Paint(IDWritePaintReader* paint_reader,
                              const DWRITE_PAINT_ELEMENT& element,
                              float text_size, Canvas* canvas, bool* did_draw) {
    AutoCanvasRestore restore(canvas, true);
    switch (element.paintType) {
      case DWRITE_PAINT_TYPE_NONE:
        return true;
      case DWRITE_PAINT_TYPE_LAYERS:
        return DrawDWriteColorV1Children(paint_reader,
                                         element.paint.layers.childCount,
                                         text_size, canvas, did_draw);
      case DWRITE_PAINT_TYPE_SOLID_GLYPH: {
        Path path;
        if (!GenerateDWritePaintGlyphPath(element.paint.solidGlyph.glyphIndex,
                                          text_size, &path)) {
          return false;
        }
        Paint paint;
        paint.SetAntiAlias(true);
        paint.SetColor(
            DWriteColorToColor(element.paint.solidGlyph.color.value));
        canvas->DrawPath(path, paint);
        *did_draw = true;
        return true;
      }
      case DWRITE_PAINT_TYPE_SOLID: {
        Paint paint;
        paint.SetColor(DWriteColorToColor(element.paint.solid.value));
        canvas->DrawPaint(paint);
        *did_draw = true;
        return true;
      }
      case DWRITE_PAINT_TYPE_LINEAR_GRADIENT: {
        Paint paint;
        bool draws_content = false;
        if (!ConfigureDWriteLinearGradientPaint(paint_reader, element, &paint,
                                                &draws_content)) {
          return false;
        }
        if (draws_content) {
          canvas->DrawPaint(paint);
          *did_draw = true;
        }
        return true;
      }
      case DWRITE_PAINT_TYPE_RADIAL_GRADIENT: {
        Paint paint;
        bool draws_content = false;
        if (!ConfigureDWriteRadialGradientPaint(paint_reader, element, &paint,
                                                &draws_content)) {
          return false;
        }
        if (draws_content) {
          canvas->DrawPaint(paint);
          *did_draw = true;
        }
        return true;
      }
      case DWRITE_PAINT_TYPE_SWEEP_GRADIENT: {
        Paint paint;
        bool draws_content = false;
        if (!ConfigureDWriteSweepGradientPaint(paint_reader, element, &paint,
                                               &draws_content)) {
          return false;
        }
        if (draws_content) {
          canvas->DrawPaint(paint);
          *did_draw = true;
        }
        return true;
      }
      case DWRITE_PAINT_TYPE_GLYPH: {
        Path path;
        if (!GenerateDWritePaintGlyphPath(element.paint.glyph.glyphIndex,
                                          text_size, &path)) {
          return false;
        }
        canvas->ClipPath(path);
        return DrawDWriteColorV1Children(paint_reader, 1, text_size, canvas,
                                         did_draw);
      }
      case DWRITE_PAINT_TYPE_COLOR_GLYPH: {
        const auto& color_glyph = element.paint.colorGlyph;
        if (!DWriteRectIsEmpty(color_glyph.clipBox)) {
          canvas->ClipRect(DWriteRectToRect(color_glyph.clipBox));
        }
        return DrawDWriteColorV1Children(paint_reader, 1, text_size, canvas,
                                         did_draw);
      }
      case DWRITE_PAINT_TYPE_TRANSFORM:
        canvas->Concat(DWriteMatrixToMatrix(element.paint.transform));
        return DrawDWriteColorV1Children(paint_reader, 1, text_size, canvas,
                                         did_draw);
      case DWRITE_PAINT_TYPE_COMPOSITE: {
        DWRITE_PAINT_ELEMENT source;
        DWRITE_PAINT_ELEMENT backdrop;
        if (FAILED(paint_reader->MoveToFirstChild(&source)) ||
            FAILED(paint_reader->MoveToNextSibling(&backdrop))) {
          return false;
        }

        canvas->SaveLayer(canvas->GetLocalClipBounds(), Paint{});
        bool ok = DrawDWriteColorV1Paint(paint_reader, backdrop, text_size,
                                         canvas, did_draw);
        if (FAILED(paint_reader->MoveToParent()) || !ok ||
            FAILED(paint_reader->MoveToFirstChild(&source))) {
          return false;
        }

        Paint blend_paint;
        blend_paint.SetBlendMode(
            DWriteCompositeModeToBlendMode(element.paint.composite.mode));
        canvas->SaveLayer(canvas->GetLocalClipBounds(), blend_paint);
        ok = DrawDWriteColorV1Paint(paint_reader, source, text_size, canvas,
                                    did_draw);
        return SUCCEEDED(paint_reader->MoveToParent()) && ok;
      }
      default:
        return false;
    }
  }
#endif

  bool DrawDWriteColorV1Image(UINT16 glyph_id, float text_size,
                              const Matrix22& transform, Canvas* canvas) {
#ifdef DWRITE_GLYPH_IMAGE_FORMATS_COLR_PAINT_TREE_DEFINED
    ScopedComPtr<IDWriteFontFace7> font_face7;
    if (!canvas || FAILED(font_face_->QueryInterface(&font_face7))) {
      return false;
    }

    ScopedComPtr<IDWritePaintReader> paint_reader;
    if (FAILED(font_face7->CreatePaintReader(
            DWRITE_GLYPH_IMAGE_FORMATS_COLR_PAINT_TREE,
            DWRITE_PAINT_FEATURE_LEVEL_COLR_V1, &paint_reader))) {
      return false;
    }

    DWRITE_PAINT_ELEMENT paint_element;
    D2D_RECT_F clip_box;
    DWRITE_PAINT_ATTRIBUTES attributes;
    if (FAILED(paint_reader->SetCurrentGlyph(glyph_id, &paint_element,
                                             &clip_box, &attributes)) ||
        paint_element.paintType == DWRITE_PAINT_TYPE_NONE) {
      return false;
    }

    Matrix matrix = transform.ToMatrix();
    matrix.PreScale(text_size, text_size);
    canvas->Concat(matrix);
    if (!DWriteRectIsEmpty(clip_box)) {
      canvas->ClipRect(DWriteRectToRect(clip_box));
    }

    paint_reader->SetTextColor(ColorToDWriteColor(desc_.foreground_color));

    bool did_draw = false;
    return DrawDWriteColorV1Paint(paint_reader.get(), paint_element, text_size,
                                  canvas, &did_draw) &&
           did_draw;
#else
    return false;
#endif
  }

  bool GenerateDWritePngImageBounds(UINT16 glyph_id, float text_size,
                                    const Matrix22& transform, Rect* bounds) {
    ScopedComPtr<IDWriteFontFace4> font_face4;
    if (!bounds || FAILED(font_face_->QueryInterface(&font_face4))) {
      return false;
    }

    DWRITE_GLYPH_IMAGE_FORMATS image_formats;
    if (FAILED(font_face4->GetGlyphImageFormats(glyph_id, 0, UINT32_MAX,
                                                &image_formats)) ||
        !(image_formats & DWRITE_GLYPH_IMAGE_FORMATS_PNG)) {
      return false;
    }

    DWRITE_GLYPH_IMAGE_DATA image_data = {};
    void* image_context = nullptr;
    if (FAILED(font_face4->GetGlyphImageData(
            glyph_id, DWritePixelsPerEm(text_size),
            DWRITE_GLYPH_IMAGE_FORMATS_PNG, &image_data, &image_context))) {
      return false;
    }

    bool ok = false;
    if (image_data.pixelSize.width > 0 && image_data.pixelSize.height > 0 &&
        image_data.pixelsPerEm > 0) {
      Rect image_bounds =
          Rect::MakeWH(static_cast<float>(image_data.pixelSize.width),
                       static_cast<float>(image_data.pixelSize.height));

      *bounds = DWritePngImageMatrix(text_size, transform, image_data)
                    .MapRect(image_bounds);
      bounds->SetLTRB(std::floor(bounds->Left()), std::floor(bounds->Top()),
                      std::ceil(bounds->Right()), std::ceil(bounds->Bottom()));
      ok = !bounds->IsEmpty();
    }

    font_face4->ReleaseGlyphImageData(image_context);
    return ok;
  }

  UINT32 DWritePixelsPerEm(float text_size) const {
    return static_cast<UINT32>(std::max(1.0f, std::round(text_size)));
  }

  Matrix DWritePngImageMatrix(float text_size, const Matrix22& transform,
                              const DWRITE_GLYPH_IMAGE_DATA& image_data) const {
    Matrix matrix = transform.ToMatrix();
    float scale = text_size / static_cast<float>(image_data.pixelsPerEm);
    matrix.PreScale(scale, scale);
    matrix.PreTranslate(-static_cast<float>(image_data.horizontalLeftOrigin.x),
                        -static_cast<float>(image_data.horizontalLeftOrigin.y));
    return matrix;
  }

  bool RasterDWritePngImage(const GlyphBitmapData& source,
                            uint32_t target_width, uint32_t target_height,
                            float text_size, const Matrix22& transform,
                            const DWRITE_GLYPH_IMAGE_DATA& image_data,
                            GlyphBitmapData* image) {
    if (!source.buffer || !image || source.width <= 0.0f ||
        source.height <= 0.0f || target_width == 0 || target_height == 0 ||
        image_data.pixelsPerEm == 0) {
      return false;
    }

    size_t target_byte_size = 0;
    if (!CheckedImageByteSize(target_width, target_height, &target_byte_size)) {
      return false;
    }

    const uint32_t source_width = static_cast<uint32_t>(source.width);
    const uint32_t source_height = static_cast<uint32_t>(source.height);
    size_t source_byte_size = 0;
    if (!CheckedImageByteSize(source_width, source_height, &source_byte_size)) {
      return false;
    }

    Bitmap source_bitmap(source_width, source_height,
                         AlphaType::kPremul_AlphaType, ColorType::kBGRA);
    if (!source_bitmap.GetPixelAddr()) {
      return false;
    }
    std::memcpy(source_bitmap.GetPixelAddr(), source.buffer, source_byte_size);

    Bitmap bitmap(target_width, target_height, AlphaType::kPremul_AlphaType,
                  ColorType::kBGRA);
    auto canvas = Canvas::MakeSoftwareCanvas(&bitmap);
    if (!canvas || !bitmap.GetPixelAddr()) {
      return false;
    }

    canvas->Clear(Color_TRANSPARENT);
    canvas->Translate(-image->origin_x_for_raster * desc_.context_scale,
                      -image->origin_y_for_raster * desc_.context_scale);
    canvas->Concat(DWritePngImageMatrix(text_size, transform, image_data));
    canvas->DrawImage(Image::MakeImage(source_bitmap.GetPixmap()), 0, 0,
                      SamplingOptions{FilterMode::kNearest, MipmapMode::kNone});

    auto* buffer = static_cast<uint8_t*>(std::malloc(target_byte_size));
    if (!buffer) {
      return false;
    }

    const uint32_t target_row_bytes = target_width * 4;
    if (bitmap.RowBytes() == target_row_bytes) {
      std::memcpy(buffer, bitmap.GetPixelAddr(), target_byte_size);
    } else {
      for (uint32_t y = 0; y < target_height; y++) {
        std::memcpy(
            buffer + static_cast<size_t>(y) * target_row_bytes,
            bitmap.GetPixelAddr() + static_cast<size_t>(y) * bitmap.RowBytes(),
            target_row_bytes);
      }
    }

    image->buffer = buffer;
    image->width = static_cast<float>(target_width);
    image->height = static_cast<float>(target_height);
    image->format = BitmapFormat::kBGRA8;
    image->need_free = true;
    return true;
  }

  bool GenerateDWritePngImage(UINT16 glyph_id, float text_size,
                              const Matrix22& transform,
                              GlyphBitmapData* image) {
    ScopedComPtr<IDWriteFontFace4> font_face4;
    if (!image || FAILED(font_face_->QueryInterface(&font_face4))) {
      return false;
    }

    DWRITE_GLYPH_IMAGE_FORMATS image_formats;
    if (FAILED(font_face4->GetGlyphImageFormats(glyph_id, 0, UINT32_MAX,
                                                &image_formats)) ||
        !(image_formats & DWRITE_GLYPH_IMAGE_FORMATS_PNG)) {
      return false;
    }

    DWRITE_GLYPH_IMAGE_DATA image_data = {};
    void* image_context = nullptr;
    if (FAILED(font_face4->GetGlyphImageData(
            glyph_id, DWritePixelsPerEm(text_size),
            DWRITE_GLYPH_IMAGE_FORMATS_PNG, &image_data, &image_context))) {
      return false;
    }

    uint32_t target_width =
        image->width > 0.0f ? static_cast<uint32_t>(std::lround(image->width))
                            : 0;
    uint32_t target_height =
        image->height > 0.0f ? static_cast<uint32_t>(std::lround(image->height))
                             : 0;
    image->buffer = nullptr;
    image->need_free = false;
    GlyphBitmapData source;
    bool ok =
        image_data.imageData && image_data.imageDataSize > 0 &&
        DecodePngGlyphImageToBGRA(image_data.imageData,
                                  image_data.imageDataSize, 0, 0, &source) &&
        RasterDWritePngImage(source, target_width, target_height, text_size,
                             transform, image_data, image);
    if (source.need_free) {
      std::free(source.buffer);
    }

    font_face4->ReleaseGlyphImageData(image_context);
    return ok;
  }

  bool GenerateDWriteColorV1Image(UINT16 glyph_id, float text_size,
                                  const Matrix22& transform,
                                  GlyphBitmapData* image) {
    if (!image) {
      return false;
    }

    uint32_t width = image->width > 0.0f
                         ? static_cast<uint32_t>(std::lround(image->width))
                         : 0;
    uint32_t height = image->height > 0.0f
                          ? static_cast<uint32_t>(std::lround(image->height))
                          : 0;
    size_t byte_size = 0;
    if (width == 0 || height == 0 ||
        !CheckedImageByteSize(width, height, &byte_size)) {
      return false;
    }

    Bitmap bitmap(width, height, AlphaType::kPremul_AlphaType,
                  ColorType::kBGRA);
    auto canvas = Canvas::MakeSoftwareCanvas(&bitmap);
    if (!canvas || !bitmap.GetPixelAddr()) {
      return false;
    }

    canvas->Clear(Color_TRANSPARENT);
    canvas->Translate(-image->origin_x_for_raster * desc_.context_scale,
                      -image->origin_y_for_raster * desc_.context_scale);
    if (!DrawDWriteColorV1Image(glyph_id, text_size, transform, canvas.get())) {
      return false;
    }

    auto* buffer = static_cast<uint8_t*>(std::malloc(byte_size));
    if (!buffer) {
      return false;
    }

    const uint32_t row_bytes = width * 4;
    if (bitmap.RowBytes() == row_bytes) {
      std::memcpy(buffer, bitmap.GetPixelAddr(), byte_size);
    } else {
      for (uint32_t y = 0; y < height; y++) {
        std::memcpy(
            buffer + static_cast<size_t>(y) * row_bytes,
            bitmap.GetPixelAddr() + static_cast<size_t>(y) * bitmap.RowBytes(),
            row_bytes);
      }
    }

    image->buffer = buffer;
    image->width = static_cast<float>(width);
    image->height = static_cast<float>(height);
    image->format = BitmapFormat::kBGRA8;
    image->need_free = true;
    return true;
  }

  bool GenerateDWriteColorBounds(UINT16 glyph_id, float text_size,
                                 const Matrix22& transform, Rect* bounds) {
    if (!bounds) {
      return false;
    }

    if (GenerateDWriteColorV1Bounds(glyph_id, text_size, transform, bounds)) {
      return true;
    }

    if (GenerateDWriteColorLayerBounds(glyph_id, text_size, transform,
                                       bounds)) {
      return true;
    }

    return GenerateDWritePngImageBounds(glyph_id, text_size, transform, bounds);
  }

  bool GenerateDWriteColorImageInfo(GlyphData* glyph) {
    UINT16 glyph_id = glyph->Id();
    if (font_face_->GetGlyphCount() <= glyph_id) {
      return false;
    }

    float text_size = desc_.text_size;
    Matrix22 transform;
    if (!GetDWriteImageTransform(&text_size, &transform)) {
      return false;
    }

    Rect bounds;
    return GenerateDWriteColorBounds(glyph_id, text_size, transform, &bounds) &&
           SetDWriteColorImageInfoFromBounds(glyph, bounds);
  }

  bool DrawDWriteColorImage(UINT16 glyph_id, float text_size,
                            const Matrix22& transform, Canvas* canvas) {
    ScopedComPtr<IDWriteColorGlyphRunEnumerator> color_layers;
    if (!CreateDWriteColorGlyphRunEnumerator(glyph_id, text_size, transform,
                                             &color_layers)) {
      return false;
    }

    Paint paint;
    paint.SetAntiAlias(true);
    if (!transform.IsIdentity()) {
      canvas->Concat(transform.ToMatrix());
    }

    BOOL has_color_layer = FALSE;
    bool drew_color_layer = false;
    while (true) {
      HRESULT hr = color_layers->MoveNext(&has_color_layer);
      if (FAILED(hr)) {
        return false;
      }
      if (!has_color_layer) {
        break;
      }

      const DWRITE_COLOR_GLYPH_RUN* color_run = nullptr;
      if (FAILED(color_layers->GetCurrentRun(&color_run))) {
        return false;
      }

      Path path;
      if (!GenerateDWriteColorLayerPath(color_run, &path)) {
        return false;
      }

      if (color_run->paletteIndex == 0xFFFF) {
        paint.SetColor(desc_.foreground_color);
      } else {
        paint.SetColor(DWriteColorToColor(color_run->runColor));
      }
      canvas->DrawPath(path, paint);
      drew_color_layer = true;
    }

    return drew_color_layer;
  }

  bool GenerateDWriteColorImage(GlyphData* glyph) {
    if (glyph->Image().width <= 0.0f || glyph->Image().height <= 0.0f ||
        glyph->Image().format != BitmapFormat::kBGRA8) {
      if (!GenerateDWriteColorImageInfo(glyph)) {
        return false;
      }
    }

    GlyphBitmapData image = glyph->Image();
    const uint32_t width = static_cast<uint32_t>(image.width);
    const uint32_t height = static_cast<uint32_t>(image.height);
    const size_t byte_size = static_cast<size_t>(width) * height * 4;
    if (width == 0 || height == 0 || byte_size == 0) {
      return false;
    }

    float text_size = desc_.text_size;
    Matrix22 transform;
    if (!GetDWriteImageTransform(&text_size, &transform)) {
      return false;
    }

    if (GenerateDWriteColorV1Image(glyph->Id(), text_size, transform, &image)) {
      GlyphDataWinAccess::SetImage(glyph, image);
      return true;
    }

    Bitmap bitmap(width, height, AlphaType::kPremul_AlphaType,
                  ColorType::kBGRA);
    auto canvas = Canvas::MakeSoftwareCanvas(&bitmap);
    if (!canvas || !bitmap.GetPixelAddr()) {
      return false;
    }

    canvas->Clear(Color_TRANSPARENT);
    canvas->Translate(-image.origin_x_for_raster * desc_.context_scale,
                      -image.origin_y_for_raster * desc_.context_scale);
    if (!DrawDWriteColorImage(glyph->Id(), text_size, transform,
                              canvas.get())) {
      if (GenerateDWritePngImage(glyph->Id(), text_size, transform, &image)) {
        GlyphDataWinAccess::SetImage(glyph, image);
        return true;
      }

      // Do not cache an undrawn transparent bitmap as success.
      GlyphDataWinAccess::SetImage(glyph, GlyphBitmapData{});
      return false;
    }

    uint8_t* buffer = reinterpret_cast<uint8_t*>(std::malloc(byte_size));
    if (!buffer) {
      return false;
    }
    std::memcpy(buffer, bitmap.GetPixelAddr(), byte_size);

    image.buffer = buffer;
    image.need_free = true;
    GlyphDataWinAccess::SetImage(glyph, image);
    return true;
  }

  bool GenerateDWriteImageInfo(GlyphData* glyph) {
    UINT16 glyph_id = glyph->Id();
    if (font_face_->GetGlyphCount() <= glyph_id) {
      return false;
    }

    float text_size = desc_.text_size;
    Matrix22 transform;
    if (!GetDWriteImageTransform(&text_size, &transform)) {
      return false;
    }

    ScopedComPtr<IDWriteGlyphRunAnalysis> glyph_run_analysis;
    if (!CreateDWriteGrayGlyphRunAnalysis(glyph_id, text_size, transform,
                                          &glyph_run_analysis)) {
      return false;
    }

    RECT texture_bounds;
    if (FAILED(glyph_run_analysis->GetAlphaTextureBounds(
            DWRITE_TEXTURE_ALIASED_1x1, &texture_bounds)) ||
        texture_bounds.left >= texture_bounds.right ||
        texture_bounds.top >= texture_bounds.bottom) {
      return false;
    }

    GlyphBitmapData image;
    image.origin_x =
        static_cast<float>(texture_bounds.left) / desc_.context_scale;
    image.origin_y =
        -static_cast<float>(texture_bounds.top) / desc_.context_scale;
    image.origin_x_for_raster = image.origin_x;
    image.origin_y_for_raster =
        static_cast<float>(texture_bounds.top) / desc_.context_scale;
    image.width =
        static_cast<float>(texture_bounds.right - texture_bounds.left);
    image.height =
        static_cast<float>(texture_bounds.bottom - texture_bounds.top);
    image.format = BitmapFormat::kGray8;
    GlyphDataWinAccess::SetImage(glyph, image);
    return true;
  }

  bool GenerateDWriteImage(GlyphData* glyph) {
    if (glyph->Image().width <= 0.0f || glyph->Image().height <= 0.0f ||
        glyph->Image().format != BitmapFormat::kGray8) {
      if (!GenerateDWriteImageInfo(glyph)) {
        return false;
      }
    }

    GlyphBitmapData image = glyph->Image();
    const size_t width = static_cast<size_t>(image.width);
    const size_t height = static_cast<size_t>(image.height);
    const size_t byte_size = width * height;
    if (width == 0 || height == 0 || byte_size == 0) {
      return false;
    }

    float text_size = desc_.text_size;
    Matrix22 transform;
    if (!GetDWriteImageTransform(&text_size, &transform)) {
      return false;
    }

    ScopedComPtr<IDWriteGlyphRunAnalysis> glyph_run_analysis;
    if (!CreateDWriteGrayGlyphRunAnalysis(glyph->Id(), text_size, transform,
                                          &glyph_run_analysis)) {
      return false;
    }

    RECT texture_bounds;
    texture_bounds.left = static_cast<LONG>(
        std::lround(image.origin_x_for_raster * desc_.context_scale));
    texture_bounds.top = static_cast<LONG>(
        std::lround(image.origin_y_for_raster * desc_.context_scale));
    texture_bounds.right = texture_bounds.left + static_cast<LONG>(width);
    texture_bounds.bottom = texture_bounds.top + static_cast<LONG>(height);

    uint8_t* buffer = reinterpret_cast<uint8_t*>(std::malloc(byte_size));
    if (!buffer) {
      return false;
    }
    std::memset(buffer, 0, byte_size);

    if (FAILED(glyph_run_analysis->CreateAlphaTexture(
            DWRITE_TEXTURE_ALIASED_1x1, &texture_bounds, buffer,
            static_cast<UINT32>(byte_size)))) {
      std::free(buffer);
      return false;
    }

    for (size_t i = 0; i < byte_size; i++) {
      buffer[i] = ApplySkiaMaskGammaToAlpha(buffer[i], desc_.foreground_color);
    }

    image.buffer = buffer;
    image.need_free = true;
    GlyphDataWinAccess::SetImage(glyph, image);
    return true;
  }

  bool GenerateDWritePath(GlyphData* glyph) {
    float scale_x = 1.0f;
    float text_size = desc_.text_size;
    Matrix22 transform;
    desc_.DecomposeMatrix(PortScaleType::kVertical, &scale_x, &text_size,
                          &transform);

    UINT16 glyph_id = glyph->Id();
    if (HasDWriteColorGlyph(glyph_id, text_size, transform)) {
      GlyphDataWinAccess::SetPath(glyph, Path{});
      return true;
    }

    Path path;
    if (!GenerateDWriteOutlinePath(glyph_id, text_size, transform, &path)) {
      return false;
    }

    GlyphDataWinAccess::SetPath(glyph, std::move(path));
    return true;
  }

  bool HasDWriteColorGlyph(UINT16 glyph_id, float text_size,
                           const Matrix22& transform) {
    ScopedComPtr<IDWriteFactory2> factory2;
    if (FAILED(factory_->QueryInterface(&factory2))) {
      return false;
    }

    DWRITE_MATRIX matrix;
    matrix.m11 = transform.GetScaleX();
    matrix.m12 = transform.GetSkewY();
    matrix.m21 = transform.GetSkewX();
    matrix.m22 = transform.GetScaleY();
    matrix.dx = 0.0f;
    matrix.dy = 0.0f;

    FLOAT advance = 0.0f;
    DWRITE_GLYPH_OFFSET offset;
    offset.advanceOffset = 0.0f;
    offset.ascenderOffset = 0.0f;

    DWRITE_GLYPH_RUN run;
    run.fontFace = font_face_.get();
    run.fontEmSize = text_size;
    run.glyphCount = 1;
    run.glyphIndices = &glyph_id;
    run.glyphAdvances = &advance;
    run.glyphOffsets = &offset;
    run.isSideways = FALSE;
    run.bidiLevel = 0;

    ScopedComPtr<IDWriteColorGlyphRunEnumerator> color_layers;
    HRESULT hr = factory2->TranslateColorGlyphRun(0.0f, 0.0f, &run, nullptr,
                                                  DWRITE_MEASURING_MODE_NATURAL,
                                                  &matrix, 0, &color_layers);
    if (hr == DWRITE_E_NOCOLOR || FAILED(hr) || !color_layers) {
      return false;
    }

    BOOL has_color_layer = FALSE;
    return SUCCEEDED(color_layers->MoveNext(&has_color_layer)) &&
           has_color_layer;
  }

  bool GenerateDWriteMetrics(GlyphData* glyph) {
    glyph->ZeroMetrics();

    UINT16 glyph_id = glyph->Id();
    if (font_face_->GetGlyphCount() <= glyph_id) {
      return false;
    }

    DWRITE_FONT_METRICS font_metrics;
    font_face_->GetMetrics(&font_metrics);
    if (font_metrics.designUnitsPerEm == 0) {
      return false;
    }

    float scale_x = 1.0f;
    float text_size = desc_.text_size;
    Matrix22 transform;
    desc_.DecomposeMatrix(PortScaleType::kVertical, &scale_x, &text_size,
                          &transform);

    DWRITE_GLYPH_METRICS glyph_metrics;
    if (FAILED(
            font_face_->GetDesignGlyphMetrics(&glyph_id, 1, &glyph_metrics))) {
      return false;
    }

    float advance_x = text_size *
                      static_cast<float>(glyph_metrics.advanceWidth) /
                      static_cast<float>(font_metrics.designUnitsPerEm);
    Vec2 advance = transform * Vec2{advance_x, 0.0f};

    Rect bounds;
    if (desc_.fake_bold &&
        GenerateDWriteFakeBoldBounds(glyph_id, text_size, transform, &bounds)) {
      GlyphDataWinAccess::SetMetrics(glyph, advance.x, advance.y, bounds,
                                     GlyphFormat::A8);
      return true;
    }

    if (GenerateDWriteColorBounds(glyph_id, text_size, transform, &bounds)) {
      GlyphDataWinAccess::SetMetrics(glyph, advance.x, advance.y, bounds,
                                     GlyphFormat::BGRA32);
      return true;
    }

    if (GenerateDWriteBounds(glyph_id, text_size, transform, &bounds)) {
      GlyphDataWinAccess::SetMetrics(glyph, advance.x, advance.y, bounds,
                                     GlyphFormat::A8);
      return true;
    }

    if (GenerateFallbackBoundsWithDWriteAdvance(glyph, advance)) {
      return true;
    }
    GlyphDataWinAccess::SetMetrics(glyph, advance.x, advance.y,
                                   Rect::MakeEmpty(), GlyphFormat::A8);
    return true;
  }

  bool GenerateDWriteFakeBoldBounds(UINT16 glyph_id, float text_size,
                                    const Matrix22& transform, Rect* bounds) {
    Path path;
    if (!GenerateDWriteOutlinePath(glyph_id, text_size, transform, &path) ||
        path.IsEmpty()) {
      return false;
    }

    Paint paint;
    paint.SetStyle(Paint::kStroke_Style);
    float stroke_width = text_size * ComputeFakeBoldScale(text_size);
    if (desc_.stroke_width > 0.0f) {
      float stroke_scale =
          desc_.text_size > 0.0f ? text_size / desc_.text_size : 1.0f;
      stroke_width = desc_.stroke_width * stroke_scale;
      paint.SetStrokeCap(desc_.cap);
      paint.SetStrokeJoin(desc_.join);
      paint.SetStrokeMiter(desc_.miter_limit);
    }
    paint.SetStrokeWidth(stroke_width);

    Path quad_path;
    Path stroke_path;
    Stroke stroke(paint);
    stroke.QuadPath(path, &quad_path);
    stroke.StrokePath(quad_path, &stroke_path);
    if (stroke_path.IsEmpty()) {
      return false;
    }

    Rect path_bounds = path.GetBounds();
    path_bounds.Join(stroke_path.GetBounds());
    bounds->SetLTRB(
        std::floor(path_bounds.Left()), std::floor(path_bounds.Top()),
        std::ceil(path_bounds.Right()), std::ceil(path_bounds.Bottom()));
    return !bounds->IsEmpty();
  }

  bool GenerateFallbackBoundsWithDWriteAdvance(GlyphData* glyph,
                                               const Vec2& advance) {
    float text_size = desc_.text_size;
    Matrix22 transform;
    if (!GetDWriteImageTransform(&text_size, &transform)) {
      return false;
    }

    Rect bounds;
    if (desc_.fake_bold) {
      if (!GenerateDWriteFakeBoldBounds(glyph->Id(), text_size, transform,
                                        &bounds)) {
        return false;
      }
    } else {
      Path path;
      if (!GenerateDWriteOutlinePath(glyph->Id(), text_size, transform,
                                     &path) ||
          path.IsEmpty()) {
        return false;
      }
      bounds = path.GetBounds();
      bounds.SetLTRB(std::floor(bounds.Left()), std::floor(bounds.Top()),
                     std::ceil(bounds.Right()), std::ceil(bounds.Bottom()));
    }

    if (bounds.IsEmpty()) {
      return false;
    }

    GlyphDataWinAccess::SetMetrics(glyph, advance.x, advance.y, bounds,
                                   GlyphFormat::A8);
    return true;
  }

  bool GenerateDWriteOutlinePath(UINT16 glyph_id, float text_size,
                                 const Matrix22& transform, Path* path) {
    Path outline;
    ScopedComPtr<IDWriteGeometrySink> geometry_sink(
        new DWritePathSink(&outline));
    if (FAILED(font_face_->GetGlyphRunOutline(text_size, &glyph_id, nullptr,
                                              nullptr, 1, FALSE, FALSE,
                                              geometry_sink.get()))) {
      return false;
    }

    if (!transform.IsIdentity()) {
      outline = outline.CopyWithMatrix(transform.ToMatrix());
    }

    *path = std::move(outline);
    return true;
  }

  bool GenerateDWriteBounds(UINT16 glyph_id, float text_size,
                            const Matrix22& transform, Rect* bounds) {
    DWRITE_MATRIX matrix;
    matrix.m11 = transform.GetScaleX();
    matrix.m12 = transform.GetSkewY();
    matrix.m21 = transform.GetSkewX();
    matrix.m22 = transform.GetScaleY();
    matrix.dx = 0.0f;
    matrix.dy = 0.0f;

    FLOAT advance = 0.0f;
    DWRITE_GLYPH_OFFSET offset;
    offset.advanceOffset = 0.0f;
    offset.ascenderOffset = 0.0f;

    DWRITE_GLYPH_RUN run;
    run.fontFace = font_face_.get();
    run.fontEmSize = text_size;
    run.glyphCount = 1;
    run.glyphIndices = &glyph_id;
    run.glyphAdvances = &advance;
    run.glyphOffsets = &offset;
    run.isSideways = FALSE;
    run.bidiLevel = 0;

    ScopedComPtr<IDWriteGlyphRunAnalysis> glyph_run_analysis;
    ScopedComPtr<IDWriteFactory2> factory2;
    ScopedComPtr<IDWriteFontFace2> font_face2;
    if (SUCCEEDED(factory_->QueryInterface(&factory2)) &&
        SUCCEEDED(font_face_->QueryInterface(&font_face2))) {
      if (FAILED(factory2->CreateGlyphRunAnalysis(
              &run, &matrix, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
              DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_ENABLED,
              DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 0.0f, 0.0f,
              &glyph_run_analysis))) {
        return false;
      }
    } else if (FAILED(factory_->CreateGlyphRunAnalysis(
                   &run, 1.0f, &matrix, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
                   DWRITE_MEASURING_MODE_NATURAL, 0.0f, 0.0f,
                   &glyph_run_analysis))) {
      return false;
    }

    RECT texture_bounds;
    if (FAILED(glyph_run_analysis->GetAlphaTextureBounds(
            DWRITE_TEXTURE_ALIASED_1x1, &texture_bounds)) ||
        texture_bounds.left >= texture_bounds.right ||
        texture_bounds.top >= texture_bounds.bottom) {
      return false;
    }

    bounds->SetLTRB(static_cast<float>(texture_bounds.left),
                    static_cast<float>(texture_bounds.top),
                    static_cast<float>(texture_bounds.right),
                    static_cast<float>(texture_bounds.bottom));
    return true;
  }

  ScopedComPtr<IDWriteFactory> factory_;
  ScopedComPtr<IDWriteFontFace> font_face_;
};

}  // namespace

std::unique_ptr<ScalerContext> MakeScalerContextDWrite(
    std::shared_ptr<Typeface> typeface, IDWriteFactory* factory,
    IDWriteFontFace* font_face, const ScalerContextDesc* desc) {
  return std::make_unique<ScalerContextDWrite>(std::move(typeface), factory,
                                               font_face, desc);
}

}  // namespace skity
