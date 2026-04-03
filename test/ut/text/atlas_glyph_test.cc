// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/atlas/atlas_glyph.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <new>

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

}  // namespace skity
