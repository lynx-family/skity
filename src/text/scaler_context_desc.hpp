// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_SCALER_CONTEXT_DESC_HPP
#define SRC_TEXT_SCALER_CONTEXT_DESC_HPP

#include <cstdint>
#include <cstring>
#include <skity/graphic/paint.hpp>
#include <skity/text/font.hpp>
#include <skity/text/typeface.hpp>
#include <type_traits>

#include "src/render/text/text_transform.hpp"

namespace skity {

// The underlying port accepts what kind of scale ratio.
enum class PortScaleType { kFull, kVertical };

struct ScalerContextDesc {
  // The complete object representation is the cache identity. Keep every
  // member initialized and preserve the dense-layout assertions below.
  uint32_t typeface_id{};
  float text_size{};
  float scale_x{};
  float skew_x{};
  Matrix22 transform{};

  // scale ratio applied to surface
  float context_scale = 1.0f;

  Color foreground_color{};

  float stroke_width{};
  float miter_limit{};
  Paint::Cap cap{};
  Paint::Join join{};

  uint8_t fake_bold{};
  uint8_t hinting{};
  uint8_t subpixel_positioning{};
  uint8_t subpixel_x_phase{};
  uint8_t subpixel_y_phase{};
  uint8_t baseline_snap{};
  uint8_t edging{};

  // Keep the complete object representation initialized and free of implicit
  // tail padding so it can be used directly as cache identity.
  uint8_t reserved_padding[3]{};

  friend inline bool operator==(const ScalerContextDesc& lhs,
                                const ScalerContextDesc& rhs) {
    return std::memcmp(&lhs, &rhs, sizeof(ScalerContextDesc)) == 0;
  }

  friend inline bool operator!=(const ScalerContextDesc& lhs,
                                const ScalerContextDesc& rhs) {
    return !(lhs == rhs);
  }

  size_t hash() const;

  static Color GetGlyphImageForegroundColor(const Font& font,
                                            const Paint& paint);

  static ScalerContextDesc MakeCanonicalized(const Font& font,
                                             const Paint& paint);

  static ScalerContextDesc MakeTransformed(const Font& font, const Paint& paint,
                                           float context_scale,
                                           const Matrix22& transform);

  Matrix22 GetTransformMatrix() const;
  Matrix22 GetLocalMatrix() const;
  void DecomposeMatrix(PortScaleType type, float* scale_x, float* scale_y,
                       Matrix22* transform) const;

  Font::FontHinting GetHinting() const {
    return static_cast<Font::FontHinting>(hinting);
  }

  Font::Edging GetEdging() const { return static_cast<Font::Edging>(edging); }
};

static_assert(sizeof(Matrix22) == sizeof(float) * 4,
              "Matrix22 must have no padding");
static_assert(std::is_trivially_copyable_v<Matrix22>,
              "Matrix22 must be trivially copyable");

static_assert(sizeof(ScalerContextDesc) ==
                  sizeof(ScalerContextDesc::typeface_id) +
                      sizeof(ScalerContextDesc::text_size) +
                      sizeof(ScalerContextDesc::scale_x) +
                      sizeof(ScalerContextDesc::skew_x) +
                      sizeof(ScalerContextDesc::transform) +
                      sizeof(ScalerContextDesc::context_scale) +
                      sizeof(ScalerContextDesc::foreground_color) +
                      sizeof(ScalerContextDesc::stroke_width) +
                      sizeof(ScalerContextDesc::miter_limit) +
                      sizeof(ScalerContextDesc::cap) +
                      sizeof(ScalerContextDesc::join) +
                      sizeof(ScalerContextDesc::fake_bold) +
                      sizeof(ScalerContextDesc::hinting) +
                      sizeof(ScalerContextDesc::subpixel_positioning) +
                      sizeof(ScalerContextDesc::subpixel_x_phase) +
                      sizeof(ScalerContextDesc::subpixel_y_phase) +
                      sizeof(ScalerContextDesc::baseline_snap) +
                      sizeof(ScalerContextDesc::edging) +
                      sizeof(ScalerContextDesc::reserved_padding),
              "ScalerContextDesc must have no padding");
static_assert(std::is_trivially_copyable_v<ScalerContextDesc>,
              "ScalerContextDesc must be trivially copyable");
}  // namespace skity

#endif  // SRC_TEXT_SCALER_CONTEXT_DESC_HPP
