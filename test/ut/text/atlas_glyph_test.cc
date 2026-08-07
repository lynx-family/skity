// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/atlas/atlas_glyph.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <new>

#include "src/render/text/atlas/atlas_bitmap.hpp"
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

  AtlasBitmap atlas(kAtlasWidth, kAtlasWidth, 1);
  ScalerContextDesc desc{};
  GlyphKey key(7, desc);
  const glm::ivec4 region = atlas.GenerateGlyphRegion(key, bitmap);
  ASSERT_NE(region, INVALID_LOC);

  for (uint32_t y = 0; y < kGlyphHeight; ++y) {
    const uint8_t* atlas_row =
        atlas.MemData() + static_cast<size_t>(region.y + y) * kAtlasWidth;
    for (uint32_t x = 0; x < kGlyphWidth; ++x) {
      EXPECT_EQ(atlas_row[region.x + x], source[y * kSourceRowBytes + x]);
    }
    EXPECT_EQ(atlas_row[region.x + kGlyphWidth], 0u);
  }
}

}  // namespace skity
