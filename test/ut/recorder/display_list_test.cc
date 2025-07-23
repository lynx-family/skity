// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <skity/recorder/picture_recorder.hpp>
#include <skity/skity.hpp>

using testing::_;

class MockCanvas : public skity::Canvas {
 public:
  MOCK_METHOD(void, OnClipRect,
              (skity::Rect const& rect, skity::Canvas::ClipOp op), (override));
  MOCK_METHOD(void, OnClipPath,
              (skity::Path const& path, skity::Canvas::ClipOp op), (override));

  MOCK_METHOD(void, OnSave, (), (override));
  MOCK_METHOD(void, OnRestore, (), (override));
  MOCK_METHOD(void, OnRestoreToCount, (int saveCount), (override));

  MOCK_METHOD(void, OnDrawRect,
              (skity::Rect const& rect, skity::Paint const& paint), (override));

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

  MOCK_METHOD(void, OnUpdateViewport, (uint32_t width, uint32_t height),
              (override));
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
