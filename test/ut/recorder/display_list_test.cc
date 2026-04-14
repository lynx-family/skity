// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <skity/effect/color_filter.hpp>
#include <skity/effect/image_filter.hpp>
#include <skity/effect/mask_filter.hpp>
#include <skity/effect/shader.hpp>
#include <skity/recorder/picture_recorder.hpp>
#include <skity/skity.hpp>

using testing::_;

class MockCanvas : public skity::Canvas {
 public:
  MOCK_METHOD(void, OnClipRect,
              (skity::Rect const& rect, skity::Canvas::ClipOp op), (override));
  MOCK_METHOD(void, OnClipRRect,
              (skity::RRect const& rrect, skity::Canvas::ClipOp op),
              (override));
  MOCK_METHOD(void, OnClipPath,
              (skity::Path const& path, skity::Canvas::ClipOp op), (override));

  MOCK_METHOD(void, OnSave, (), (override));
  MOCK_METHOD(void, OnRestore, (), (override));
  MOCK_METHOD(void, OnRestoreToCount, (int saveCount), (override));
  MOCK_METHOD(void, OnTranslate, (float dx, float dy), (override));

  MOCK_METHOD(void, OnDrawRect,
              (skity::Rect const& rect, skity::Paint const& paint), (override));

  MOCK_METHOD(void, OnDrawDRRect,
              (skity::RRect const& outer, skity::RRect const& inner,
               skity::Paint const& paint),
              (override));

  MOCK_METHOD(void, OnDrawPath,
              (skity::Path const& path, skity::Paint const& paint), (override));

  MOCK_METHOD(void, OnSaveLayer,
              (const skity::Rect& bounds, const skity::Paint& paint),
              (override));

  MOCK_METHOD(void, OnDrawBlob,
              (const skity::TextBlob* blob, float x, float y,
               skity::Paint const& paint),
              (override));

  MOCK_METHOD(void, OnDrawImageRect,
              (std::shared_ptr<skity::Image> image, const skity::Rect& src,
               const skity::Rect& dst, const skity::SamplingOptions& sampling,
               skity::Paint const* paint),
              (override));

  MOCK_METHOD(void, OnDrawGlyphs,
              (uint32_t count, const skity::GlyphID glyphs[],
               const float position_x[], const float position_y[],
               const skity::Font& font, const skity::Paint& paint),
              (override));

  MOCK_METHOD(void, OnDrawPaint, (skity::Paint const& paint), (override));

  MOCK_METHOD(void, OnFlush, (), (override));

  MOCK_METHOD(uint32_t, OnGetWidth, (), (const, override));

  MOCK_METHOD(uint32_t, OnGetHeight, (), (const, override));
};

skity::Rect CalculateDisplayListBounds(
    skity::Rect cull_rect,
    std::function<void(skity::Canvas* canvas)> draw_callback) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(cull_rect);
  auto canvas = recorder.GetRecordingCanvas();
  draw_callback(canvas);
  auto display_list = recorder.FinishRecording();
  return display_list->GetBounds();
}

std::vector<int32_t> SearchDisplayListOffsets(skity::DisplayList* display_list,
                                              const skity::Rect& rect) {
  std::vector<int32_t> offsets;
  for (const auto& offset : display_list->Search(rect)) {
    offsets.push_back(offset.GetValue());
  }
  return offsets;
}

std::vector<skity::Rect> SearchNonOverlappingDrawnRects(
    skity::DisplayList* display_list, const skity::Rect& rect) {
  return display_list->SearchNonOverlappingDrawnRects(rect);
}

TEST(DisplayList, CanCalculateBounds) {
  skity::Rect bounds;
  bounds = CalculateDisplayListBounds(
      /*cull_rect=*/skity::Rect::MakeLTRB(0, 0, 100, 100),
      [](skity::Canvas* canvas) {
        // do nothing
      });
  EXPECT_EQ(bounds, skity::Rect::MakeEmpty());

  bounds = CalculateDisplayListBounds(
      /*cull_rect=*/skity::Rect::MakeLTRB(0, 0, 100, 100),
      [](skity::Canvas* canvas) {
        canvas->DrawRect(skity::Rect::MakeLTRB(10, 20, 30, 40), skity::Paint{});
      });
  EXPECT_EQ(bounds, skity::Rect::MakeLTRB(10, 20, 30, 40));

  bounds = CalculateDisplayListBounds(
      /*cull_rect=*/skity::Rect::MakeLTRB(0, 0, 100, 100),
      [](skity::Canvas* canvas) {
        canvas->DrawRect(skity::Rect::MakeLTRB(10, 10, 20, 20), skity::Paint{});
        canvas->DrawRect(skity::Rect::MakeLTRB(30, 30, 70, 70), skity::Paint{});
      });
  EXPECT_EQ(bounds, skity::Rect::MakeLTRB(10, 10, 70, 70));

  bounds = CalculateDisplayListBounds(
      /*cull_rect=*/skity::Rect::MakeLTRB(0, 0, 100, 100),
      [](skity::Canvas* canvas) {
        skity::Paint paint;
        paint.SetColor(skity::Color_RED);
        canvas->DrawPaint(paint);
      });
  EXPECT_EQ(bounds, skity::Rect::MakeLTRB(0, 0, 100, 100));

  bounds = CalculateDisplayListBounds(
      /*cull_rect=*/skity::Rect::MakeLTRB(0, 0, 100, 100),
      [](skity::Canvas* canvas) {
        canvas->DrawRect(skity::Rect::MakeLTRB(-30, 30, 70, 110),
                         skity::Paint{});
      });
  EXPECT_EQ(bounds, skity::Rect::MakeLTRB(0, 30, 70, 100));

  bounds = CalculateDisplayListBounds(
      /*cull_rect=*/skity::Rect::MakeLTRB(0, 0, 100, 100),
      [](skity::Canvas* canvas) {
        skity::Path path;
        path.MoveTo(30, 30);
        path.LineTo(60, 60);
        path.LineTo(30, 60);
        path.Close();
        canvas->DrawPath(path, skity::Paint{});
      });
  EXPECT_EQ(bounds, skity::Rect::MakeLTRB(30, 30, 60, 60));

  bounds = CalculateDisplayListBounds(
      /*cull_rect=*/skity::Rect::MakeLTRB(0, 0, 100, 100),
      [](skity::Canvas* canvas) {
        skity::Path path;
        path.MoveTo(30, 30);
        path.LineTo(60, 60);
        path.LineTo(30, 60);
        path.Close();
        skity::Paint paint;
        paint.SetStrokeWidth(10);
        paint.SetStyle(skity::Paint::kStroke_Style);
        canvas->DrawPath(path, paint);
      });
  EXPECT_NE(bounds, skity::Rect::MakeLTRB(30, 30, 60, 60));
  EXPECT_TRUE(bounds.Contains(skity::Rect::MakeLTRB(30, 30, 60, 60)));

  bounds = CalculateDisplayListBounds(
      /*cull_rect=*/skity::Rect::MakeLTRB(0, 0, 100, 100),
      [](skity::Canvas* canvas) {
        canvas->ClipRect(skity::Rect::MakeLTRB(40, 20, 70, 50));
        skity::Path path;
        path.MoveTo(30, 30);
        path.LineTo(60, 60);
        path.LineTo(30, 60);
        path.Close();
        canvas->DrawPath(path, skity::Paint{});
      });
  EXPECT_EQ(bounds, skity::Rect::MakeLTRB(40, 30, 60, 50));

  bounds = CalculateDisplayListBounds(
      /*cull_rect=*/skity::Rect::MakeLTRB(0, 0, 200, 200),
      [](skity::Canvas* canvas) {
        canvas->Scale(2, 2);
        skity::Path path;
        path.MoveTo(30, 30);
        path.LineTo(60, 60);
        path.LineTo(30, 60);
        path.Close();
        canvas->DrawPath(path, skity::Paint{});
      });
  EXPECT_EQ(bounds, skity::Rect::MakeLTRB(60, 60, 120, 120));

  bounds = CalculateDisplayListBounds(
      /*cull_rect=*/skity::Rect::MakeLTRB(0, 0, 200, 200),
      [](skity::Canvas* canvas) {
        canvas->ClipRect(skity::Rect::MakeEmpty());
        skity::Paint paint;
        paint.SetColor(skity::Color_RED);
        canvas->DrawPaint(paint);
      });
  EXPECT_EQ(bounds, skity::Rect::MakeEmpty());
}

TEST(DisplayList, ChangeOpPaint) {
  skity::Paint red_paint;
  red_paint.SetColor(skity::Color_RED);
  skity::Paint blue_paint;
  blue_paint.SetColor(skity::Color_BLUE);
  skity::Paint yellow_paint;
  yellow_paint.SetColor(skity::Color_YELLOW);

  skity::PictureRecorder recorder;
  recorder.BeginRecording();
  auto canvas = recorder.GetRecordingCanvas();
  skity::RecordedOpOffset offset1 = canvas->GetLastOpOffset();
  EXPECT_FALSE(offset1.IsValid());

  canvas->DrawRect(skity::Rect::MakeLTRB(0, 0, 100, 100), red_paint);
  skity::RecordedOpOffset offset2 = canvas->GetLastOpOffset();
  EXPECT_TRUE(offset2.IsValid());
  EXPECT_EQ(offset2.GetValue(), 0);
  canvas->DrawCircle(50, 50, 30, red_paint);
  skity::RecordedOpOffset offset3 = canvas->GetLastOpOffset();
  EXPECT_TRUE(offset3.IsValid());

  auto display_list = recorder.FinishRecording();

  MockCanvas mock_canvas;
  EXPECT_CALL(mock_canvas, OnDrawRect(_, red_paint)).Times(1);
  EXPECT_CALL(mock_canvas, OnDrawPath(_, red_paint)).Times(1);
  display_list->Draw(&mock_canvas);

  skity::Paint* paint1 = display_list->GetOpPaintByOffset(offset1);
  EXPECT_EQ(paint1, nullptr);

  skity::Paint* paint2 = display_list->GetOpPaintByOffset(offset2);
  EXPECT_NE(paint2, nullptr);
  EXPECT_EQ(paint2->GetColor(), skity::Color_RED);
  paint2->SetColor(skity::Color_BLUE);

  skity::Paint* paint3 = display_list->GetOpPaintByOffset(offset3);
  EXPECT_EQ(paint3->GetColor(), skity::Color_RED);
  paint3->SetColor(skity::Color_YELLOW);

  MockCanvas mock_canvas2;
  EXPECT_CALL(mock_canvas2, OnDrawRect(_, blue_paint)).Times(1);
  EXPECT_CALL(mock_canvas2, OnDrawPath(_, yellow_paint)).Times(1);
  display_list->Draw(&mock_canvas2);
}

TEST(DisplayList, SearchByRect) {
  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 200, 200), options);
  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  canvas->Translate(20, 10);
  canvas->DrawRect(skity::Rect::MakeLTRB(0, 0, 20, 20), skity::Paint{});
  auto translated_rect_offset = canvas->GetLastOpOffset();
  canvas->Restore();

  canvas->Save();
  canvas->ClipRect(skity::Rect::MakeLTRB(60, 60, 90, 90));
  canvas->DrawRect(skity::Rect::MakeLTRB(50, 50, 120, 120), skity::Paint{});
  auto clipped_rect_offset = canvas->GetLastOpOffset();
  canvas->Restore();

  canvas->DrawRect(skity::Rect::MakeLTRB(140, 140, 180, 180), skity::Paint{});
  auto far_rect_offset = canvas->GetLastOpOffset();

  auto display_list = recorder.FinishRecording();

  EXPECT_THAT(SearchDisplayListOffsets(display_list.get(),
                                       skity::Rect::MakeLTRB(25, 15, 35, 25)),
              testing::ElementsAre(translated_rect_offset.GetValue()));
  EXPECT_THAT(SearchDisplayListOffsets(display_list.get(),
                                       skity::Rect::MakeLTRB(65, 65, 75, 75)),
              testing::ElementsAre(clipped_rect_offset.GetValue()));
  EXPECT_THAT(
      SearchDisplayListOffsets(display_list.get(),
                               skity::Rect::MakeLTRB(145, 145, 150, 150)),
      testing::ElementsAre(far_rect_offset.GetValue()));
  EXPECT_THAT(SearchDisplayListOffsets(display_list.get(),
                                       skity::Rect::MakeLTRB(0, 0, 55, 55)),
              testing::ElementsAre(translated_rect_offset.GetValue()));
  EXPECT_TRUE(SearchDisplayListOffsets(display_list.get(),
                                       skity::Rect::MakeLTRB(0, 0, 10, 10))
                  .empty());
}

TEST(DisplayList, SearchWithoutRTree) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 100, 100));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->DrawRect(skity::Rect::MakeLTRB(10, 10, 40, 40), skity::Paint{});

  auto display_list = recorder.FinishRecording();
  EXPECT_TRUE(SearchDisplayListOffsets(display_list.get(),
                                       skity::Rect::MakeLTRB(20, 20, 30, 30))
                  .empty());
}

TEST(DisplayList, SearchNonOverlappingDrawnRects) {
  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 100, 100), options);
  auto canvas = recorder.GetRecordingCanvas();

  canvas->DrawRect(skity::Rect::MakeLTRB(0, 0, 20, 20), skity::Paint{});
  canvas->DrawRect(skity::Rect::MakeLTRB(10, 10, 30, 30), skity::Paint{});
  canvas->DrawRect(skity::Rect::MakeLTRB(40, 0, 50, 10), skity::Paint{});

  auto display_list = recorder.FinishRecording();

  EXPECT_THAT(SearchNonOverlappingDrawnRects(
                  display_list.get(), skity::Rect::MakeLTRB(15, 15, 16, 16)),
              testing::ElementsAre(skity::Rect::MakeLTRB(0, 0, 20, 10),
                                   skity::Rect::MakeLTRB(0, 10, 30, 20),
                                   skity::Rect::MakeLTRB(10, 20, 30, 30)));

  EXPECT_THAT(SearchNonOverlappingDrawnRects(
                  display_list.get(), skity::Rect::MakeLTRB(45, 5, 46, 6)),
              testing::ElementsAre(skity::Rect::MakeLTRB(40, 0, 50, 10)));
}

TEST(DisplayList, SearchNonOverlappingDrawnRectsWithoutRTree) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 100, 100));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->DrawRect(skity::Rect::MakeLTRB(0, 0, 20, 20), skity::Paint{});

  auto display_list = recorder.FinishRecording();
  EXPECT_TRUE(SearchNonOverlappingDrawnRects(
                  display_list.get(), skity::Rect::MakeLTRB(5, 5, 10, 10))
                  .empty());
}

TEST(DisplayList, SearchByRectManySizes) {
  constexpr int kMaxN = 24;

  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 500, 500), options);
  auto canvas = recorder.GetRecordingCanvas();

  std::vector<int32_t> offsets;
  offsets.reserve(kMaxN);
  for (int i = 0; i < kMaxN; ++i) {
    canvas->DrawRect(skity::Rect::MakeXYWH(i * 20, i * 20, 10, 10),
                     skity::Paint{});
    offsets.push_back(canvas->GetLastOpOffset().GetValue());
  }

  auto display_list = recorder.FinishRecording();
  EXPECT_TRUE(
      SearchDisplayListOffsets(display_list.get(), skity::Rect()).empty());

  for (int i = 0; i < kMaxN; ++i) {
    auto query = skity::Rect::MakeXYWH(i * 20 + 2, i * 20 + 2, 6, 6);
    EXPECT_THAT(SearchDisplayListOffsets(display_list.get(), query),
                testing::ElementsAre(offsets[i]));
    EXPECT_THAT(
        SearchNonOverlappingDrawnRects(display_list.get(), query),
        testing::ElementsAre(skity::Rect::MakeXYWH(i * 20, i * 20, 10, 10)));
  }
}

TEST(DisplayList, SearchByRectGrid) {
  constexpr int kRows = 4;
  constexpr int kCols = 4;

  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 100, 100), options);
  auto canvas = recorder.GetRecordingCanvas();

  int32_t offsets[kRows][kCols];
  for (int r = 0; r < kRows; ++r) {
    for (int c = 0; c < kCols; ++c) {
      canvas->DrawRect(skity::Rect::MakeXYWH(c * 20 + 5, r * 20 + 5, 10, 10),
                       skity::Paint{});
      offsets[r][c] = canvas->GetLastOpOffset().GetValue();
    }
  }

  auto display_list = recorder.FinishRecording();

  EXPECT_THAT(SearchDisplayListOffsets(display_list.get(),
                                       skity::Rect::MakeXYWH(27, 27, 6, 6)),
              testing::ElementsAre(offsets[1][1]));
  EXPECT_TRUE(SearchDisplayListOffsets(display_list.get(),
                                       skity::Rect::MakeXYWH(17, 17, 6, 6))
                  .empty());
  EXPECT_THAT(SearchDisplayListOffsets(display_list.get(),
                                       skity::Rect::MakeXYWH(14, 14, 12, 12)),
              testing::ElementsAre(offsets[0][0], offsets[0][1], offsets[1][0],
                                   offsets[1][1]));
  EXPECT_THAT(
      SearchNonOverlappingDrawnRects(display_list.get(),
                                     skity::Rect::MakeXYWH(14, 14, 12, 12)),
      testing::UnorderedElementsAre(skity::Rect::MakeXYWH(5, 5, 10, 10),
                                    skity::Rect::MakeXYWH(25, 5, 10, 10),
                                    skity::Rect::MakeXYWH(5, 25, 10, 10),
                                    skity::Rect::MakeXYWH(25, 25, 10, 10)));
}

TEST(DisplayList, SearchNonOverlappingDrawnRectsOverlappingRects) {
  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 80, 80), options);
  auto canvas = recorder.GetRecordingCanvas();

  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      const int x = 15 + 20 * c;
      const int y = 15 + 20 * r;
      canvas->DrawRect(skity::Rect::MakeLTRB(x - 15, y - 15, x + 15, y + 15),
                       skity::Paint{});
    }
  }

  auto display_list = recorder.FinishRecording();

  EXPECT_THAT(SearchNonOverlappingDrawnRects(
                  display_list.get(), skity::Rect::MakeLTRB(29, 34, 41, 36)),
              testing::ElementsAre(skity::Rect::MakeLTRB(0, 20, 70, 50)));
  EXPECT_THAT(SearchNonOverlappingDrawnRects(
                  display_list.get(), skity::Rect::MakeLTRB(34, 29, 36, 41)),
              testing::ElementsAre(skity::Rect::MakeLTRB(20, 0, 50, 70)));
  EXPECT_THAT(SearchNonOverlappingDrawnRects(
                  display_list.get(), skity::Rect::MakeLTRB(29, 29, 41, 41)),
              testing::ElementsAre(skity::Rect::MakeLTRB(0, 0, 70, 70)));
}

TEST(DisplayList, SearchNonOverlappingDrawnRectsRegion) {
  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 100, 100), options);
  auto canvas = recorder.GetRecordingCanvas();

  for (int i = 0; i < 9; ++i) {
    canvas->DrawRect(skity::Rect::MakeXYWH(i * 10, i * 10, 20, 20),
                     skity::Paint{});
  }

  auto display_list = recorder.FinishRecording();
  EXPECT_THAT(
      SearchNonOverlappingDrawnRects(
          display_list.get(), skity::Rect::MakeLTRB(-100, -100, 200, 200)),
      testing::ElementsAre(skity::Rect::MakeLTRB(0, 0, 20, 10),
                           skity::Rect::MakeLTRB(0, 10, 30, 20),
                           skity::Rect::MakeLTRB(10, 20, 40, 30),
                           skity::Rect::MakeLTRB(20, 30, 50, 40),
                           skity::Rect::MakeLTRB(30, 40, 60, 50),
                           skity::Rect::MakeLTRB(40, 50, 70, 60),
                           skity::Rect::MakeLTRB(50, 60, 80, 70),
                           skity::Rect::MakeLTRB(60, 70, 90, 80),
                           skity::Rect::MakeLTRB(70, 80, 100, 90),
                           skity::Rect::MakeLTRB(80, 90, 100, 100)));
}

TEST(DisplayList, SearchNonOverlappingDrawnRectsMergeTouchingRectangles) {
  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeLTRB(-20, -20, 30, 30), options);
  auto canvas = recorder.GetRecordingCanvas();

  const int starts[3] = {-10, 0, 10};
  for (int y : starts) {
    for (int x : starts) {
      canvas->DrawRect(skity::Rect::MakeXYWH(x, y, 10, 10), skity::Paint{});
    }
  }

  auto display_list = recorder.FinishRecording();
  EXPECT_THAT(
      SearchNonOverlappingDrawnRects(
          display_list.get(), skity::Rect::MakeLTRB(-100, -100, 100, 100)),
      testing::ElementsAre(skity::Rect::MakeLTRB(-10, -10, 20, 20)));
}

TEST(DisplayList, SearchNonOverlappingDrawnRectsOverlappingChain) {
  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 100, 100), options);
  auto canvas = recorder.GetRecordingCanvas();

  for (int i = 0; i < 6; ++i) {
    canvas->DrawRect(skity::Rect::MakeXYWH(i * 10, i * 10, 50, 50),
                     skity::Paint{});
  }

  auto display_list = recorder.FinishRecording();
  EXPECT_THAT(
      SearchNonOverlappingDrawnRects(
          display_list.get(), skity::Rect::MakeLTRB(-100, -100, 200, 200)),
      testing::ElementsAre(skity::Rect::MakeLTRB(0, 0, 50, 10),
                           skity::Rect::MakeLTRB(0, 10, 60, 20),
                           skity::Rect::MakeLTRB(0, 20, 70, 30),
                           skity::Rect::MakeLTRB(0, 30, 80, 40),
                           skity::Rect::MakeLTRB(0, 40, 90, 50),
                           skity::Rect::MakeLTRB(10, 50, 100, 60),
                           skity::Rect::MakeLTRB(20, 60, 100, 70),
                           skity::Rect::MakeLTRB(30, 70, 100, 80),
                           skity::Rect::MakeLTRB(40, 80, 100, 90),
                           skity::Rect::MakeLTRB(50, 90, 100, 100)));
}

TEST(DisplayList, PartialReplayByRect) {
  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 200, 200), options);
  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  canvas->Translate(10, 5);
  canvas->DrawRect(skity::Rect::MakeLTRB(20, 20, 40, 40), skity::Paint{});
  canvas->Restore();
  canvas->DrawRect(skity::Rect::MakeLTRB(100, 100, 140, 140), skity::Paint{});

  auto display_list = recorder.FinishRecording();

  MockCanvas mock_canvas;
  EXPECT_CALL(mock_canvas, OnSave()).Times(1);
  EXPECT_CALL(mock_canvas, OnTranslate(10, 5)).Times(1);
  EXPECT_CALL(mock_canvas, OnDrawRect(skity::Rect::MakeLTRB(20, 20, 40, 40), _))
      .Times(1);
  EXPECT_CALL(mock_canvas,
              OnDrawRect(skity::Rect::MakeLTRB(100, 100, 140, 140), _))
      .Times(0);
  EXPECT_CALL(mock_canvas, OnRestore()).Times(1);

  display_list->Draw(&mock_canvas, skity::Rect::MakeLTRB(30, 30, 35, 35));
}

TEST(DisplayList, PartialReplayRestoreToCount) {
  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 200, 200), options);
  auto canvas = recorder.GetRecordingCanvas();

  int outer_save_count = canvas->Save();
  canvas->Translate(10, 0);
  canvas->Save();
  canvas->ClipRect(skity::Rect::MakeLTRB(0, 0, 50, 50));
  canvas->DrawRect(skity::Rect::MakeLTRB(10, 10, 30, 30), skity::Paint{});
  canvas->RestoreToCount(outer_save_count);
  canvas->DrawRect(skity::Rect::MakeLTRB(120, 120, 150, 150), skity::Paint{});

  auto display_list = recorder.FinishRecording();

  MockCanvas mock_canvas;
  EXPECT_CALL(mock_canvas, OnSave()).Times(2);
  EXPECT_CALL(mock_canvas, OnTranslate(10, 0)).Times(1);
  EXPECT_CALL(mock_canvas, OnClipRect(skity::Rect::MakeLTRB(0, 0, 50, 50),
                                      skity::Canvas::ClipOp::kIntersect))
      .Times(1);
  EXPECT_CALL(mock_canvas, OnDrawRect(skity::Rect::MakeLTRB(10, 10, 30, 30), _))
      .Times(1);
  EXPECT_CALL(mock_canvas,
              OnDrawRect(skity::Rect::MakeLTRB(120, 120, 150, 150), _))
      .Times(0);
  EXPECT_CALL(mock_canvas, OnRestore()).Times(2);
  EXPECT_CALL(mock_canvas, OnRestoreToCount(_)).Times(0);

  display_list->Draw(&mock_canvas, skity::Rect::MakeLTRB(20, 20, 25, 25));
}

TEST(DisplayList, PartialReplayRestoreToCountSkipsIrrelevantScope) {
  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 200, 200), options);
  auto canvas = recorder.GetRecordingCanvas();

  int outer_save_count = canvas->Save();
  canvas->Translate(10, 0);
  canvas->Save();
  canvas->ClipRect(skity::Rect::MakeLTRB(0, 0, 50, 50));
  canvas->DrawRect(skity::Rect::MakeLTRB(10, 10, 30, 30), skity::Paint{});
  canvas->RestoreToCount(outer_save_count);
  canvas->DrawRect(skity::Rect::MakeLTRB(120, 120, 150, 150), skity::Paint{});

  auto display_list = recorder.FinishRecording();

  testing::StrictMock<MockCanvas> mock_canvas;
  EXPECT_CALL(mock_canvas, OnSave()).Times(0);
  EXPECT_CALL(mock_canvas, OnTranslate(_, _)).Times(0);
  EXPECT_CALL(mock_canvas, OnClipRect(_, _)).Times(0);
  EXPECT_CALL(mock_canvas, OnRestore()).Times(0);
  EXPECT_CALL(mock_canvas, OnRestoreToCount(_)).Times(0);
  EXPECT_CALL(mock_canvas,
              OnDrawRect(skity::Rect::MakeLTRB(120, 120, 150, 150), _))
      .Times(1);

  display_list->Draw(&mock_canvas, skity::Rect::MakeLTRB(125, 125, 130, 130));
}

TEST(DisplayList, PartialReplaySaveLayerScope) {
  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 200, 200), options);
  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint layer_paint;
  layer_paint.SetColor(skity::Color_RED);
  canvas->SaveLayer(skity::Rect::MakeLTRB(0, 0, 100, 100), layer_paint);
  canvas->Translate(10, 5);
  canvas->DrawRect(skity::Rect::MakeLTRB(20, 20, 40, 40), skity::Paint{});
  canvas->Restore();
  canvas->DrawRect(skity::Rect::MakeLTRB(140, 140, 180, 180), skity::Paint{});

  auto display_list = recorder.FinishRecording();

  MockCanvas mock_canvas;
  EXPECT_CALL(mock_canvas,
              OnSaveLayer(skity::Rect::MakeLTRB(0, 0, 100, 100), layer_paint))
      .Times(1);
  EXPECT_CALL(mock_canvas, OnTranslate(10, 5)).Times(1);
  EXPECT_CALL(mock_canvas, OnDrawRect(skity::Rect::MakeLTRB(20, 20, 40, 40), _))
      .Times(1);
  EXPECT_CALL(mock_canvas,
              OnDrawRect(skity::Rect::MakeLTRB(140, 140, 180, 180), _))
      .Times(0);
  EXPECT_CALL(mock_canvas, OnRestore()).Times(1);

  display_list->Draw(&mock_canvas, skity::Rect::MakeLTRB(30, 30, 35, 35));
}

TEST(DisplayList, PartialReplayMultipleHitsInSameScope) {
  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 200, 200), options);
  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  canvas->Translate(10, 5);
  canvas->DrawRect(skity::Rect::MakeLTRB(20, 20, 40, 40), skity::Paint{});
  canvas->DrawRect(skity::Rect::MakeLTRB(50, 20, 70, 40), skity::Paint{});
  canvas->Restore();
  canvas->DrawRect(skity::Rect::MakeLTRB(120, 120, 160, 160), skity::Paint{});

  auto display_list = recorder.FinishRecording();

  testing::StrictMock<MockCanvas> mock_canvas;
  EXPECT_CALL(mock_canvas, OnSave()).Times(1);
  EXPECT_CALL(mock_canvas, OnTranslate(10, 5)).Times(1);
  EXPECT_CALL(mock_canvas, OnDrawRect(skity::Rect::MakeLTRB(20, 20, 40, 40), _))
      .Times(1);
  EXPECT_CALL(mock_canvas, OnDrawRect(skity::Rect::MakeLTRB(50, 20, 70, 40), _))
      .Times(1);
  EXPECT_CALL(mock_canvas,
              OnDrawRect(skity::Rect::MakeLTRB(120, 120, 160, 160), _))
      .Times(0);
  EXPECT_CALL(mock_canvas, OnRestore()).Times(1);

  display_list->Draw(&mock_canvas, skity::Rect::MakeLTRB(30, 25, 65, 30));
}

TEST(DisplayList, PartialReplayOuterScopeContinuesAfterInnerRestore) {
  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 200, 200), options);
  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  canvas->Translate(10, 0);
  canvas->Save();
  canvas->ClipRect(skity::Rect::MakeLTRB(0, 0, 40, 40));
  canvas->DrawRect(skity::Rect::MakeLTRB(10, 10, 30, 30), skity::Paint{});
  canvas->Restore();
  canvas->DrawRect(skity::Rect::MakeLTRB(80, 10, 110, 40), skity::Paint{});
  canvas->Restore();

  auto display_list = recorder.FinishRecording();

  testing::StrictMock<MockCanvas> mock_canvas;
  EXPECT_CALL(mock_canvas, OnSave()).Times(1);
  EXPECT_CALL(mock_canvas, OnTranslate(10, 0)).Times(1);
  EXPECT_CALL(mock_canvas, OnClipRect(_, _)).Times(0);
  EXPECT_CALL(mock_canvas, OnDrawRect(skity::Rect::MakeLTRB(10, 10, 30, 30), _))
      .Times(0);
  EXPECT_CALL(mock_canvas,
              OnDrawRect(skity::Rect::MakeLTRB(80, 10, 110, 40), _))
      .Times(1);
  EXPECT_CALL(mock_canvas, OnRestore()).Times(1);

  display_list->Draw(&mock_canvas, skity::Rect::MakeLTRB(95, 20, 100, 25));
}

TEST(DisplayList, PartialReplayWithoutRTree) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 100, 100));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->DrawRect(skity::Rect::MakeLTRB(10, 10, 40, 40), skity::Paint{});

  auto display_list = recorder.FinishRecording();
  MockCanvas mock_canvas;

  EXPECT_CALL(mock_canvas, OnDrawRect(skity::Rect::MakeLTRB(10, 10, 40, 40), _))
      .Times(1);
  display_list->Draw(&mock_canvas, skity::Rect::MakeLTRB(20, 20, 30, 30));
}

TEST(DisplayList, PartialReplayEmptyRectNoOp) {
  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 200, 200), options);
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Save();
  canvas->Translate(10, 5);
  canvas->DrawRect(skity::Rect::MakeLTRB(20, 20, 40, 40), skity::Paint{});
  canvas->Restore();

  auto display_list = recorder.FinishRecording();
  testing::StrictMock<MockCanvas> mock_canvas;

  display_list->Draw(&mock_canvas, skity::Rect::MakeEmpty());
}

TEST(DisplayList, DrawGlyphsWithZeroCount) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 100, 100));
  auto canvas = recorder.GetRecordingCanvas();
  skity::Font font;
  skity::Paint paint;

  canvas->DrawGlyphs(0, nullptr, nullptr, nullptr, font, paint);

  auto display_list = recorder.FinishRecording();
  testing::StrictMock<MockCanvas> mock_canvas;

  EXPECT_CALL(mock_canvas, OnDrawGlyphs(0, _, _, _, _, _)).Times(1);
  display_list->Draw(&mock_canvas);
}

TEST(DisplayList, DrawNullCanvasNoOp) {
  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 100, 100), options);
  auto canvas = recorder.GetRecordingCanvas();
  canvas->DrawRect(skity::Rect::MakeLTRB(10, 10, 40, 40), skity::Paint{});

  auto display_list = recorder.FinishRecording();
  display_list->Draw(nullptr);
  display_list->Draw(nullptr, skity::Rect::MakeLTRB(10, 10, 20, 20));
}

TEST(DisplayList, ClipRect) {
  {
    skity::PictureRecorder recorder;
    recorder.BeginRecording();
    auto canvas = recorder.GetRecordingCanvas();

    canvas->ClipRect(skity::Rect::MakeLTRB(0, 0, 100, 100),
                     skity::Canvas::ClipOp::kIntersect);
    auto display_list = recorder.FinishRecording();
    MockCanvas mock_canvas;
    EXPECT_CALL(mock_canvas, OnClipRect(skity::Rect::MakeLTRB(0, 0, 100, 100),
                                        skity::Canvas::ClipOp::kIntersect))
        .Times(1);
    display_list->Draw(&mock_canvas);
  }
  {
    skity::PictureRecorder recorder;
    recorder.BeginRecording();
    auto canvas = recorder.GetRecordingCanvas();

    canvas->ClipRect(skity::Rect::MakeLTRB(0, 0, 100, 100),
                     skity::Canvas::ClipOp::kDifference);
    auto display_list = recorder.FinishRecording();
    MockCanvas mock_canvas;
    EXPECT_CALL(mock_canvas, OnClipRect(skity::Rect::MakeLTRB(0, 0, 100, 100),
                                        skity::Canvas::ClipOp::kDifference))
        .Times(1);
    display_list->Draw(&mock_canvas);
  }
}

TEST(DisplayList, ClipRRect) {
  {
    skity::PictureRecorder recorder;
    recorder.BeginRecording();
    auto canvas = recorder.GetRecordingCanvas();

    canvas->ClipRRect(
        skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 100), 10, 10),
        skity::Canvas::ClipOp::kIntersect);
    auto display_list = recorder.FinishRecording();
    MockCanvas mock_canvas;
    EXPECT_CALL(mock_canvas,
                OnClipRRect(skity::RRect::MakeRectXY(
                                skity::Rect::MakeLTRB(0, 0, 100, 100), 10, 10),
                            skity::Canvas::ClipOp::kIntersect))
        .Times(1);
    display_list->Draw(&mock_canvas);
  }
  {
    skity::PictureRecorder recorder;
    recorder.BeginRecording();
    auto canvas = recorder.GetRecordingCanvas();

    canvas->ClipRRect(
        skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 100), 10, 10),
        skity::Canvas::ClipOp::kDifference);
    auto display_list = recorder.FinishRecording();
    MockCanvas mock_canvas;
    EXPECT_CALL(mock_canvas,
                OnClipRRect(skity::RRect::MakeRectXY(
                                skity::Rect::MakeLTRB(0, 0, 100, 100), 10, 10),
                            skity::Canvas::ClipOp::kDifference))
        .Times(1);
    display_list->Draw(&mock_canvas);
  }
}

TEST(DisplayList, DrawDRRect) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording();
  auto canvas = recorder.GetRecordingCanvas();
  skity::Paint paint;
  canvas->DrawDRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 100), 10, 10),
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(10, 10, 90, 90), 5, 5),
      paint);
  auto display_list = recorder.FinishRecording();
  MockCanvas mock_canvas;
  EXPECT_CALL(mock_canvas,
              OnDrawDRRect(skity::RRect::MakeRectXY(
                               skity::Rect::MakeLTRB(0, 0, 100, 100), 10, 10),
                           skity::RRect::MakeRectXY(
                               skity::Rect::MakeLTRB(10, 10, 90, 90), 5, 5),
                           paint))
      .Times(1);
  display_list->Draw(&mock_canvas);
}

TEST(DisplayList, CheckProperties) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 100, 100));
  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  canvas->DrawRect(skity::Rect::MakeWH(10, 10), paint);

  auto display_list = recorder.FinishRecording();
  EXPECT_FALSE(display_list->HasSaveLayer());
  EXPECT_FALSE(display_list->HasShader());
  EXPECT_FALSE(display_list->HasColorFilter());
  EXPECT_FALSE(display_list->HasMaskFilter());
  EXPECT_FALSE(display_list->HasImageFilter());

  // Test SaveLayer
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 100, 100));
  canvas = recorder.GetRecordingCanvas();
  canvas->SaveLayer(skity::Rect::MakeWH(100, 100), paint);
  canvas->Restore();
  display_list = recorder.FinishRecording();
  EXPECT_TRUE(display_list->HasSaveLayer());

  // Test Shader
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 100, 100));
  canvas = recorder.GetRecordingCanvas();
  skity::Point pts[] = {skity::Point(0.f, 0.f, 0.f, 1.f),
                        skity::Point(100.f, 100.f, 0.f, 1.f)};
  skity::Vec4 colors[] = {skity::Vec4(1.f, 0.f, 0.f, 1.f),
                          skity::Vec4(0.f, 0.f, 1.f, 1.f)};
  paint.SetShader(skity::Shader::MakeLinear(pts, colors, nullptr, 2));
  canvas->DrawRect(skity::Rect::MakeWH(10, 10), paint);
  display_list = recorder.FinishRecording();
  EXPECT_TRUE(display_list->HasShader());
  paint.SetShader(nullptr);

  // Test ColorFilter
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 100, 100));
  canvas = recorder.GetRecordingCanvas();
  paint.SetColorFilter(
      skity::ColorFilters::Blend(skity::Color_RED, skity::BlendMode::kSrc));
  canvas->DrawRect(skity::Rect::MakeWH(10, 10), paint);
  display_list = recorder.FinishRecording();
  EXPECT_TRUE(display_list->HasColorFilter());
  paint.SetColorFilter(nullptr);

  // Test MaskFilter
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 100, 100));
  canvas = recorder.GetRecordingCanvas();
  // Using MakeBlur with enum value for Normal style
  paint.SetMaskFilter(
      skity::MaskFilter::MakeBlur(skity::BlurStyle::kNormal, 10));
  canvas->DrawRect(skity::Rect::MakeWH(10, 10), paint);
  display_list = recorder.FinishRecording();
  EXPECT_TRUE(display_list->HasMaskFilter());
  paint.SetMaskFilter(nullptr);

  // Test ImageFilter
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 100, 100));
  canvas = recorder.GetRecordingCanvas();
  paint.SetImageFilter(skity::ImageFilters::Blur(10, 10));
  canvas->DrawRect(skity::Rect::MakeWH(10, 10), paint);
  display_list = recorder.FinishRecording();
  EXPECT_TRUE(display_list->HasImageFilter());
  paint.SetImageFilter(nullptr);
}

TEST(DisplayList, CheckPropertiesWithDrawPaint) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeLTRB(0, 0, 100, 100));
  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  skity::Point pts[] = {skity::Point(0.f, 0.f, 0.f, 1.f),
                        skity::Point(100.f, 100.f, 0.f, 1.f)};
  skity::Vec4 colors[] = {skity::Vec4(1.f, 0.f, 0.f, 1.f),
                          skity::Vec4(0.f, 0.f, 1.f, 1.f)};
  paint.SetShader(skity::Shader::MakeLinear(pts, colors, nullptr, 2));

  canvas->DrawPaint(paint);

  auto display_list = recorder.FinishRecording();
  EXPECT_TRUE(display_list->HasShader());
}
