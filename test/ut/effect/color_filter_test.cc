// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <cstring>
#include <skity/effect/color_filter.hpp>
#include <skity/graphic/paint.hpp>
#include <vector>

#include "src/effect/color_filter_base.hpp"
#include "src/graphic/color_priv.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"

TEST(BlendFilterTest, Creation) {
  auto filter =
      skity::ColorFilters::Blend(skity::Color_WHITE, skity::BlendMode::kDst);

  // no needs to create filter if BlendMode is kDst
  EXPECT_EQ(filter, nullptr);

  filter =
      skity::ColorFilters::Blend(skity::Color_WHITE, skity::BlendMode::kDstIn);

  // no needs to create filter if BlendMode is kDstIn and color alpha is 255
  EXPECT_EQ(filter, nullptr);

  filter = skity::ColorFilters::Blend(skity::Color_TRANSPARENT,
                                      skity::BlendMode::kDstOut);

  // no needs to create filter if BlendMode is kDstOut and color alpha is 0
  EXPECT_EQ(filter, nullptr);

  filter = skity::ColorFilters::Blend(skity::ColorSetARGB(127, 255, 0, 0),
                                      skity::BlendMode::kSrcOver);

  // needs to create filter if BlendMode is kDstOut and color alpha is not 0
  EXPECT_NE(filter, nullptr);

  auto bf = static_cast<skity::ColorFilterBase*>(filter.get());

  EXPECT_EQ(bf->GetType(), skity::ColorFilterType::kBlend);

  auto nf = static_cast<skity::BlendColorFilter*>(bf);

  EXPECT_EQ(nf->GetColor(), skity::ColorSetARGB(127, 255, 0, 0));
  EXPECT_EQ(nf->GetBlendMode(), skity::BlendMode::kSrcOver);
}

TEST(MatrixFilterTest, Creation) {
  auto filter = skity::ColorFilters::Matrix(nullptr);

  // nullptr matrix will cause creation failed
  EXPECT_EQ(filter, nullptr);

  constexpr float identity_matrix[20] = {1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f,
                                         0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f,
                                         0.f, 0.f, 0.f, 0.f, 1.f, 0.f};

  filter = skity::ColorFilters::Matrix(identity_matrix);

  // no needs to create filter if matrix is identity matrix
  EXPECT_EQ(filter, nullptr);

  constexpr float color_matrix[20] = {
      0, 1, 0, 0, 0,  //
      0, 0, 1, 0, 0,  //
      1, 0, 0, 0, 0,  //
      0, 0, 0, 1, 0,  //
  };

  filter = skity::ColorFilters::Matrix(color_matrix);

  auto bf = static_cast<skity::ColorFilterBase*>(filter.get());

  EXPECT_EQ(bf->GetType(), skity::ColorFilterType::kMatrix);

  auto matrix = skity::Matrix{
      color_matrix[0], color_matrix[5], color_matrix[10], color_matrix[15],  //
      color_matrix[1], color_matrix[6], color_matrix[11], color_matrix[16],  //
      color_matrix[2], color_matrix[7], color_matrix[12], color_matrix[17],  //
      color_matrix[3], color_matrix[8], color_matrix[13], color_matrix[18],  //
  };

  auto vec = skity::Vec4{
      color_matrix[4],
      color_matrix[9],
      color_matrix[14],
      color_matrix[19],
  };

  auto nf = static_cast<skity::MatrixColorFilter*>(bf);

  auto [m, v] = nf->GetMatrix();

  EXPECT_EQ(m, matrix);
  EXPECT_EQ(v, vec);
}

TEST(MatrixFilterTest, Apply) {
  constexpr float color_matrix[20] = {
      0, 1, 0, 0, 0,  //
      0, 0, 1, 0, 0,  //
      1, 0, 0, 0, 0,  //
      0, 0, 0, 1, 0,  //
  };

  auto filter = skity::ColorFilters::Matrix(color_matrix);

  auto nf = static_cast<skity::MatrixColorFilter*>(filter.get());

  auto [m, v] = nf->GetMatrix();

  auto color_4fv = skity::Vec4{1.f, 0.f, 0.f, 1.f};

  auto dst = filter->FilterColor(skity::Color4fToColor(color_4fv));

  auto expect_4fv = m * color_4fv + v;

  auto expect_c = skity::Color4fToColor(expect_4fv);

  EXPECT_EQ(dst, expect_c);
}

TEST(LinearToSRGBGammaTest, Apply) {
  auto filter = skity::ColorFilters::LinearToSRGBGamma();

  auto bf = static_cast<skity::ColorFilterBase*>(filter.get());

  EXPECT_EQ(bf->GetType(), skity::ColorFilterType::kLinearToSRGBGamma);

  auto dst = filter->FilterColor(
      skity::ColorToPMColor(skity::ColorSetARGB(127, 127, 0, 0)));

  auto expect = skity::ColorSetARGB(127, 187, 0, 0);

  EXPECT_EQ(dst, skity::ColorToPMColor(expect));
}

TEST(SRGBToLinearGammaTest, Apply) {
  auto filter = skity::ColorFilters::SRGBToLinearGamma();

  auto bf = static_cast<skity::ColorFilterBase*>(filter.get());

  EXPECT_EQ(bf->GetType(), skity::ColorFilterType::kSRGBToLinearGamma);

  auto dst = filter->FilterColor(
      skity::ColorToPMColor(skity::ColorSetARGB(127, 187, 0, 0)));

  auto expect = skity::ColorSetARGB(127, 126, 0, 0);

  EXPECT_EQ(dst, skity::ColorToPMColor(expect));
}

TEST(ComposeFilterTest, Creation) {
  auto filter = skity::ColorFilters::Compose(nullptr, nullptr);

  EXPECT_EQ(filter, nullptr);

  {
    auto filter1 = skity::ColorFilters::SRGBToLinearGamma();

    filter = skity::ColorFilters::Compose(filter1, nullptr);

    EXPECT_EQ(filter, filter1);

    auto filter2 = skity::ColorFilters::Compose(nullptr, filter1);

    EXPECT_EQ(filter2, filter1);
  }

  auto filter1 = skity::ColorFilters::SRGBToLinearGamma();
  auto filter2 = skity::ColorFilters::LinearToSRGBGamma();

  filter = skity::ColorFilters::Compose(filter1, filter2);

  EXPECT_NE(filter, nullptr);

  auto bf = static_cast<skity::ColorFilterBase*>(filter.get());

  EXPECT_EQ(bf->GetType(), skity::ColorFilterType::kCompose);

  auto compose_filter = static_cast<skity::ComposeColorFilter*>(bf);

  auto filters = compose_filter->GetFilters();

  EXPECT_EQ(filters.size(), 2);

  EXPECT_EQ(filters[0], filter2.get());
  EXPECT_EQ(filters[1], filter1.get());
}

TEST(ColorFilterOpacityTest, IncludesPaintAlphaAndColorFilter) {
  skity::Paint paint;
  EXPECT_TRUE(skity::IsPaintSourceOpaque(paint));

  paint.SetAlphaF(0.5f);
  EXPECT_FALSE(skity::IsPaintSourceOpaque(paint));
  paint.SetAlphaF(1.f);

  paint.SetColorFilter(
      skity::ColorFilters::Blend(skity::Color_RED, skity::BlendMode::kSrcATop));
  EXPECT_TRUE(skity::IsPaintSourceOpaque(paint));

  constexpr float alpha_preserving_matrix[20] = {
      0.f, 1.f, 0.f, 0.f, 0.f,  //
      1.f, 0.f, 0.f, 0.f, 0.f,  //
      0.f, 0.f, 1.f, 0.f, 0.f,  //
      0.f, 0.f, 0.f, 1.f, 0.f,  //
  };
  paint.SetColorFilter(skity::ColorFilters::Matrix(alpha_preserving_matrix));
  EXPECT_TRUE(skity::IsPaintSourceOpaque(paint));

  constexpr float alpha_reducing_matrix[20] = {
      1.f, 0.f, 0.f, 0.f,  0.f,  //
      0.f, 1.f, 0.f, 0.f,  0.f,  //
      0.f, 0.f, 1.f, 0.f,  0.f,  //
      0.f, 0.f, 0.f, 0.5f, 0.f,  //
  };
  paint.SetColorFilter(skity::ColorFilters::Matrix(alpha_reducing_matrix));
  EXPECT_FALSE(skity::IsPaintSourceOpaque(paint));

  paint.SetColorFilter(skity::ColorFilters::Compose(
      skity::ColorFilters::SRGBToLinearGamma(),
      skity::ColorFilters::Matrix(alpha_preserving_matrix)));
  EXPECT_TRUE(skity::IsPaintSourceOpaque(paint));

  paint.SetColorFilter(skity::ColorFilters::Compose(
      skity::ColorFilters::SRGBToLinearGamma(),
      skity::ColorFilters::Matrix(alpha_reducing_matrix)));
  EXPECT_FALSE(skity::IsPaintSourceOpaque(paint));
}

namespace {

// Captures the byte array payloads; the gamma filter flatten path writes
// nothing else.
class CaptureBuffer : public skity::WriteBuffer {
 public:
  std::vector<std::vector<uint8_t>> chunks;

  void WriteByteArray(const uint8_t* data, size_t size) override {
    chunks.emplace_back(data, data + size);
  }
  void WriteBool(bool) override {}
  void WriteFloat(float) override {}
  void WriteFloatArray(float const*, size_t) override {}
  void WriteInt32(int32_t) override {}
  void WriteUint32(uint32_t) override {}
  void WriteColor(skity::Color) override {}
  void WriteColorArray(skity::Color const*, size_t) override {}
  void WriteColor4f(const skity::Color4f&) override {}
  void WriteColor4fArray(const skity::Color4f*, size_t) override {}
  void WritePoint(const skity::Vec2&) override {}
  void WritePointArray(const skity::Vec2*, size_t) override {}
  void WriteSampling(const skity::SamplingOptions&) override {}
  void WriteMatrix(const skity::Matrix&) override {}
  void WriteRect(const skity::Rect&) override {}
  void WriteImage(const skity::Image*) override {}
  void WriteTypeface(const std::shared_ptr<skity::Typeface>&) override {}
  void WriteFlattenable(const skity::Flattenable*) override {}
};

}  // namespace

TEST(SRGBGammaColorFilterFlattenTest, WireMatchesLegacyConstants) {
  // FlattenToBuffer reads its constants from the ColorSpace factories now;
  // the wire must stay byte-identical with the legacy tables it replaced,
  // because buffers written by older builds are still read. The expected
  // values below are spelled with the legacy literals.
  const float srgb_fn[7] = {
      2.4f,
      static_cast<float>(1 / 1.055),
      static_cast<float>(0.055 / 1.055),
      static_cast<float>(1 / 12.92),
      0.04045f,
      0.f,
      0.f,
  };
  const float linear_fn[7] = {1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  const float srgb_gamut[9] = {
      0x6FA2 * 1.52587890625e-5f, 0x6299 * 1.52587890625e-5f,
      0x24A0 * 1.52587890625e-5f,  //
      0x38F5 * 1.52587890625e-5f, 0xB785 * 1.52587890625e-5f,
      0x0F84 * 1.52587890625e-5f,  //
      0x0390 * 1.52587890625e-5f, 0x18DA * 1.52587890625e-5f,
      0xB6CF * 1.52587890625e-5f,
  };

  auto expected_chunk = [&](const float* fn) {
    std::vector<uint8_t> expected(4 + 16 * sizeof(float));
    const uint8_t header[4] = {1, 0, 0, 0};  // k1_Version + reserved
    std::memcpy(expected.data(), header, sizeof(header));
    std::memcpy(expected.data() + sizeof(header), fn, 7 * sizeof(float));
    std::memcpy(expected.data() + sizeof(header) + 7 * sizeof(float),
                srgb_gamut, 9 * sizeof(float));
    return expected;
  };

  CaptureBuffer linear_to_srgb_buffer;
  auto linear_to_srgb = skity::ColorFilters::LinearToSRGBGamma();
  ASSERT_TRUE(linear_to_srgb != nullptr);
  linear_to_srgb->FlattenToBuffer(linear_to_srgb_buffer);
  ASSERT_EQ(linear_to_srgb_buffer.chunks.size(), 2u);
  EXPECT_EQ(linear_to_srgb_buffer.chunks[0], expected_chunk(linear_fn));
  EXPECT_EQ(linear_to_srgb_buffer.chunks[1], expected_chunk(srgb_fn));

  CaptureBuffer srgb_to_linear_buffer;
  auto srgb_to_linear = skity::ColorFilters::SRGBToLinearGamma();
  ASSERT_TRUE(srgb_to_linear != nullptr);
  srgb_to_linear->FlattenToBuffer(srgb_to_linear_buffer);
  ASSERT_EQ(srgb_to_linear_buffer.chunks.size(), 2u);
  EXPECT_EQ(srgb_to_linear_buffer.chunks[0], expected_chunk(srgb_fn));
  EXPECT_EQ(srgb_to_linear_buffer.chunks[1], expected_chunk(linear_fn));
}
