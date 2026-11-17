// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include "src/render/hw/hw_draw.hpp"
#include "src/render/text/transformed_mask_glyph_run.hpp"
#include "test/ut/gpu/fake_gpu_context.hpp"
#include "test/ut/text/glyph_test_typeface.hpp"
#include "test/ut/text/recording_glyph_draw_target.hpp"

namespace skity::test {
namespace {

class GlyphFallbackTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(gpu.Init()); }

  RecordingGlyphDrawTarget Build(const std::shared_ptr<Typeface>& face,
                                 const std::vector<GlyphID>& glyphs,
                                 const Paint& paint = Paint{},
                                 float size = 20.f,
                                 const Matrix& matrix = Matrix{},
                                 float density = 1.f) {
    Font font(face, size);
    std::vector<float> x(glyphs.size()), y(glyphs.size(), 40.f);
    for (size_t i = 0; i < x.size(); ++i) {
      x[i] = i * 700.f;
    }
    auto runs = GlyphRunBuilder::Build(
        {uint32_t(glyphs.size()), glyphs.data(), Point{0.f, 0.f, 0.f, 1.f},
         x.data(), y.data(), font},
        paint, {*gpu.GetAtlasManager(), arena, density, matrix});
    RecordingGlyphDrawTarget target;
    for (auto* run : runs) {
      run->Record({arena, Matrix{}, matrix, false}, target);
    }
    return target;
  }

  FakeGPUContext gpu;
  ArenaAllocator arena;
};

TEST_F(GlyphFallbackTest, NativeAndSyntheticTypefacesKeepCachesSeparate) {
  auto first =
      std::make_shared<GlyphTestTypeface>(std::vector<TestGlyphSpec>{{}});
  const auto first_draw = Build(first, {0});
  ASSERT_EQ(first_draw.atlas_draws.size(), 1u);

  // These constructors run in the test executable and the shared library.
  // A counter duplicated across them can reuse a synthetic scaler for a real
  // glyph, whose ID is outside the synthetic font's glyph table.
  auto native = Typeface::MakeFromFile(SKITY_FONT_DIR
                                       "fonts/resources/Roboto-Regular.ttf");
  ASSERT_NE(native, nullptr);
  auto last =
      std::make_shared<GlyphTestTypeface>(std::vector<TestGlyphSpec>{{}});
  ASSERT_GT(native->TypefaceId(), first->TypefaceId());
  ASSERT_GT(last->TypefaceId(), native->TypefaceId());

  const GlyphID glyph = native->UnicharToGlyph('O');
  ASSERT_NE(glyph, 0);
  const auto native_draw = Build(native, {glyph});
  ASSERT_EQ(native_draw.atlas_draws.size(), 1u);
  EXPECT_FALSE(native_draw.atlas_draws[0].layer_bounds.IsEmpty());
  EXPECT_EQ(first->state->image_requests[0], 1u);

  const auto cached_draw = Build(first, {0});
  ASSERT_EQ(cached_draw.atlas_draws.size(), 1u);
  EXPECT_EQ(cached_draw.atlas_draws[0].layer_bounds,
            first_draw.atlas_draws[0].layer_bounds);
  EXPECT_EQ(first->state->image_requests[0], 1u);
}

TEST_F(GlyphFallbackTest,
       AffineBitmapOnlyAndColorOutlineKeepAtlasRepresentation) {
  for (auto format :
       {GlyphFormat::A8, GlyphFormat::RGBA32, GlyphFormat::BGRA32}) {
    for (bool outline : {false, true}) {
      auto face = std::make_shared<GlyphTestTypeface>(
          std::vector<TestGlyphSpec>{{20.f, 20.f, outline, format}});
      Paint paint;
      paint.SetFontThreshold(10.f);
      auto target = Build(face, {0}, paint);
      if (format == GlyphFormat::A8 && outline) {
        EXPECT_EQ(target.path_draws.size(), 1u);
        EXPECT_TRUE(target.atlas_draws.empty());
      } else {
        ASSERT_EQ(target.atlas_draws.size(), 1u);
        EXPECT_TRUE(target.path_draws.empty());
        EXPECT_EQ(target.atlas_draws[0].has_coverage,
                  format == GlyphFormat::A8);
        EXPECT_EQ(target.atlas_draws[0].layer_bounds,
                  Rect::MakeLTRB(-3.f, 23.f, 17.f, 43.f));
      }
      if (format != GlyphFormat::A8) {
        EXPECT_EQ(face->state->path_requests, 0u);
      }
    }
  }
}

TEST_F(GlyphFallbackTest, OversizedDirectPreservesMixedGlyphOrder) {
  for (bool outline : {false, true}) {
    auto face = std::make_shared<GlyphTestTypeface>(
        std::vector<TestGlyphSpec>{{}, {600.f, 20.f, outline}, {}});
    auto target = Build(face, {0, 1, 2});
    ASSERT_EQ(target.events.size(), 3u);
    EXPECT_TRUE(target.events[0].atlas);
    EXPECT_EQ(target.events[1].atlas, !outline);
    EXPECT_TRUE(target.events[2].atlas);
    EXPECT_FLOAT_EQ(target.atlas_draws.front().layer_bounds.Left(), -3.f);
    EXPECT_FLOAT_EQ(target.atlas_draws.back().layer_bounds.Left(), 1397.f);
    if (outline) {
      EXPECT_FLOAT_EQ(target.path_draws[0].path.GetBounds().Width(), 600.f);
      EXPECT_EQ(face->state->image_requests[1], 1u);
    } else {
      EXPECT_NEAR(target.atlas_draws[1].layer_bounds.Width(), 600.f, 2.f);
      EXPECT_GT(face->state->image_requests[1], 1u);
      EXPECT_LE(face->state->image_requests[1], 9u);
    }
    EXPECT_EQ(face->state->image_requests[0], 1u);
    EXPECT_EQ(face->state->image_requests[2], 1u);
    EXPECT_EQ(face->state->path_requests, 1u);
  }
}

TEST_F(GlyphFallbackTest,
       CombinedPaintKeepsPassOrderWithoutDuplicateBitmapFill) {
  for (auto style :
       {Paint::kFill_Style, Paint::kStroke_Style, Paint::kStrokeAndFill_Style,
        Paint::kStrokeThenFill_Style}) {
    for (bool outline : {false, true}) {
      auto face =
          std::make_shared<GlyphTestTypeface>(std::vector<TestGlyphSpec>{
              {}, {20.f, 20.f, outline, GlyphFormat::A8, 600.f}, {}});
      Paint paint;
      paint.SetStyle(style);
      paint.SetStrokeWidth(2.f);
      paint.SetStrokeCap(Paint::kRound_Cap);
      paint.SetStrokeJoin(Paint::kBevel_Join);
      paint.SetStrokeColor(Color_BLUE);
      const auto target = Build(face, {0, 1, 2}, paint);
      if (style == Paint::kFill_Style) {
        EXPECT_EQ(target.events.size(), 1u);
        continue;
      }
      const bool combined = style != Paint::kStroke_Style;
      if (outline) {
        ASSERT_EQ(target.path_draws.size(), 1u);
        EXPECT_EQ(target.path_draws[0].paint.GetStyle(), Paint::kStroke_Style);
        EXPECT_EQ(target.path_draws[0].paint.GetStrokeColor(),
                  paint.GetStrokeColor());
        EXPECT_EQ(target.path_draws[0].paint.GetStrokeCap(),
                  paint.GetStrokeCap());
        EXPECT_EQ(target.path_draws[0].paint.GetStrokeJoin(),
                  paint.GetStrokeJoin());
        ASSERT_EQ(target.events.size(), combined ? 4u : 3u);
        const size_t path_index = style == Paint::kStrokeAndFill_Style ? 2 : 1;
        EXPECT_FALSE(target.events[path_index].atlas);
      } else {
        EXPECT_TRUE(target.path_draws.empty());
        // Fill/T/Fill and Stroke/skip/Stroke: exactly one draw for glyph 1.
        ASSERT_EQ(target.events.size(), combined ? 5u : 3u);
        size_t middle = 0;
        for (const auto& draw : target.atlas_draws) {
          middle += draw.layer_bounds.Left() > 600.f &&
                    draw.layer_bounds.Right() < 800.f;
        }
        EXPECT_EQ(middle, 1u);
      }
    }
  }
}

TEST_F(GlyphFallbackTest, ActualSizeRetryIsBoundedAndKeepsOtherGlyphs) {
  auto face = std::make_shared<GlyphTestTypeface>(std::vector<TestGlyphSpec>{
      {},
      {600.f, 20.f, false, GlyphFormat::A8, 0.f, true},
      {0.f, 0.f},
      {20.f, 20.f, false, GlyphFormat::A8, 0.f, false, true},
      {}});
  Paint paint;
  paint.SetFontThreshold(10.f);
  auto target = Build(face, {0, 1, 2, 3, 4}, paint);
  ASSERT_EQ(target.atlas_draws.size(), 2u);
  EXPECT_NEAR(target.atlas_draws[0].layer_bounds.Width(), 20.f, 5.f);
  EXPECT_NEAR(target.atlas_draws[1].layer_bounds.Left(), 2797.f, .001f);
  EXPECT_EQ(face->state->image_requests[1], 8u);
  EXPECT_EQ(face->state->image_requests[2], 1u);
  EXPECT_EQ(face->state->image_requests[3], 1u);
}

TEST_F(GlyphFallbackTest, PartialFailureRetainsEachSuccessfulCreationScale) {
  auto face = std::make_shared<GlyphTestTypeface>(std::vector<TestGlyphSpec>{
      {20.f, 20.f, false, GlyphFormat::A8, 0.f, false, false, 1.f},
      {600.f, 20.f}});
  Paint paint;
  paint.SetFontThreshold(10.f);
  auto target = Build(face, {0, 1}, paint);
  ASSERT_EQ(target.atlas_draws.size(), 2u);
  EXPECT_FLOAT_EQ(target.atlas_draws[0].draw->GetTransform().GetScaleX(), 1.f);
  EXPECT_GT(target.atlas_draws[1].draw->GetTransform().GetScaleX(), 1.f);
  EXPECT_EQ(target.atlas_draws[0].layer_bounds,
            Rect::MakeLTRB(-3.f, 23.f, 17.f, 43.f));
  EXPECT_NEAR(target.atlas_draws[1].layer_bounds.Width(), 600.f, 2.f);
}

TEST_F(GlyphFallbackTest, PageFullReusesOwnedImageAndCacheRetainsFormat) {
  for (auto format : {GlyphFormat::RGBA32, GlyphFormat::BGRA32}) {
    auto face = std::make_shared<GlyphTestTypeface>(std::vector<TestGlyphSpec>{
        {252.f, 252.f, false, format}, {252.f, 252.f, false, format}});
    Font font(face, 20.f);
    auto* atlas = gpu.GetAtlasManager()->GetAtlas(AtlasFormat::RGBA32);
    auto first = atlas->GetGlyphRegion(font, PackedGlyphID(0), Paint{}, false,
                                       1.f, Matrix{});
    auto second = atlas->GetGlyphRegion(font, PackedGlyphID(1), Paint{}, false,
                                        1.f, Matrix{});
    ASSERT_TRUE(first.IsReady());
    ASSERT_TRUE(second.IsReady());
    EXPECT_NE(first.region.index_in_group, second.region.index_in_group);
    EXPECT_EQ(face->state->image_requests[1], 1u);
    auto cached = atlas->GetGlyphRegion(font, PackedGlyphID(1), Paint{}, false,
                                        1.f, Matrix{});
    EXPECT_EQ(cached.region.loc, second.region.loc);
    EXPECT_EQ(cached.region.format, format);
    EXPECT_EQ(face->state->image_requests[1], 1u);
  }
}

TEST_F(GlyphFallbackTest, UnspecifiedFormatUsesImageBeforeChoosingFallback) {
  for (auto format : {GlyphFormat::A8, GlyphFormat::RGBA32}) {
    auto face = std::make_shared<GlyphTestTypeface>(std::vector<TestGlyphSpec>{
        {20.f, 20.f, true, format, 0.f, false, false, 0.f, true}});
    Paint paint;
    paint.SetFontThreshold(10.f);
    auto target = Build(face, {0}, paint);
    EXPECT_EQ(target.path_draws.size(), format == GlyphFormat::A8 ? 1u : 0u);
    EXPECT_EQ(target.atlas_draws.size(), format == GlyphFormat::A8 ? 0u : 1u);
  }
}

TEST_F(GlyphFallbackTest, EmptyAndInvalidDirectImagesDoNotTriggerScaleRetries) {
  auto face = std::make_shared<GlyphTestTypeface>(std::vector<TestGlyphSpec>{
      {0.f, 0.f}, {20.f, 20.f, false, GlyphFormat::A8, 0.f, false, true}, {}});
  const auto target = Build(face, {0, 1, 2});
  ASSERT_EQ(target.atlas_draws.size(), 1u);
  EXPECT_FLOAT_EQ(target.atlas_draws[0].layer_bounds.Left(), 1397.f);
  EXPECT_EQ(face->state->image_requests, (std::vector<uint32_t>{1, 1, 1}));
}

TEST_F(GlyphFallbackTest, ActualSizeRetryKeepsBearingAndDensityUnits) {
  auto face = std::make_shared<GlyphTestTypeface>(
      std::vector<TestGlyphSpec>{{600.f, 20.f}});
  Paint paint;
  paint.SetFontThreshold(10.f);
  for (float density : {1.f, 2.f}) {
    auto first = Build(face, {0}, paint, 20.f, Matrix{}, density);
    auto cached = Build(face, {0}, paint, 20.f, Matrix{}, density);
    ASSERT_EQ(first.atlas_draws.size(), 1u);
    ASSERT_EQ(cached.atlas_draws.size(), 1u);
    const auto& bounds = first.atlas_draws[0].layer_bounds;
    EXPECT_NEAR(bounds.Left(), -3.f, .001f);
    EXPECT_NEAR(bounds.Top(), 23.f, .001f);
    EXPECT_NEAR(bounds.Width(), 600.f, 2.f);
    EXPECT_EQ(cached.atlas_draws[0].layer_bounds, bounds);
  }
}

TEST_F(GlyphFallbackTest, LargeEmojiAtlasStillHasFallbackAtSizeCutoff) {
  gpu.SetLargerAtlasMask(8);
  auto face = std::make_shared<GlyphTestTypeface>(
      std::vector<TestGlyphSpec>{{20.f, 20.f, false, GlyphFormat::RGBA32}});
  for (float size : {320.f, 1024.f, 2048.f}) {
    auto target = Build(face, {0}, Paint{}, size);
    ASSERT_EQ(target.atlas_draws.size(), 1u);
    EXPECT_TRUE(target.path_draws.empty());
    EXPECT_NEAR(target.atlas_draws[0].layer_bounds.Width(), size, 2.f);
  }
}

}  // namespace
}  // namespace skity::test
