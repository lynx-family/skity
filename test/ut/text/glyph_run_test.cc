// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <limits>
#include <vector>

#include "src/render/hw/hw_draw.hpp"
#include "src/render/text/atlas_glyph_run.hpp"
#include "src/render/text/sdf_glyph_run.hpp"
#include "test/ut/gpu/fake_gpu_context.hpp"
#include "test/ut/text/recording_glyph_draw_target.hpp"

namespace skity::test {

TEST(GlyphRunTest, AtlasBatchesPreserveNonAdjacentTextureOrder) {
  Atlas atlas(AtlasFormat::A8, nullptr, false, false);
  AtlasGlyphBatchBuilder builder(atlas);
  const uint32_t group_size = atlas.GetConfig().max_num_bitmap_per_atlas;
  const Rect first = Rect::MakeXYWH(0.f, 0.f, 10.f, 12.f);
  const Rect second = Rect::MakeXYWH(30.f, 4.f, 10.f, 12.f);
  builder.Append({1, {2, 3, 10, 12}, 1.f}, GlyphFormat::A8, first);
  builder.Append({2, {2, 3, 10, 12}, 1.f}, GlyphFormat::A8, second);
  builder.Append({group_size + 1, {2, 3, 10, 12}, 1.f}, GlyphFormat::A8, first);
  builder.Append({1, {2, 3, 10, 12}, 1.f}, GlyphFormat::A8, second);

  const auto batches = builder.TakeBatches();
  ASSERT_EQ(batches.size(), 3u);
  EXPECT_EQ(batches[0].group_index, 0u);
  EXPECT_EQ(batches[1].group_index, 1u);
  EXPECT_EQ(batches[2].group_index, 0u);
  ASSERT_EQ(batches[0].quads.size(), 2u);
  EXPECT_EQ(batches[1].quads.size(), 1u);
  EXPECT_EQ(batches[2].quads.size(), 1u);
  EXPECT_EQ(batches[0].quads[0].GetBounds(), first);
  EXPECT_EQ(batches[0].quads[1].GetBounds(), second);
  EXPECT_EQ(batches[0].bounds, Rect::MakeLTRB(0.f, 0.f, 40.f, 16.f));
  // Texture selection bits and UVs are local to each atlas group.
  EXPECT_EQ(batches[1].quads[0].texture_coord_tl,
            batches[0].quads[0].texture_coord_tl);
  EXPECT_EQ(batches[1].quads[0].texture_coord_br,
            batches[0].quads[0].texture_coord_br);
}

TEST(GlyphRunTest, AtlasBatchesSeparateColorFormatsWithinOneTextureGroup) {
  Atlas atlas(AtlasFormat::RGBA32, nullptr, false, false);
  AtlasGlyphBatchBuilder builder(atlas);
  const Rect bounds = Rect::MakeWH(10.f, 12.f);
  const GlyphRegion region{0, {0, 0, 10, 12}, 1.f};
  builder.Append(region, GlyphFormat::RGBA32, bounds);
  builder.Append(region, GlyphFormat::BGRA32, bounds);
  builder.Append(region, GlyphFormat::RGBA32, bounds);

  const auto batches = builder.TakeBatches();
  ASSERT_EQ(batches.size(), 3u);
  EXPECT_EQ(batches[0].format, GlyphFormat::RGBA32);
  EXPECT_EQ(batches[1].format, GlyphFormat::BGRA32);
  EXPECT_EQ(batches[2].format, GlyphFormat::RGBA32);
}

TEST(GlyphRunTest, EmptyAtlasRegionsDoNotCreateDrawsOrExpandBounds) {
  Atlas atlas(AtlasFormat::A8, nullptr, false, false);
  AtlasGlyphBatchBuilder builder(atlas);
  const Rect bounds = Rect::MakeWH(10.f, 12.f);
  const GlyphRegion region{0, {0, 0, 10, 12}, 1.f};
  builder.Append({0, {-1, -1, 0, 0}, 1.f}, GlyphFormat::A8, bounds);
  builder.Append(region, GlyphFormat::A8, Rect::MakeEmpty());
  builder.Append(region, GlyphFormat::A8,
                 Rect::MakeWH(std::numeric_limits<float>::infinity(), 12.f));
  EXPECT_TRUE(builder.TakeBatches().empty());

  builder.Append(region, GlyphFormat::A8, bounds);
  builder.Append({0, {0, 0, 0, 12}, 1.f}, GlyphFormat::A8,
                 Rect::MakeXYWH(100.f, 100.f, 10.f, 12.f));
  const auto batches = builder.TakeBatches();
  ASSERT_EQ(batches.size(), 1u);
  EXPECT_EQ(batches[0].quads.size(), 1u);
  EXPECT_EQ(batches[0].bounds, bounds);
}

TEST(GlyphRunTest, RecordsWholeRunLayerBounds) {
  for (bool emoji : {false, true}) {
    auto typeface = Typeface::MakeFromFile(
        emoji ? SKITY_FONT_DIR "fonts/resources/NotoColorEmoji.ttf"
              : SKITY_FONT_DIR "fonts/resources/Roboto-Regular.ttf");
    ASSERT_NE(typeface, nullptr);
    const GlyphID glyph = typeface->UnicharToGlyph(emoji ? 0x1F600 : 'O');
    ASSERT_NE(glyph, 0);
    const GlyphID glyphs[] = {glyph, glyph, glyph};
    // Overlapping and distant glyphs all contribute to the same draw's bounds.
    const float positions_x[] = {0.f, 0.f, 80.f};
    const float positions_y[] = {0.f, 0.f, 0.f};
    for (bool perspective : {false, true}) {
      // Outline glyphs use paths under perspective; emoji uses atlas draws.
      if (perspective && !emoji) {
        continue;
      }
      SCOPED_TRACE(::testing::Message()
                   << "emoji=" << emoji << " perspective=" << perspective);
      FakeGPUContext context;
      ASSERT_TRUE(context.Init());
      ArenaAllocator arena;
      Paint paint;
      if (!emoji) {
        paint.SetStyle(Paint::kStroke_Style);
        paint.SetStrokeWidth(2.f);
      }
      Matrix transform = Matrix::Translate(4.f, 40.f);
      if (perspective) {
        transform.SetPersp0(0.001f);
      }
      const Matrix glyph_to_layer =
          Matrix::Translate(7.f, 9.f) * Matrix::Scale(0.5f, 0.5f);
      const Matrix layer_to_physical =
          Matrix::Translate(-12.f, 18.f) * Matrix::Scale(2.f, 3.f);
      FakeRootLayer layer(256, 192, Rect::MakeXYWH(6.f, -6.f, 128.f, 64.f),
                          GPUTextureFormat::kRGBA8Unorm, nullptr, false);
      const Font font(typeface, 20.f);
      auto runs = GlyphRunBuilder::Build(
          {3, glyphs, Point{0.f, 0.f, 0.f, 1.f}, positions_x, positions_y,
           font},
          paint, {*context.GetAtlasManager(), arena, 2.f, transform});
      ASSERT_EQ(runs.size(), 1u);
      const auto operation = RecordAtlasGlyphRun(
          *runs[0], {arena, glyph_to_layer, glyph_to_layer * transform, false});
      auto* draw = operation.draw;
      ASSERT_NE(draw, nullptr);
      const Rect bounds = operation.layer_bounds;
      // Canvas performs the final conversion using the current layer.
      draw->SetLayerSpaceBounds(
          layer.CalculateLayerSpaceBounds(bounds, Matrix{}));
      const Matrix offset = Matrix::Translate(13.f, 17.f);
      const auto shifted_operation = RecordAtlasGlyphRun(
          *runs[0], {arena, offset * glyph_to_layer,
                     offset * glyph_to_layer * transform, false});
      ASSERT_NE(shifted_operation.draw, nullptr);
      EXPECT_GT(bounds.Width(), 35.f);
      EXPECT_GT(bounds.Height(), 0.f);
      EXPECT_LT(bounds.Width(), 200.f);
      EXPECT_LT(bounds.Height(), 200.f);
      EXPECT_EQ(draw->GetLayerSpaceBounds(), layer_to_physical.MapRect(bounds));
      const Rect expected = offset.MapRect(bounds);
      const Rect shifted = shifted_operation.layer_bounds;
      EXPECT_NEAR(shifted.Left(), expected.Left(), 1e-4f);
      EXPECT_NEAR(shifted.Top(), expected.Top(), 1e-4f);
      EXPECT_NEAR(shifted.Right(), expected.Right(), 1e-4f);
      EXPECT_NEAR(shifted.Bottom(), expected.Bottom(), 1e-4f);
      // Recording again must not accumulate the shifted result.
      const auto repeated = RecordAtlasGlyphRun(
          *runs[0], {arena, glyph_to_layer, glyph_to_layer * transform, false});
      ASSERT_NE(repeated.draw, nullptr);
      EXPECT_EQ(repeated.layer_bounds, bounds);
      EXPECT_EQ(operation.layer_bounds, bounds);
    }
  }
}

TEST(GlyphRunTest, PathRunOwnsInputAndSubmitsEachGlyph) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  ArenaAllocator arena;
  GlyphSubRunList runs;
  std::vector<Rect> expected_bounds;
  {
    auto typeface = Typeface::MakeFromFile(
        SKITY_FONT_DIR "fonts/resources/Roboto-Regular.ttf");
    ASSERT_NE(typeface, nullptr);
    Font font(typeface, 20.f);
    GlyphID glyphs[] = {typeface->UnicharToGlyph('A'),
                        typeface->UnicharToGlyph('B')};
    float x[] = {3.f, 40.f};
    float y[] = {5.f, 7.f};
    const Point origin{11.f, 13.f, 0.f, 1.f};
    Paint paint;
    paint.SetFillColor(Color_RED);
    paint.SetFontThreshold(1.f);
    const GlyphData* paths[2]{};
    font.LoadGlyphPath(glyphs, 2, paths);
    for (size_t i = 0; i < 2; ++i) {
      ASSERT_NE(paths[i], nullptr);
      expected_bounds.push_back(paths[i]
                                    ->GetPath()
                                    .CopyWithMatrix(Matrix::Translate(
                                        origin.x + x[i], origin.y + y[i]))
                                    .GetBounds());
    }
    runs = GlyphRunBuilder::Build(
        {2, glyphs, origin, x, y, font}, paint,
        {*context.GetAtlasManager(), arena, 1.f, Matrix{}});
    ASSERT_EQ(runs.size(), 1u);
    paint.SetFillColor(Color_BLUE);
    glyphs[0] = 0;
    x[0] = 1000.f;
  }
  RecordingGlyphDrawTarget target;
  const Matrix local_to_layer = Matrix::Translate(17.f, 19.f);
  runs[0]->Record({arena, Matrix{}, local_to_layer, false}, target);
  EXPECT_TRUE(target.atlas_draws.empty());
  ASSERT_EQ(target.path_draws.size(), 2u);
  for (size_t i = 0; i < 2; ++i) {
    EXPECT_EQ(target.path_draws[i].path.GetBounds(), expected_bounds[i]);
    EXPECT_EQ(target.path_draws[i].paint.GetFillColor(),
              Color4fFromColor(Color_RED));
    EXPECT_EQ(target.path_draws[i].local_to_layer, local_to_layer);
  }
}

TEST(GlyphRunTest, AtlasRunsOwnPreparedGeometryAndMetadata) {
  for (int strategy = 0; strategy < 3; ++strategy) {
    SCOPED_TRACE(strategy);
    FakeGPUContext context;
    ASSERT_TRUE(context.Init());
    ArenaAllocator arena;
    GlyphSubRunList runs;
    GlyphAtlasDraw initial{};
    const Matrix transform = Matrix::Translate(5.f, 31.f);
    Matrix raster_transform = transform;
    if (strategy == 2) {
      raster_transform.SetPersp0(0.001f);
    }
    {
      const bool emoji = strategy == 2;
      auto typeface = Typeface::MakeFromFile(
          emoji ? SKITY_FONT_DIR "fonts/resources/NotoColorEmoji.ttf"
                : SKITY_FONT_DIR "fonts/resources/Roboto-Regular.ttf");
      ASSERT_NE(typeface, nullptr);
      Font font(typeface, 20.f);
      GlyphID glyph = typeface->UnicharToGlyph(emoji ? 0x1F600 : 'O');
      float x = 7.f, y = 9.f;
      Paint paint;
      paint.SetBlendMode(BlendMode::kSrc);
      const GlyphRunInput input{1,  &glyph, Point{3.f, 4.f, 0.f, 1.f},
                                &x, &y,     font};
      const GlyphRunBuildContext build_context{*context.GetAtlasManager(),
                                               arena, 2.f, raster_transform};
      if (strategy == 1) {
        runs.SetArenaAllocator(&arena);
        AppendSDFGlyphRuns(input, paint, build_context, runs);
      } else {
        runs = GlyphRunBuilder::Build(input, paint, build_context);
      }
      ASSERT_EQ(runs.size(), 1u);
      initial = RecordAtlasGlyphRun(*runs[0],
                                    {arena, Matrix{}, raster_transform, false});
      ASSERT_NE(initial.draw, nullptr);
      paint.SetBlendMode(BlendMode::kClear);
      paint.SetAlphaF(0.f);
      glyph = 0;
      x = 1000.f;
      y = 1000.f;
    }
    const auto recorded = RecordAtlasGlyphRun(
        *runs[0], {arena, Matrix{}, raster_transform, true});
    ASSERT_NE(recorded.draw, nullptr);
    EXPECT_EQ(recorded.layer_bounds, initial.layer_bounds);
    EXPECT_FALSE(recorded.layer_bounds.IsEmpty());
    EXPECT_EQ(recorded.blend_mode, BlendMode::kSrc);
    EXPECT_EQ(recorded.has_coverage, strategy != 2);
    EXPECT_EQ(recorded.source_opaque, strategy != 2);
  }
}

TEST(GlyphRunTest, BoundsMatchSeparatePaintStyles) {
  for (bool emoji : {false, true}) {
    auto typeface = Typeface::MakeFromFile(
        emoji ? SKITY_FONT_DIR "fonts/resources/NotoColorEmoji.ttf"
              : SKITY_FONT_DIR "fonts/resources/Roboto-Regular.ttf");
    ASSERT_NE(typeface, nullptr);
    const GlyphID glyph = typeface->UnicharToGlyph(emoji ? 0x1F600 : 'O');
    ASSERT_NE(glyph, 0);
    const float position_x = 0.f;
    const float position_y = 0.f;
    for (bool perspective : {false, true}) {
      if (perspective && !emoji) {
        continue;
      }
      FakeGPUContext context;
      ASSERT_TRUE(context.Init());
      ArenaAllocator arena;
      Matrix transform = Matrix::Translate(4.f, 40.f);
      if (perspective) {
        transform.SetPersp0(0.001f);
      }
      auto make_runs = [&](const Paint& paint) {
        const Font font(typeface, 20.f);
        return GlyphRunBuilder::Build(
            {1, &glyph, Point{0.f, 0.f, 0.f, 1.f}, &position_x, &position_y,
             font},
            paint, {*context.GetAtlasManager(), arena, 1.f, transform});
      };
      for (auto style :
           {Paint::kStrokeAndFill_Style, Paint::kStrokeThenFill_Style}) {
        SCOPED_TRACE(::testing::Message()
                     << "emoji=" << emoji << " perspective=" << perspective
                     << " style=" << style);
        Paint paint;
        paint.SetStyle(style);
        paint.SetStrokeWidth(2.f);
        auto runs = make_runs(paint);
        ASSERT_EQ(runs.size(), emoji ? 1u : 2u);
        for (size_t index = 0; index < runs.size(); ++index) {
          Paint run_paint = paint;
          const bool stroke =
              !emoji &&
              (style == Paint::kStrokeThenFill_Style ? index == 0 : index == 1);
          run_paint.SetStyle(stroke ? Paint::kStroke_Style
                                    : Paint::kFill_Style);
          auto reference = make_runs(run_paint);
          ASSERT_EQ(reference.size(), 1u);
          const auto operation = RecordAtlasGlyphRun(
              *runs[index], {arena, Matrix{}, transform, false});
          const auto reference_operation = RecordAtlasGlyphRun(
              *reference[0], {arena, Matrix{}, transform, false});
          ASSERT_NE(operation.draw, nullptr);
          ASSERT_NE(reference_operation.draw, nullptr);
          // A fill run (including emoji) must not inherit stroke expansion
          // from the original combined paint.
          EXPECT_EQ(operation.layer_bounds, reference_operation.layer_bounds);
        }
      }
    }
  }
}

}  // namespace skity::test
