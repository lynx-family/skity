// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/drawable_glyph_run.hpp"

#include <gtest/gtest.h>

#include "src/text/scaler_context_cache.hpp"
#include "test/ut/gpu/fake_gpu_context.hpp"
#include "test/ut/text/concurrent_runner.h"
#include "test/ut/text/glyph_test_typeface.hpp"
#include "test/ut/text/recording_glyph_draw_target.hpp"

namespace skity::test {
namespace {

TestGlyphSpec ColorGlyph(bool drawable = true, float width = 20.f) {
  TestGlyphSpec spec;
  spec.format = GlyphFormat::RGBA32;
  spec.width = width;
  spec.drawable = drawable;
  return spec;
}

class DrawableGlyphRunTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(gpu.Init()); }

  GlyphSubRunList Build(const Font& font, const std::vector<GlyphID>& glyphs,
                        const Paint& paint, GlyphRunOptions options = {},
                        const Matrix& matrix = Matrix{}, float density = 1.f) {
    std::vector<float> x(glyphs.size(), 31.f), y(glyphs.size(), 43.f);
    return GlyphRunBuilder::Build(
        {uint32_t(glyphs.size()), glyphs.data(), Point{7.f, 11.f, 0.f, 1.f},
         x.data(), y.data(), font},
        paint, {*gpu.GetAtlasManager(), arena, density, matrix, options});
  }

  RecordingGlyphDrawTarget Record(const GlyphSubRunList& runs,
                                  const Matrix& matrix = Matrix{}) {
    RecordingGlyphDrawTarget target;
    for (auto* run : runs) {
      run->Record({arena, Matrix{}, matrix, false}, target);
    }
    return target;
  }

  Paint FallbackPaint() {
    Paint paint;
    paint.SetFontThreshold(10.f);
    paint.SetColor(Color_BLUE);
    return paint;
  }

  FakeGPUContext gpu;
  ArenaAllocator arena;
};

TEST_F(DrawableGlyphRunTest, DirectSuccessDoesNotPrepareDrawable) {
  auto face = std::make_shared<GlyphTestTypeface>(
      std::vector<TestGlyphSpec>{ColorGlyph()});
  auto target = Record(Build(Font(face, 20.f), {0}, Paint{}));
  ASSERT_EQ(target.atlas_draws.size(), 1u);
  EXPECT_TRUE(target.drawable_draws.empty());
  EXPECT_EQ(face->state->drawable_requests[0], 0u);
  EXPECT_EQ(face->state->path_requests, 0u);
}

TEST_F(DrawableGlyphRunTest, LargeAndPerspectiveUseVectorWithoutImages) {
  auto face = std::make_shared<GlyphTestTypeface>(
      std::vector<TestGlyphSpec>{ColorGlyph()});
  Matrix perspective;
  perspective.SetPersp0(.002f);
  for (const Matrix& matrix : {Matrix{}, perspective}) {
    for (auto style :
         {Paint::kFill_Style, Paint::kStroke_Style, Paint::kStrokeAndFill_Style,
          Paint::kStrokeThenFill_Style}) {
      Paint paint = FallbackPaint();
      paint.SetStyle(style);
      paint.SetAlphaF(.5f);
      paint.SetBlendMode(BlendMode::kMultiply);
      auto target =
          Record(Build(Font(face, 300.f), {0}, paint, {}, matrix, 2.f), matrix);
      ASSERT_EQ(target.drawable_draws.size(), 1u);
      EXPECT_TRUE(target.atlas_draws.empty());
      EXPECT_TRUE(target.path_draws.empty());
      const auto& draw = target.drawable_draws[0];
      EXPECT_EQ(draw.glyph_to_local,
                Matrix::Translate(38.f, 54.f) *
                    Matrix::Scale(300.f / 64.f, 300.f / 64.f));
      EXPECT_EQ(draw.local_to_layer, matrix);
      EXPECT_EQ(draw.paint.GetStyle(), Paint::kFill_Style);
      EXPECT_EQ(draw.paint.GetBlendMode(), BlendMode::kMultiply);
      EXPECT_FLOAT_EQ(draw.paint.GetAlphaF(), .5f);
    }
  }
  EXPECT_EQ(face->state->drawable_requests[0], 1u);
  EXPECT_EQ(face->state->image_requests[0], 0u);
  EXPECT_EQ(face->state->path_requests, 0u);
}

TEST_F(DrawableGlyphRunTest, RejectedDirectPreservesMixedRepresentationOrder) {
  auto face = std::make_shared<GlyphTestTypeface>(
      std::vector<TestGlyphSpec>{ColorGlyph(false), ColorGlyph(true, 600.f),
                                 ColorGlyph(false, 600.f), ColorGlyph(false)});
  auto target = Record(Build(Font(face, 20.f), {0, 1, 2, 3}, Paint{}));
  ASSERT_EQ(target.events.size(), 4u);
  EXPECT_TRUE(target.events[0].atlas);
  EXPECT_TRUE(target.events[1].drawable);
  EXPECT_TRUE(target.events[2].atlas);
  EXPECT_TRUE(target.events[3].atlas);
  EXPECT_EQ(face->state->image_requests[1], 1u);
  EXPECT_GT(face->state->image_requests[2], 1u);
  EXPECT_EQ(face->state->drawable_requests[0], 0u);
  EXPECT_EQ(face->state->drawable_requests[1], 1u);
  EXPECT_EQ(face->state->drawable_requests[2], 1u);
  EXPECT_EQ(face->state->drawable_requests[3], 0u);
}

TEST_F(DrawableGlyphRunTest, MissingOrInvalidImageStillAllowsDrawable) {
  auto missing = ColorGlyph();
  missing.fail_below_scale = 2.f;
  auto invalid = ColorGlyph();
  invalid.invalid_image = true;
  auto unsupported = missing;
  unsupported.drawable = false;
  auto face = std::make_shared<GlyphTestTypeface>(
      std::vector<TestGlyphSpec>{missing, invalid, unsupported});
  auto target = Record(Build(Font(face, 20.f), {0, 1, 2}, Paint{}));
  EXPECT_EQ(target.drawable_draws.size(), 2u);
  EXPECT_TRUE(target.atlas_draws.empty());
  for (auto requests : face->state->image_requests) {
    EXPECT_EQ(requests, 1u);
  }
}

TEST_F(DrawableGlyphRunTest,
       CacheIgnoresSizeTransformAndAlphaButKeysForeground) {
  auto face = std::make_shared<GlyphTestTypeface>(
      std::vector<TestGlyphSpec>{ColorGlyph(), ColorGlyph(false)});
  Paint paint = FallbackPaint();
  auto first = Record(Build(Font(face, 20.f), {0, 1}, paint));
  paint.SetAlphaF(.25f);
  Font font(face, 80.f);
  font.SetSubpixel(true);
  auto second =
      Record(Build(font, {0, 1}, paint, {}, Matrix::Scale(2.f, 3.f), 2.f));
  ASSERT_EQ(first.drawable_draws.size(), 1u);
  ASSERT_EQ(second.drawable_draws.size(), 1u);
  EXPECT_EQ(first.drawable_draws[0].drawable,
            second.drawable_draws[0].drawable);
  EXPECT_EQ(face->state->drawable_requests[0], 1u);
  EXPECT_EQ(face->state->drawable_requests[1], 1u);
  paint.SetColor(Color_GREEN);
  auto third = Record(Build(font, {0}, paint));
  EXPECT_NE(first.drawable_draws[0].drawable, third.drawable_draws[0].drawable);
  EXPECT_EQ(face->state->drawable_requests[0], 2u);
}

TEST_F(DrawableGlyphRunTest, RecordingOwnsGlyphAfterCacheAndTypefaceRelease) {
  GlyphSubRunList runs;
  std::weak_ptr<Typeface> weak_face;
  {
    auto face = std::make_shared<GlyphTestTypeface>(
        std::vector<TestGlyphSpec>{ColorGlyph()});
    weak_face = face;
    runs = Build(Font(face, 20.f), {0}, FallbackPaint());
    ScalerContextCache::GlobalScalerContextCache()->PurgeByTypeface(
        face->TypefaceId());
  }
  EXPECT_TRUE(weak_face.expired());
  auto target = Record(runs);
  ASSERT_EQ(target.drawable_draws.size(), 1u);
  PictureRecorder recorder;
  recorder.BeginRecording();
  target.drawable_draws[0].drawable->Draw(recorder.GetRecordingCanvas());
  EXPECT_FALSE(recorder.FinishRecording()->Empty());
}

TEST(DrawableGlyphCacheTest, BitmapSnapshotKeepsPathWithoutRetainingDrawable) {
  OwnedGlyphImage image;
  std::weak_ptr<const GlyphDrawable> recording;
  {
    auto spec = ColorGlyph();
    spec.outline = true;
    auto face =
        std::make_shared<GlyphTestTypeface>(std::vector<TestGlyphSpec>{spec});
    const Paint paint;
    const auto desc = ScalerContextDesc::MakeTransformed(
        Font(face, 64.f), paint, 1.f, Matrix22{});
    ScalerContextContainer scaler(face->CreateScalerContext(&desc));
    const GlyphID id = 0;
    std::shared_ptr<const GlyphDrawable> drawable;
    scaler.PrepareDrawables(&id, 1, &drawable);
    ASSERT_TRUE(drawable);
    recording = drawable;
    const GlyphData* glyph = nullptr;
    scaler.PreparePaths(&id, 1, &glyph);
    ASSERT_FALSE(glyph->GetPath().IsEmpty());
    image = scaler.PrepareOwnedImage(PackedGlyphID(id), paint);
    ASSERT_EQ(image.GetStatus(), OwnedGlyphImage::Status::kReady);
    drawable.reset();
    EXPECT_FALSE(recording.expired());
  }
  EXPECT_TRUE(recording.expired());
  EXPECT_FALSE(image.Get()->GetPath().IsEmpty());
  EXPECT_NE(image.Image().buffer, nullptr);
}

TEST(DrawableGlyphCacheTest, ConcurrentFirstReplayOfDrawAndClipPaths) {
  class PathReader final : public RecordingCanvas {
   public:
    Rect draw_bounds, clip_bounds;
    bool draw_convex = false, clip_convex = false;

   private:
    void OnDrawPath(const Path& path, const Paint&) override {
      draw_bounds = path.GetBounds();
      draw_convex = path.IsConvex();
    }
    void OnClipPath(const Path& path, ClipOp) override {
      clip_bounds = path.GetBounds();
      clip_convex = path.IsConvex();
    }
  };

  Path triangle;
  triangle.MoveTo(0, 0).LineTo(8, 0).LineTo(0, 6).Close();
  PictureRecorder recorder;
  recorder.BeginRecording();
  auto* canvas = recorder.GetRecordingCanvas();
  canvas->DrawPath(triangle, Paint{});
  canvas->ClipPath(triangle);
  const Rect bounds = Rect::MakeWH(8, 6);
  auto drawable = GlyphDrawable::Make(recorder.FinishRecording(), bounds);
  ASSERT_TRUE(drawable);

  ConcurrentRunner(2, 1).Run([&](int) {
    PathReader reader;
    drawable->Draw(&reader);
    EXPECT_EQ(reader.draw_bounds, bounds);
    EXPECT_EQ(reader.clip_bounds, bounds);
    EXPECT_TRUE(reader.draw_convex);
    EXPECT_TRUE(reader.clip_convex);
  });
}

TEST_F(DrawableGlyphRunTest, StrategiesCanBeDisabledIndependently) {
  auto face = std::make_shared<GlyphTestTypeface>(
      std::vector<TestGlyphSpec>{ColorGlyph()});
  GlyphRunOptions options;
  options.direct = false;
  auto drawable = Record(Build(Font(face, 20.f), {0}, Paint{}, options));
  EXPECT_EQ(drawable.drawable_draws.size(), 1u);
  options.drawable = false;
  auto bitmap = Record(Build(Font(face, 20.f), {0}, Paint{}, options));
  EXPECT_TRUE(bitmap.drawable_draws.empty());
  EXPECT_EQ(bitmap.atlas_draws.size(), 1u);
  options.transformed_mask = false;
  EXPECT_TRUE(Build(Font(face, 20.f), {0}, Paint{}, options).empty());
  options.drawable = true;
  EXPECT_EQ(Record(Build(Font(face, 20.f), {0}, Paint{}, options))
                .drawable_draws.size(),
            1u);
}

#if !SKITY_TEST_CT_FONT && !defined(_WIN32)
TEST_F(DrawableGlyphRunTest, NativeCOLRv1UsesDrawableAndBitmapFontKeepsAtlas) {
  auto colr = Typeface::MakeFromFile(SKITY_FONT_DIR
                                     "fonts/resources/NotoColorEmoji.ttf");
  ASSERT_NE(colr, nullptr);
  const GlyphID glyph = colr->UnicharToGlyph(0x1F600);
  ASSERT_NE(glyph, 0);
  auto color = Record(Build(Font(colr, 320.f), {glyph}, FallbackPaint()));
  ASSERT_EQ(color.drawable_draws.size(), 1u);
  EXPECT_TRUE(color.atlas_draws.empty());
  EXPECT_TRUE(color.path_draws.empty());
  EXPECT_FALSE(color.drawable_draws[0].drawable->GetBounds().IsEmpty());

  auto bitmap = Typeface::MakeFromFile(
      SKITY_FONT_DIR "fonts/resources/NotoColorEmojiFlags.ttf");
  ASSERT_NE(bitmap, nullptr);
  const GlyphID flag = bitmap->UnicharToGlyph(0x1F1FA);
  ASSERT_NE(flag, 0);
  auto mask = Record(Build(Font(bitmap, 320.f), {flag}, FallbackPaint()));
  EXPECT_TRUE(mask.drawable_draws.empty());
  EXPECT_EQ(mask.atlas_draws.size(), 1u);
}
#endif

}  // namespace
}  // namespace skity::test
