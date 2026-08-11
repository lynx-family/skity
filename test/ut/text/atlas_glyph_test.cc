// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/atlas/atlas_glyph.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <new>

#include "src/render/text/atlas/atlas_bitmap.hpp"
#include "src/render/text/glyph_raster_alignment.hpp"
#include "src/text/scaler_context_desc.hpp"

namespace skity {

TEST(AtlasGlyphTest, GlyphKeyHashIgnoresPadding) {
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

  // Use placement new to construct GlyphKey in the dirty buffers.
  // The padding bytes will retain the garbage values (0xAA and 0xBB).
  GlyphKey* key1 = new (buffer1) GlyphKey(id, desc);
  GlyphKey* key2 = new (buffer2) GlyphKey(id, desc);

  GlyphKey::Hash hasher;

  // If the hash function hashes the entire struct including padding,
  // this EXPECT_EQ will fail.
  EXPECT_EQ(hasher(*key1), hasher(*key2));

  key1->~GlyphKey();
  key2->~GlyphKey();
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

TEST(GlyphRasterAlignmentTest, QuantizesAndAlignsPhysicalPixelPhase) {
  constexpr float kScale = 2.f;
  const uint8_t phase = QuantizeGlyphSubpixelPhase(10.25f, kScale);

  EXPECT_EQ(phase, kGlyphSubpixelPhaseCount / 2);
  EXPECT_FLOAT_EQ(AlignRasterPointAtOrAbove(1.1f, kScale, phase), 1.25f);
}

TEST(AtlasGlyphTest, SubpixelPhaseParticipatesInGlyphKeyEquality) {
  ScalerContextDesc first_desc{};
  ScalerContextDesc second_desc{};
  second_desc.subpixel_x_phase = 1;

  EXPECT_FALSE(
      GlyphKey::Equal{}(GlyphKey(7, first_desc), GlyphKey(7, second_desc)));
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
