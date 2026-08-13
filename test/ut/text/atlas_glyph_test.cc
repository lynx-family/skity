// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/atlas/atlas_glyph.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <new>

#include "src/render/text/atlas/atlas_bitmap.hpp"
#include "src/render/text/glyph_position.hpp"
#include "src/text/scaler_context_desc.hpp"

namespace skity {

TEST(AtlasGlyphTest, GlyphKeyUsesDenseObjectRepresentation) {
  // Allocate two buffers with different "garbage" bytes, ensuring proper
  // alignment.
  alignas(GlyphKey) char buffer1[sizeof(GlyphKey)];
  alignas(GlyphKey) char buffer2[sizeof(GlyphKey)];

  std::memset(buffer1, 0xAA, sizeof(buffer1));
  std::memset(buffer2, 0xBB, sizeof(buffer2));

  GlyphID id = 42;
  ScalerContextDesc desc{};
  desc.typeface_id = 1;
  desc.text_size = 14.0f;
  desc.scale_x = 1.0f;
  desc.skew_x = 0.0f;
  desc.context_scale = 1.0f;
  desc.foreground_color = 0xFF000000;
  desc.stroke_width = 1.0f;
  desc.miter_limit = 4.0f;
  desc.cap = Paint::Cap::kButt_Cap;
  desc.join = Paint::Join::kMiter_Join;
  desc.fake_bold = 0;
  desc.hinting = 0;

  // Construct in prefilled storage to verify that initialization overwrites
  // the complete dense key representation.
  GlyphKey* key1 = new (buffer1) GlyphKey(id, desc);
  GlyphKey* key2 = new (buffer2) GlyphKey(id, desc);

  GlyphKey::Hash hasher;

  EXPECT_EQ(std::memcmp(key1, key2, sizeof(GlyphKey)), 0);
  EXPECT_TRUE(GlyphKey::Equal{}(*key1, *key2));
  EXPECT_EQ(hasher(*key1), hasher(*key2));

  key1->~GlyphKey();
  key2->~GlyphKey();
}

TEST(ScalerContextDescTest, SignedZeroHasDistinctBitwiseIdentity) {
  ScalerContextDesc positive_zero{};
  ScalerContextDesc negative_zero{};
  positive_zero.skew_x = 0.f;
  negative_zero.skew_x = -0.f;

  EXPECT_NE(
      std::memcmp(&positive_zero, &negative_zero, sizeof(ScalerContextDesc)),
      0);
  EXPECT_NE(positive_zero, negative_zero);
}

TEST(GlyphBitmapDataTest, ResolvesTightAndExplicitRowBytes) {
  GlyphBitmapData bitmap;
  bitmap.width = 3;

  bitmap.format = BitmapFormat::kGray8;
  EXPECT_EQ(bitmap.RowBytes(), 3u);

  bitmap.format = BitmapFormat::kBGRA8;
  EXPECT_EQ(bitmap.RowBytes(), 12u);

  bitmap.row_bytes = 16;
  EXPECT_EQ(bitmap.RowBytes(), 16u);
}

TEST(AtlasBitmapTest, KeepsBitwiseDistinctGlyphKeysSeparate) {
  uint8_t source = 0xFF;
  GlyphBitmapData bitmap;
  bitmap.width = 1;
  bitmap.height = 1;
  bitmap.buffer = &source;
  bitmap.format = BitmapFormat::kGray8;

  ScalerContextDesc positive_zero{};
  ScalerContextDesc negative_zero{};
  positive_zero.skew_x = 0.f;
  negative_zero.skew_x = -0.f;

  AtlasBitmap atlas(16, 16, 1);
  const GlyphRegion inserted =
      atlas.GenerateGlyphRegion(GlyphKey(7, positive_zero), bitmap);
  ASSERT_NE(inserted.loc, INVALID_LOC);
  EXPECT_EQ(atlas.GetGlyphRegion(GlyphKey(7, positive_zero)).loc, inserted.loc);
  EXPECT_EQ(atlas.GetGlyphRegion(GlyphKey(7, negative_zero)).loc, INVALID_LOC);
}

TEST(GlyphPositionTest, MatchesSkiaQuarterPixelBoundaries) {
  const GlyphPositionRoundingSpec spec{true, GlyphAxisAlignment::kX};
  struct Sample {
    float position;
    uint8_t phase;
    float quantized_position;
  };
  constexpr std::array<Sample, 8> samples = {{{10.124f, 0, 10.f},
                                              {10.126f, 1, 10.25f},
                                              {10.374f, 1, 10.25f},
                                              {10.376f, 2, 10.5f},
                                              {10.624f, 2, 10.5f},
                                              {10.626f, 3, 10.75f},
                                              {10.874f, 3, 10.75f},
                                              {10.876f, 0, 11.f}}};

  for (const Sample& sample : samples) {
    const QuantizedGlyphPosition result =
        QuantizeGlyphPosition({sample.position, 0.f}, 1.f, spec);
    EXPECT_EQ(result.x_phase, sample.phase);
    EXPECT_FLOAT_EQ(result.position.x, sample.quantized_position);
  }
}

TEST(GlyphPositionTest, UsesFloorForNegativeCoordinates) {
  const GlyphPositionRoundingSpec spec{true, GlyphAxisAlignment::kX};
  const QuantizedGlyphPosition below =
      QuantizeGlyphPosition({-0.126f, 0.f}, 1.f, spec);
  const QuantizedGlyphPosition above =
      QuantizeGlyphPosition({-0.124f, 0.f}, 1.f, spec);

  EXPECT_EQ(below.x_phase, 3);
  EXPECT_FLOAT_EQ(below.position.x, -0.25f);
  EXPECT_EQ(above.x_phase, 0);
  EXPECT_FLOAT_EQ(above.position.x, 0.f);
}

TEST(GlyphPositionTest, QuantizesInPhysicalPixelSpace) {
  const GlyphPositionRoundingSpec spec{true, GlyphAxisAlignment::kX};
  const QuantizedGlyphPosition below =
      QuantizeGlyphPosition({0.062f, 0.f}, 2.f, spec);
  const QuantizedGlyphPosition above =
      QuantizeGlyphPosition({0.063f, 0.f}, 2.f, spec);

  EXPECT_EQ(below.x_phase, 0);
  EXPECT_FLOAT_EQ(below.position.x, 0.f);
  EXPECT_EQ(above.x_phase, 1);
  EXPECT_FLOAT_EQ(above.position.x, 0.125f);
}

TEST(GlyphPositionTest, RoundsToHalfPixelThresholdWhenSubpixelIsDisabled) {
  const GlyphPositionRoundingSpec spec{false, GlyphAxisAlignment::kX};
  const QuantizedGlyphPosition below =
      QuantizeGlyphPosition({0.499f, 0.f}, 1.f, spec);
  const QuantizedGlyphPosition above =
      QuantizeGlyphPosition({0.5f, 0.f}, 1.f, spec);

  EXPECT_EQ(below.x_phase, 0);
  EXPECT_FLOAT_EQ(below.position.x, 0.f);
  EXPECT_EQ(above.x_phase, 0);
  EXPECT_FLOAT_EQ(above.position.x, 1.f);
}

TEST(GlyphPositionTest, KeepsOnlyTheAlignedAxisSubpixelPhase) {
  const GlyphPositionRoundingSpec spec{true, GlyphAxisAlignment::kX};
  const QuantizedGlyphPosition result =
      QuantizeGlyphPosition({10.26f, 20.26f}, 1.f, spec);

  EXPECT_EQ(result.x_phase, 1);
  EXPECT_EQ(result.y_phase, 0);
  EXPECT_FLOAT_EQ(result.position.x, 10.25f);
  EXPECT_FLOAT_EQ(result.position.y, 20.f);
}

TEST(GlyphPositionTest, ClassifiesBaselineAxisLikeSkia) {
  Matrix identity;
  EXPECT_EQ(ComputeAxisAlignmentForHorizontalText(false, identity),
            GlyphAxisAlignment::kNone);
  EXPECT_EQ(ComputeAxisAlignmentForHorizontalText(true, identity),
            GlyphAxisAlignment::kX);

  Matrix x_skew;
  x_skew.SetSkewX(0.25f);
  EXPECT_EQ(ComputeAxisAlignmentForHorizontalText(true, x_skew),
            GlyphAxisAlignment::kX);

  Matrix quarter_turn;
  quarter_turn.SetScaleX(0.f);
  quarter_turn.SetSkewX(-1.f);
  quarter_turn.SetSkewY(1.f);
  quarter_turn.SetScaleY(0.f);
  EXPECT_EQ(ComputeAxisAlignmentForHorizontalText(true, quarter_turn),
            GlyphAxisAlignment::kY);

  Matrix oblique;
  oblique.SetSkewY(0.25f);
  EXPECT_EQ(ComputeAxisAlignmentForHorizontalText(true, oblique),
            GlyphAxisAlignment::kNone);
}

TEST(GlyphPositionTest, QuantizesBothAxesWhenBaselineSnapIsDisabled) {
  const GlyphPositionRoundingSpec spec{true, GlyphAxisAlignment::kNone};
  const QuantizedGlyphPosition result =
      QuantizeGlyphPosition({10.26f, 20.51f}, 1.f, spec);

  EXPECT_EQ(result.x_phase, 1);
  EXPECT_EQ(result.y_phase, 2);
  EXPECT_FLOAT_EQ(result.position.x, 10.25f);
  EXPECT_FLOAT_EQ(result.position.y, 20.5f);
}

TEST(GlyphPositionTest, KeepsYPhaseForQuarterTurnBaseline) {
  const GlyphPositionRoundingSpec spec{true, GlyphAxisAlignment::kY};
  const QuantizedGlyphPosition result =
      QuantizeGlyphPosition({10.26f, 20.26f}, 1.f, spec);

  EXPECT_EQ(result.x_phase, 0);
  EXPECT_EQ(result.y_phase, 1);
  EXPECT_FLOAT_EQ(result.position.x, 10.f);
  EXPECT_FLOAT_EQ(result.position.y, 20.25f);
}

TEST(GlyphPositionTest, FlipsDeviceYPhaseForCoreGraphics) {
  EXPECT_EQ(FlipGlyphSubpixelPhase(0), 0);
  EXPECT_EQ(FlipGlyphSubpixelPhase(1), 3);
  EXPECT_EQ(FlipGlyphSubpixelPhase(2), 2);
  EXPECT_EQ(FlipGlyphSubpixelPhase(3), 1);
}

TEST(GlyphPositionTest, AlignsRasterPointToRequestedPhysicalPhase) {
  EXPECT_FLOAT_EQ(AlignRasterPointAtOrAbove(1.1f, 2.f, 2), 1.25f);
}

TEST(AtlasGlyphTest, SubpixelStateAndPhaseParticipateInGlyphKey) {
  ScalerContextDesc first_desc{};
  ScalerContextDesc second_desc{};
  second_desc.subpixel_positioning = 1;

  EXPECT_FALSE(
      GlyphKey::Equal{}(GlyphKey(7, first_desc), GlyphKey(7, second_desc)));

  first_desc.subpixel_positioning = 1;
  second_desc.subpixel_x_phase = 1;
  EXPECT_FALSE(
      GlyphKey::Equal{}(GlyphKey(7, first_desc), GlyphKey(7, second_desc)));
}

TEST(AtlasGlyphTest, EdgingParticipatesInGlyphKey) {
  ScalerContextDesc alias_desc{};
  ScalerContextDesc antialias_desc{};
  alias_desc.edging = static_cast<uint8_t>(Font::Edging::kAlias);
  antialias_desc.edging = static_cast<uint8_t>(Font::Edging::kAntiAlias);

  EXPECT_FALSE(
      GlyphKey::Equal{}(GlyphKey(7, alias_desc), GlyphKey(7, antialias_desc)));
}

TEST(AtlasBitmapTest, CopiesGlyphWithPaddedRows) {
  constexpr uint32_t kAtlasWidth = 16;
  constexpr uint32_t kGlyphWidth = 3;
  constexpr uint32_t kGlyphHeight = 2;
  constexpr size_t kSourceRowBytes = 5;
  std::array<uint8_t, kSourceRowBytes * kGlyphHeight> source = {
      1, 2, 3, 0xA1, 0xA2, 4, 5, 6, 0xB1, 0xB2,
  };

  GlyphBitmapData bitmap;
  bitmap.width = kGlyphWidth;
  bitmap.height = kGlyphHeight;
  bitmap.buffer = source.data();
  bitmap.row_bytes = kSourceRowBytes;
  bitmap.format = BitmapFormat::kGray8;
  bitmap.origin_x = -1.25f;
  bitmap.origin_y = 3.5f;

  AtlasBitmap atlas(kAtlasWidth, kAtlasWidth, 1);
  ScalerContextDesc desc{};
  GlyphKey key(7, desc);
  const GlyphRegion region = atlas.GenerateGlyphRegion(key, bitmap);
  ASSERT_NE(region.loc, INVALID_LOC);
  EXPECT_FLOAT_EQ(region.origin_x, bitmap.origin_x);
  EXPECT_FLOAT_EQ(region.origin_y, bitmap.origin_y);

  const GlyphRegion cached_region = atlas.GetGlyphRegion(key);
  EXPECT_EQ(cached_region.loc, region.loc);
  EXPECT_FLOAT_EQ(cached_region.origin_x, bitmap.origin_x);
  EXPECT_FLOAT_EQ(cached_region.origin_y, bitmap.origin_y);

  for (uint32_t y = 0; y < kGlyphHeight; ++y) {
    const uint8_t* atlas_row =
        atlas.MemData() + static_cast<size_t>(region.loc.y + y) * kAtlasWidth;
    for (uint32_t x = 0; x < kGlyphWidth; ++x) {
      EXPECT_EQ(atlas_row[region.loc.x + x], source[y * kSourceRowBytes + x]);
    }
    EXPECT_EQ(atlas_row[region.loc.x + kGlyphWidth], 0u);
  }
}

}  // namespace skity
