// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cstring>
#include <glm/glm.hpp>
#include <skity/io/pixmap.hpp>

#include "src/effect/color_filter_base.hpp"
#include "src/graphic/blend_mode_priv.hpp"
#include "src/graphic/color_priv.hpp"
#include "src/logging.hpp"

namespace skity {

PMColor ColorFilter::FilterColor(PMColor c) const {
#ifdef SKITY_CPU
  return As_CFB(this)->OnFilterColor(c);
#else
  return c;
#endif
}

std::shared_ptr<ColorFilter> ColorFilters::Blend(Color color, BlendMode mode) {
  if ((mode == BlendMode::kDst) ||  // r = d
      ((mode == BlendMode::kDstIn) &&
       (ColorGetA(color) == 255)) ||  // r = d * sa
      ((mode == BlendMode::kDstOut) &&
       (ColorGetA(color) == 0)))  // r = d * (1-sa)
  {
    return nullptr;
  }
  return std::make_shared<BlendColorFilter>(color, mode);
}

std::shared_ptr<ColorFilter> ColorFilters::Matrix(const float row_major[20]) {
  const float identity_matrix[20] = {1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f,
                                     0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f,
                                     0.f, 0.f, 0.f, 0.f, 1.f, 0.f};
  if ((row_major == nullptr) ||
      (std::memcmp(row_major, identity_matrix, 20 * sizeof(float)) == 0)) {
    return nullptr;
  }
  return std::make_shared<MatrixColorFilter>(row_major);
}

std::shared_ptr<ColorFilter> ColorFilters::LinearToSRGBGamma() {
  return std::make_shared<SRGBGammaColorFilter>(
      ColorFilterType::kLinearToSRGBGamma);
}

std::shared_ptr<ColorFilter> ColorFilters::SRGBToLinearGamma() {
  return std::make_shared<SRGBGammaColorFilter>(
      ColorFilterType::kSRGBToLinearGamma);
}

std::shared_ptr<ColorFilter> ColorFilters::Compose(
    std::shared_ptr<ColorFilter> outer, std::shared_ptr<ColorFilter> inner) {
  if (inner == nullptr) {
    return outer;
  }
  if (outer == nullptr) {
    return inner;
  }
  return std::make_shared<ComposeColorFilter>(outer, inner);
}

BlendColorFilter::BlendColorFilter(Color c, BlendMode m)
    : color_(c), pm_color_(ColorToPMColor(c)), mode_(m) {}

#ifdef SKITY_CPU

PMColor MatrixColorFilter::OnFilterColor(PMColor src_pm) const {
  Color src = PMColorToColor(src_pm);
  int32_t src_u32[4] = {int32_t(ColorGetR(src)), int32_t(ColorGetG(src)),
                        int32_t(ColorGetB(src)), int32_t(ColorGetA(src))};
  uint8_t dst_u8[4];
  for (size_t i = 0; i < 4; ++i) {
    int32_t mul_u32 =
        src_u32[0] * matrix_i16_[i][0] + src_u32[1] * matrix_i16_[i][1] +
        src_u32[2] * matrix_i16_[i][2] + src_u32[3] * matrix_i16_[i][3];
    dst_u8[i] = glm::clamp(mul_u32 / 255 + matrix_i16_[i][4], 0, 255);
  }
  return ColorToPMColor(
      ColorSetARGB(dst_u8[3], dst_u8[0], dst_u8[1], dst_u8[2]));
}

static constexpr uint8_t linear_to_srgb_table[256] = {
    0,   12,  21,  28,  33,  38,  42,  46,  49,  52,  55,  58,  61,  63,  66,
    68,  70,  73,  75,  77,  79,  81,  82,  84,  86,  88,  89,  91,  93,  94,
    96,  97,  99,  100, 102, 103, 104, 106, 107, 109, 110, 111, 112, 114, 115,
    116, 117, 118, 120, 121, 122, 123, 124, 125, 126, 127, 129, 130, 131, 132,
    133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 142, 143, 144, 145, 146,
    147, 148, 149, 150, 151, 151, 152, 153, 154, 155, 156, 157, 157, 158, 159,
    160, 161, 161, 162, 163, 164, 165, 165, 166, 167, 168, 168, 169, 170, 171,
    171, 172, 173, 174, 174, 175, 176, 176, 177, 178, 179, 179, 180, 181, 181,
    182, 183, 183, 184, 185, 185, 186, 187, 187, 188, 189, 189, 190, 191, 191,
    192, 193, 193, 194, 194, 195, 196, 196, 197, 197, 198, 199, 199, 200, 201,
    201, 202, 202, 203, 204, 204, 205, 205, 206, 206, 207, 208, 208, 209, 209,
    210, 210, 211, 212, 212, 213, 213, 214, 214, 215, 215, 216, 217, 217, 218,
    218, 219, 219, 220, 220, 221, 221, 222, 222, 223, 223, 224, 224, 225, 226,
    226, 227, 227, 228, 228, 229, 229, 230, 230, 231, 231, 232, 232, 233, 233,
    234, 234, 235, 235, 236, 236, 237, 237, 237, 238, 238, 239, 239, 240, 240,
    241, 241, 242, 242, 243, 243, 244, 244, 245, 245, 245, 246, 246, 247, 247,
    248, 248, 249, 249, 250, 250, 251, 251, 251, 252, 252, 253, 253, 254, 254,
    255};

static constexpr uint8_t srgb_to_linear_table[256] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,
    1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   2,   3,
    3,   3,   3,   3,   4,   4,   4,   4,   4,   5,   5,   5,   5,   6,   6,
    6,   6,   7,   7,   7,   8,   8,   8,   9,   9,   9,   10,  10,  10,  11,
    11,  11,  12,  12,  13,  13,  13,  14,  14,  15,  15,  16,  16,  16,  17,
    17,  18,  18,  19,  19,  20,  20,  21,  22,  22,  23,  23,  24,  24,  25,
    26,  26,  27,  27,  28,  29,  29,  30,  31,  31,  32,  33,  33,  34,  35,
    36,  36,  37,  38,  38,  39,  40,  41,  42,  42,  43,  44,  45,  46,  47,
    47,  48,  49,  50,  51,  52,  53,  54,  55,  55,  56,  57,  58,  59,  60,
    61,  62,  63,  64,  65,  66,  67,  68,  70,  71,  72,  73,  74,  75,  76,
    77,  78,  80,  81,  82,  83,  84,  85,  87,  88,  89,  90,  92,  93,  94,
    95,  97,  98,  99,  101, 102, 103, 105, 106, 107, 109, 110, 112, 113, 114,
    116, 117, 119, 120, 122, 123, 125, 126, 128, 129, 131, 132, 134, 135, 137,
    139, 140, 142, 144, 145, 147, 148, 150, 152, 153, 155, 157, 159, 160, 162,
    164, 166, 167, 169, 171, 173, 175, 176, 178, 180, 182, 184, 186, 188, 190,
    192, 193, 195, 197, 199, 201, 203, 205, 207, 209, 211, 213, 215, 218, 220,
    222, 224, 226, 228, 230, 232, 235, 237, 239, 241, 243, 245, 248, 250, 252,
    255};

PMColor SRGBGammaColorFilter::OnFilterColor(PMColor src_pm) const {
  Color src = PMColorToColor(src_pm);
  auto* table = type_ == ColorFilterType::kLinearToSRGBGamma
                    ? linear_to_srgb_table
                    : srgb_to_linear_table;
  return ColorToPMColor(ColorSetARGB(ColorGetA(src), table[ColorGetR(src)],
                                     table[ColorGetG(src)],
                                     table[ColorGetB(src)]));
}

PMColor BlendColorFilter::OnFilterColor(PMColor src) const {
  return PorterDuffBlend(pm_color_, src, mode_);
}

PMColor ComposeColorFilter::OnFilterColor(PMColor src) const {
  // TODO(zhangzhijian): Fix it.
  return src;
}

#endif

}  // namespace skity
