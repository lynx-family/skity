// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Smoke tests for the header-only RAII wrapper (skity_hpp/): compile-time
// surface (every wrapped entry point is called once) plus CPU-only behavior
// spot checks. This TU must not include the legacy headers under
// include/skity/ — see the namespace-collision note in skity_hpp/skity.hpp.

#include <gtest/gtest.h>
#include <skity_hpp/skity.hpp>

#include <cstdint>
#include <vector>

namespace {

using skity::raii::Bitmap;
using skity::raii::BlendMode;
using skity::raii::BlurStyle;
using skity::raii::Canvas;
using skity::raii::ColorFilter;
using skity::raii::ColorPackRGBA;
using skity::raii::Color_BLACK;
using skity::raii::Color_WHITE;
using skity::raii::Data;
using skity::raii::DisplayList;
using skity::raii::DisplayListBuildOptions;
using skity::raii::Font;
using skity::raii::FontManager;
using skity::raii::Image;
using skity::raii::ImageFilter;
using skity::raii::MaskFilter;
using skity::raii::Matrix;
using skity::raii::Op;
using skity::raii::Paint;
using skity::raii::Path;
using skity::raii::PathEffect;
using skity::raii::PathMeasure;
using skity::raii::PathOp;
using skity::raii::PictureRecorder;
using skity::raii::Pixmap;
using skity::raii::Rect;
using skity::raii::RRect;
using skity::raii::Shader;
using skity::raii::StrokePath;
using skity::raii::TextBlob;
using skity::raii::Typeface;
using skity::raii::Vec2;

constexpr Rect kBounds = Rect::MakeWH(1000.f, 1000.f);

Paint RedStrokePaint() {
  Paint paint;
  paint.SetStyle(Paint::Style::kStroke);
  paint.SetStrokeWidth(4.f);
  paint.SetColor(ColorPackRGBA(1.f, 0.f, 0.f, 1.f));
  return paint;
}

}  // namespace

TEST(WrapperHpp, ValueTypes) {
  Rect r = Rect::MakeXYWH(10.f, 20.f, 30.f, 40.f);
  EXPECT_FLOAT_EQ(r.Width(), 30.f);
  EXPECT_FLOAT_EQ(r.CenterX(), 25.f);
  r.Offset(1.f, 1.f);
  EXPECT_FLOAT_EQ(r.Left(), 11.f);
  EXPECT_TRUE(Rect::MakeWH(0.f, 0.f).IsEmpty());

  Matrix m = Matrix::Translate(3.f, 4.f);
  m = Matrix::Concat(m, Matrix::Scale(2.f, 2.f));
  EXPECT_FLOAT_EQ(m.GetScaleX(), 2.f);
  EXPECT_FLOAT_EQ(m.GetTranslateX(), 3.f);
  auto pt = m.MapPoint(1.f, 1.f);
  EXPECT_FLOAT_EQ(pt.e[0], 5.f);
  EXPECT_FLOAT_EQ(pt.e[1], 6.f);
  auto mapped = m.MapRect(Rect::MakeXYWH(0.f, 0.f, 1.f, 1.f));
  EXPECT_FLOAT_EQ(mapped.Right(), 5.f);

  RRect rrect;
  rrect.SetOval(Rect::MakeWH(20.f, 10.f));
  EXPECT_FLOAT_EQ(rrect.GetRadii()[0].e[0], 10.f);
}

TEST(WrapperHpp, PaintEffectsRoundTrip) {
  Paint paint;
  paint.SetAntiAlias(true);
  EXPECT_TRUE(paint.IsAntiAlias());
  paint.SetBlendMode(BlendMode::kMultiply);
  EXPECT_EQ(paint.GetBlendMode(), BlendMode::kMultiply);
  paint.SetStrokeCap(Paint::Cap::kRound);
  EXPECT_EQ(paint.GetStrokeCap(), Paint::Cap::kRound);
  paint.SetAlphaF(0.5f);
  EXPECT_NEAR(paint.GetAlpha(), 128, 1);

  const skity::raii::Color4f stops[] = {
      {0.f, 0.f, 0.f, 1.f},
      {1.f, 1.f, 1.f, 1.f},
  };
  const float pos[] = {0.f, 1.f};
  const skity::raii::Point pts[] = {
      {0.f, 0.f, 0.f, 1.f},
      {100.f, 0.f, 0.f, 1.f},
  };
  Shader shader = Shader::MakeLinear(pts, stops, pos, 2);
  ASSERT_TRUE(shader);
  paint.SetShader(shader);
  Shader got = paint.GetShader();
  EXPECT_TRUE(got);
  // The getter owns a shared reference to the SAME shader object, so the
  // local matrix set through it is visible through the paint's reference.
  got.SetLocalMatrix(Matrix::Translate(1.f, 2.f));
  EXPECT_FLOAT_EQ(paint.GetShader().GetLocalMatrix().GetTranslateX(), 1.f);

  paint.SetMaskFilter(MaskFilter::MakeBlur(BlurStyle::kNormal, 5.f));
  EXPECT_TRUE(paint.GetMaskFilter());
  paint.SetColorFilter(ColorFilter::Blend(Color_WHITE, BlendMode::kSrcOver));
  EXPECT_TRUE(paint.GetColorFilter());
  const float matrix20[20] = {
      0.3f, 0.6f, 0.1f, 0.f, 0.f,  //
      0.3f, 0.6f, 0.1f, 0.f, 0.f,  //
      0.3f, 0.6f, 0.1f, 0.f, 0.f,  //
      0.f, 0.f, 0.f, 1.f, 0.f,
  };
  paint.SetColorFilter(ColorFilter::Matrix(matrix20));
  paint.SetImageFilter(ImageFilter::Blur(2.f, 2.f));
  EXPECT_TRUE(paint.GetImageFilter());
  paint.SetPathEffect(PathEffect::MakeDashPathEffect(pos, 2));
  EXPECT_TRUE(paint.GetPathEffect());

  // Getter wrappers own a shared reference: they outlive the paint.
  ImageFilter filter = paint.GetImageFilter();
  paint.Reset();
  EXPECT_TRUE(filter);
}

TEST(WrapperHpp, ShaderFactories) {
  const skity::raii::Color4f stops[] = {
      {0.f, 0.f, 1.f, 1.f},
      {1.f, 0.f, 0.f, 1.f},
  };
  const skity::raii::Point pts[] = {
      {0.f, 0.f, 0.f, 1.f},
      {100.f, 100.f, 0.f, 1.f},
  };
  ASSERT_TRUE(Shader::MakeRadial(pts[0], 50.f, stops, nullptr, 2));
  ASSERT_TRUE(Shader::MakeSweep(50.f, 50.f, 0.f, 360.f, stops, nullptr, 2));
  ASSERT_TRUE(Shader::MakeTwoPointConical(pts[0], 0.f, pts[1], 60.f, stops,
                                          nullptr, 2));

  Image image = Image::MakeFromPixels(
      2, 2, stops, 0, skity::raii::AlphaType::kUnpremul_AlphaType,
      skity::raii::ColorType::kRGBA);
  ASSERT_TRUE(image);
  EXPECT_EQ(image.Width(), 2u);
  EXPECT_TRUE(Shader::MakeShader(image));
  EXPECT_TRUE(
      Shader::MakeShader(image, skity::raii::SamplingOptions(
                                    skity::raii::FilterMode::kLinear,
                                    skity::raii::MipmapMode::kNone)));
}

TEST(WrapperHpp, PathConstructionAndQueries) {
  Path path;
  path.MoveTo(10.f, 10.f).LineTo(90.f, 10.f).LineTo(90.f, 90.f).Close();
  EXPECT_EQ(path.CountVerbs(), 4u);
  EXPECT_EQ(path.CountPoints(), 3u);
  EXPECT_EQ(path.GetVerb(0), Path::Verb::kMove);
  EXPECT_EQ(path.GetVerb(1), Path::Verb::kLine);
  EXPECT_FLOAT_EQ(path.GetPoint(1).e[0], 90.f);
  EXPECT_TRUE(path.GetConvexityType() != Path::ConvexityType::kConcave);

  Path arc;
  arc.MoveTo(50.f, 10.f);
  arc.ArcTo(Rect::MakeWH(100.f, 100.f), 0.f, 90.f, true);
  arc.MoveTo(0.f, 0.f);
  arc.ArcTo(30.f, 30.f, 0.f, Path::ArcSize::kLarge, Path::Direction::kCW,
            100.f, 100.f);
  arc.ArcTo(0.f, 0.f, 100.f, 0.f, 25.f);
  EXPECT_FALSE(arc.IsEmpty());

  Path round;
  const Vec2 radii[4] = {
      Vec2{4.f, 4.f}, Vec2{8.f, 8.f}, Vec2{12.f, 12.f}, Vec2{16.f, 16.f}};
  round.AddRoundRect(Rect::MakeWH(100.f, 80.f), radii);
  Rect as_rect;
  EXPECT_FALSE(round.IsRect(&as_rect));
  round.AddRect(Rect::MakeWH(10.f, 10.f));
  EXPECT_TRUE(round.GetSegmentMasks() != 0u);

  Path rect_path;
  rect_path.AddRect(Rect::MakeWH(30.f, 20.f));
  Rect out_rect;
  ASSERT_TRUE(rect_path.IsRect(&out_rect));
  EXPECT_FLOAT_EQ(out_rect.Width(), 30.f);

  Path line_path;
  line_path.MoveTo(0.f, 0.f);
  line_path.LineTo(10.f, 10.f);
  // NOTE: legacy Path::IsLine (src/graphic/path.cc) unconditionally returns
  // false — an upstream bug — so the C wrapper never fills the out points
  // either. Call it once to keep the entry point covered.
  skity::raii::Point line_pts[2]{};
  EXPECT_FALSE(line_path.IsLine(line_pts));

  // Legacy operator== compares storage pointers, not contents: a deep clone
  // is never "equal"; only identity compares true.
  Path copy = rect_path.Clone();
  EXPECT_FALSE(copy.IsEqual(rect_path));
  EXPECT_TRUE(rect_path.IsEqual(rect_path));
  copy.Reset();
  EXPECT_TRUE(copy.IsEmpty());

  Path appended;
  appended.AddPath(rect_path, Path::AddMode::kExtend);
  EXPECT_EQ(appended.CountVerbs(), rect_path.CountVerbs());
  appended.ReverseAddPath(rect_path);
  EXPECT_GT(appended.CountVerbs(), rect_path.CountVerbs());

  Path scaled = rect_path.CopyWithScale(2.f);
  EXPECT_FLOAT_EQ(scaled.GetBounds().Width(), 60.f);
  Path moved = rect_path.CopyWithMatrix(Matrix::Translate(5.f, 0.f));
  EXPECT_FLOAT_EQ(moved.GetBounds().Left(), 5.f);

  // A closed rect contour ends on its (0, 0) move point.
  skity::raii::Point last;
  ASSERT_TRUE(rect_path.GetLastPt(&last));
  EXPECT_FLOAT_EQ(last.e[0], 0.f);
  rect_path.SetLastPt(40.f, 20.f);
  ASSERT_TRUE(rect_path.GetLastPt(&last));
  EXPECT_FLOAT_EQ(last.e[0], 40.f);

  Path closed;
  closed.MoveTo(0.f, 0.f);
  closed.Close();
  skity::raii::Point move_pt;
  closed.GetLastMovePt(&move_pt);
  EXPECT_FLOAT_EQ(move_pt.e[0], 0.f);
}

TEST(WrapperHpp, PathMeasureSampling) {
  Path line;
  line.MoveTo(0.f, 0.f);
  line.LineTo(100.f, 0.f);
  PathMeasure measure(line);
  EXPECT_FLOAT_EQ(measure.GetLength(), 100.f);
  skity::raii::Point position, tangent;
  ASSERT_TRUE(measure.GetPosTan(50.f, &position, &tangent));
  EXPECT_FLOAT_EQ(position.e[0], 50.f);
  EXPECT_FLOAT_EQ(tangent.e[0], 1.f);
  EXPECT_FALSE(measure.IsClosed());

  Path segment;
  ASSERT_TRUE(measure.GetSegment(10.f, 20.f, segment));
  EXPECT_FLOAT_EQ(segment.GetBounds().Left(), 10.f);
  EXPECT_FALSE(measure.NextContour());
}

TEST(WrapperHpp, PathBooleanOp) {
  Path a;
  a.AddRect(Rect::MakeXYWH(0.f, 0.f, 50.f, 50.f));
  Path b;
  b.AddRect(Rect::MakeXYWH(25.f, 0.f, 50.f, 50.f));
  Path result;
  ASSERT_TRUE(Op(a, b, PathOp::kUnion, &result));
  Rect bounds = result.GetBounds();
  EXPECT_FLOAT_EQ(bounds.Width(), 75.f);

  ASSERT_TRUE(Op(a, b, PathOp::kIntersect, &result));
  EXPECT_FLOAT_EQ(result.GetBounds().Width(), 25.f);
}

TEST(WrapperHpp, StrokeOutline) {
  Paint paint = RedStrokePaint();
  Path rect;
  rect.AddRect(Rect::MakeWH(40.f, 40.f));
  Path stroked;
  StrokePath(paint, rect, &stroked);
  Rect bounds = stroked.GetBounds();
  // 4px stroke centered on the geometry grows the bounds by 2px per side.
  EXPECT_FLOAT_EQ(bounds.Width(), 44.f);
  EXPECT_FLOAT_EQ(bounds.Height(), 44.f);

  Path quads;
  StrokePath(paint, rect, &quads);
  EXPECT_FALSE(quads.IsEmpty());
}

TEST(WrapperHpp, DataBitmapPixmap) {
  Data data = Data::MakeWithCopy("rgba", 4);
  ASSERT_TRUE(data);
  EXPECT_EQ(data.GetSize(), 4u);
  EXPECT_NE(data.GetData(), nullptr);
  EXPECT_TRUE(Data::MakeEmpty());

  Bitmap bitmap(4, 4);
  ASSERT_TRUE(bitmap);
  EXPECT_EQ(bitmap.Width(), 4u);
  EXPECT_EQ(bitmap.RowBytes(), 16u);
  EXPECT_NE(bitmap.GetPixels(), nullptr);
  Pixmap pixmap = bitmap.GetPixmap();
  ASSERT_TRUE(pixmap);
  EXPECT_EQ(pixmap.Width(), 4u);
  EXPECT_NE(pixmap.GetPixels(), nullptr);
  EXPECT_EQ(pixmap.SetColorInfo(skity::raii::AlphaType::kPremul_AlphaType,
                                skity::raii::ColorType::kRGBA),
            SKITY_SUCCESS);

  Data pixels = Data::MakeWithCopy("0123456789abcdef", 16);
  Pixmap wrapped = Pixmap::MakeFromData(
      pixels, 16, 4, 4, skity::raii::AlphaType::kUnpremul_AlphaType,
      skity::raii::ColorType::kRGBA);
  ASSERT_TRUE(wrapped);
  Bitmap from_pixmap = Bitmap::MakeFromPixmap(wrapped, true);
  ASSERT_TRUE(from_pixmap);
  EXPECT_EQ(from_pixmap.Width(), 4u);

  // The wrapped pixmap keeps the data alive beyond the Data wrapper.
  pixels = Data::MakeEmpty();
  EXPECT_NE(wrapped.GetPixels(), nullptr);
  EXPECT_EQ(from_pixmap.Height(), 4u);
}

TEST(WrapperHpp, ImageRaster) {
  const uint32_t pixels[4] = {0xFF0000FFu, 0xFF00FF00u, 0xFFFF0000u,
                              0xFFFFFFFFu};
  Image image = Image::MakeFromPixels(
      2, 2, pixels, 0, skity::raii::AlphaType::kUnpremul_AlphaType,
      skity::raii::ColorType::kRGBA);
  ASSERT_TRUE(image);
  EXPECT_EQ(image.Width(), 2u);
  EXPECT_EQ(image.Height(), 2u);

  Pixmap read = image.ReadPixels(skity::raii::Context());
  ASSERT_TRUE(read);
  EXPECT_EQ(read.Width(), 2u);
  const auto* out = static_cast<const uint32_t*>(read.GetPixels());
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out[0], 0xFF0000FFu);
}

TEST(WrapperHpp, ImageFilters) {
  ASSERT_TRUE(ImageFilter::Blur(1.f, 1.f));
  ASSERT_TRUE(ImageFilter::Dilate(1.f, 1.f));
  ASSERT_TRUE(ImageFilter::Erode(1.f, 1.f));
  ASSERT_TRUE(ImageFilter::MatrixTransform(Matrix::Scale(2.f, 2.f)));
  ASSERT_TRUE(ImageFilter::ColorFilter(ColorFilter::LinearToSRGBGamma()));
  ASSERT_TRUE(ColorFilter::SRGBToLinearGamma());
  ASSERT_TRUE(ColorFilter::Compose(ColorFilter::LinearToSRGBGamma(),
                                   ColorFilter::SRGBToLinearGamma()));
  ImageFilter blur = ImageFilter::Blur(1.f, 1.f);
  ASSERT_TRUE(ImageFilter::Compose(blur, blur));
  ASSERT_TRUE(ImageFilter::LocalMatrix(blur, Matrix::Translate(1.f, 1.f)));
  ASSERT_TRUE(ImageFilter::DropShadow(1.f, 1.f, 2.f, 2.f, Color_BLACK));
  ASSERT_TRUE(ImageFilter::DropShadow(1.f, 1.f, 2.f, 2.f, Color_BLACK, blur));
  ASSERT_TRUE(ImageFilter::DropShadow(1.f, 1.f, 2.f, 2.f, Color_BLACK, blur,
                                      Rect::MakeWH(10.f, 10.f)));
}

TEST(WrapperHpp, RecordAndReplay) {
  PictureRecorder recorder;
  DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(kBounds, options);
  Canvas canvas = recorder.GetRecordingCanvas();
  ASSERT_TRUE(canvas);

  Paint paint = RedStrokePaint();
  canvas.DrawRect(Rect::MakeXYWH(10.f, 10.f, 20.f, 20.f), paint);
  int32_t rect_offset = recorder.GetLastOpOffset();
  EXPECT_GE(rect_offset, 0);

  Path path;
  path.AddCircle(60.f, 60.f, 10.f);
  canvas.DrawPath(path, Paint());
  int32_t circle_offset = recorder.GetLastOpOffset();
  EXPECT_GT(circle_offset, rect_offset);

  DisplayList list = recorder.FinishRecording();
  ASSERT_TRUE(list);
  EXPECT_EQ(list.GetOpCount(), 2u);
  // Bounds cover the recorded content (stroked rect + circle), not the cull
  // rect.
  Rect bounds = list.GetBounds();
  EXPECT_GE(bounds.Width(), 60.f);
  EXPECT_LE(bounds.Width(), 80.f);
  EXPECT_FALSE(list.HasProperty(DisplayList::Property::kShader));

  // In-place paint mutation through the offset lookup changes the replay.
  Paint op_paint = list.GetOpPaintByOffset(rect_offset);
  ASSERT_TRUE(op_paint);
  op_paint.SetColor(Color_WHITE);

  PictureRecorder replay_target;
  replay_target.BeginRecording(kBounds);
  Canvas target = replay_target.GetRecordingCanvas();
  // The cull rect misses the circle at (50..70, 50..70): partial replay.
  list.Draw(target, Rect::MakeXYWH(0.f, 0.f, 30.f, 30.f));
  DisplayList partial = replay_target.FinishRecording();
  EXPECT_EQ(partial.GetOpCount(), 1u);

  std::vector<int32_t> offsets =
      list.Search(Rect::MakeXYWH(55.f, 55.f, 10.f, 10.f));
  EXPECT_EQ(offsets.size(), 1u);
  std::vector<Rect> damage = list.SearchNonOverlappingDrawnRects(
      Rect::MakeXYWH(0.f, 0.f, 1000.f, 1000.f));
  EXPECT_EQ(damage.size(), 2u);
}

#ifdef SKITY_FONT_DIR
TEST(WrapperHpp, TextFontTypeface) {
  Typeface typeface = Typeface::MakeFromFile(
      SKITY_FONT_DIR "fonts/resources/Roboto-Regular.ttf");
  ASSERT_TRUE(typeface);
  EXPECT_NE(typeface.UnicharToGlyph('A'), 0);
  uint32_t code_points[] = {'A', 'B'};
  uint16_t glyphs[2] = {};
  typeface.UnicharsToGlyphs(code_points, 2, glyphs);
  EXPECT_NE(glyphs[0], 0);

  Font font(typeface.get(), 32.f);
  EXPECT_FLOAT_EQ(font.GetSize(), 32.f);
  font.SetScaleX(1.5f);
  EXPECT_FLOAT_EQ(font.GetScaleX(), 1.5f);
  font.SetEdging(Font::Edging::kAntiAlias);
  EXPECT_EQ(font.GetEdging(), Font::Edging::kAntiAlias);
  font.SetHinting(Font::FontHinting::kFull);
  EXPECT_EQ(font.GetHinting(), Font::FontHinting::kFull);
  font.SetSubpixel(true);
  EXPECT_TRUE(font.IsSubpixel());
  font.SetEmbolden(true);
  EXPECT_TRUE(font.IsEmbolden());
  Font bigger = font.MakeWithSize(64.f);
  EXPECT_FLOAT_EQ(bigger.GetSize(), 64.f);
  EXPECT_TRUE(bigger.GetTypeface());

  skity_font_metrics metrics{};
  font.GetMetrics(&metrics);
  const uint16_t ids[] = {glyphs[0], glyphs[1]};
  float widths[2] = {};
  font.GetWidths(ids, 2, widths);
  EXPECT_GT(widths[0], 0.f);

  Paint paint;
  paint.SetTypeface(typeface.get());
  paint.SetTextSize(32.f);
  TextBlob blob = TextBlob::Make("AB", paint);
  ASSERT_TRUE(blob);
  EXPECT_GT(blob.GetBounds().Width(), 0.f);

  TextBlob glyphs_blob = TextBlob::MakeFromGlyphs(font, ids, nullptr, nullptr, 2);
  ASSERT_TRUE(glyphs_blob);
  const float pos_x[] = {0.f, 24.f};
  const float pos_y[] = {0.f, 0.f};
  Rect run_bounds = TextBlob::ComputeRunBounds(2, ids, pos_x, pos_y, font, paint);
  EXPECT_GT(run_bounds.Width(), 0.f);
}

TEST(WrapperHpp, FontManagerFamilies) {
  FontManager manager = FontManager::RefDefault();
  ASSERT_TRUE(manager);
  int32_t count = manager.GetFamilyCount();
  EXPECT_GE(count, 0);
  if (count > 0) {
    std::string name = manager.GetFamilyName(0);
    EXPECT_FALSE(name.empty());
    auto style_set = manager.CreateStyleSet(0);
    ASSERT_TRUE(style_set);
    EXPECT_GE(style_set.GetCount(), 0);
    std::string style_name;
    style_set.GetStyle(0, &style_name);
  }
  Typeface from_file =
      manager.MakeFromFile(SKITY_FONT_DIR "fonts/resources/Roboto-Regular.ttf");
  ASSERT_TRUE(from_file);
}
#endif  // SKITY_FONT_DIR

TEST(WrapperHpp, SoftwareCanvasDraws) {
  Bitmap bitmap(8, 8, skity::raii::AlphaType::kPremul_AlphaType);
  ASSERT_TRUE(bitmap);
  skity::raii::SoftwareCanvas canvas(bitmap.get());
  ASSERT_TRUE(canvas);
  canvas.DrawColor(Color_WHITE, BlendMode::kSrcOver);
  Paint paint;
  paint.SetColor(ColorPackRGBA(1.f, 0.f, 0.f, 1.f));
  canvas.DrawRect(Rect::MakeWH(4.f, 4.f), paint);
  {
    // RGBA bytes read back as little-endian words: red = 0xFF0000FF.
    const auto* px = static_cast<const uint32_t*>(bitmap.GetPixmap().GetPixels());
    ASSERT_NE(px, nullptr);
    EXPECT_EQ(px[0], 0xFF0000FFu);
    EXPECT_EQ(px[8 * 7 + 7], 0xFFFFFFFFu);
  }
  Path path;
  path.AddCircle(6.f, 6.f, 1.f);
  canvas.DrawPath(path, paint);
  canvas.DrawCircle(2.f, 2.f, 1.f, paint);
  canvas.DrawOval(Rect::MakeWH(4.f, 2.f), paint);
  canvas.DrawRoundRect(Rect::MakeWH(6.f, 6.f), 1.f, 1.f, paint);
  const Vec2 radii[4] = {
      Vec2{1.f, 1.f}, Vec2{1.f, 1.f}, Vec2{1.f, 1.f}, Vec2{1.f, 1.f}};
  canvas.DrawRRect(Rect::MakeWH(8.f, 8.f), radii, paint);
  RRect rrect;
  rrect.SetRect(Rect::MakeWH(8.f, 8.f));
  canvas.DrawRRect(rrect, paint);
  canvas.DrawDRRect(Rect::MakeWH(8.f, 8.f), 2.f, 2.f, Rect::MakeWH(4.f, 4.f),
                    1.f, 1.f, paint);
  canvas.DrawArc(Rect::MakeWH(8.f, 8.f), 0.f, 90.f, true, paint);
  canvas.DrawPoint(1.f, 1.f, paint);
  canvas.DrawLine(0.f, 0.f, 8.f, 8.f, paint);
  canvas.Save();
  canvas.ClipRect(Rect::MakeWH(4.f, 4.f));
  canvas.ClipRRect(Rect::MakeWH(6.f, 6.f), 1.f, 1.f);
  canvas.ClipPath(path, skity::raii::ClipOp::kDifference);
  int depth = canvas.SaveLayer(Rect::MakeWH(8.f, 8.f), paint);
  EXPECT_GT(depth, 1);
  canvas.Translate(1.f, 1.f);
  canvas.Scale(2.f, 2.f);
  canvas.Rotate(45.f);
  canvas.Rotate(45.f, 4.f, 4.f);
  canvas.Skew(0.1f, 0.1f);
  canvas.Concat(Matrix::Translate(1.f, 1.f));
  canvas.SetMatrix(Matrix::Scale(1.f, 1.f));
  EXPECT_TRUE(canvas.GetTotalMatrix().IsIdentity());
  canvas.ResetMatrix();
  // The 8x8 device bounds clip everything else away.
  EXPECT_TRUE(canvas.QuickReject(Rect::MakeXYWH(100.f, 100.f, 1.f, 1.f)));
  canvas.RestoreToCount(1);
  EXPECT_EQ(canvas.GetSaveCount(), 1);
  canvas.Flush();
}
