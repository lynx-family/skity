// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Smoke tests for the header-only RAII wrapper (skity_hpp/): compile-time
// surface (every wrapped entry point is called once) plus CPU-only behavior
// spot checks. This TU must not include the legacy headers under
// include/skity/ — see the namespace-collision note in skity_hpp/skity.hpp.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <skity_hpp/skity.hpp>
#include <vector>

namespace {

using skity::raii::Bitmap;
using skity::raii::BlendMode;
using skity::raii::BlurStyle;
using skity::raii::Camera;
using skity::raii::Canvas;
using skity::raii::Color_BLACK;
using skity::raii::Color_WHITE;
using skity::raii::ColorFilter;
using skity::raii::ColorPackRGBA;
using skity::raii::Data;
using skity::raii::DisplayList;
using skity::raii::DisplayListBuildOptions;
using skity::raii::Font;
using skity::raii::FontManager;
using skity::raii::GlyphBitmap;
using skity::raii::GlyphData;
using skity::raii::GlyphFormat;
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
using skity::raii::QuaternionAxisAngleToMatrix;
using skity::raii::QuaternionEulerToMatrix;
using skity::raii::Rect;
using skity::raii::RRect;
using skity::raii::Semaphore;
using skity::raii::Shader;
using skity::raii::StrokePath;
using skity::raii::TextBlob;
using skity::raii::Typeface;
using skity::raii::Vec2;
using skity::raii::Vec3;
using skity::raii::Vec4;

constexpr Rect kBounds = Rect::MakeWH(1000.f, 1000.f);

Paint RedStrokePaint() {
  Paint paint;
  paint.SetStyle(Paint::Style::kStroke);
  paint.SetStrokeWidth(4.f);
  paint.SetColor(ColorPackRGBA(1.f, 0.f, 0.f, 1.f));
  return paint;
}

}  // namespace

TEST(WrapperHpp, VkHandleAdoptionBridges) {
  // Raw-C handles (e.g. from skity_context_create_vk or a native-window
  // acquire) adopt into the RAII layer; empty stays empty.
  auto context = skity::raii::Context::Adopt(nullptr);
  EXPECT_FALSE(context);
  auto surface = skity::raii::Surface::Adopt(nullptr);
  EXPECT_FALSE(surface);
}

TEST(WrapperHpp, SemaphoreNeutralApi) {
  // Compile + NULL-path gate; real semaphores need a GPU context (Vulkan).
  Semaphore semaphore = Semaphore::Adopt(nullptr);
  EXPECT_FALSE(semaphore);
  EXPECT_EQ(semaphore.GetBackendType(),
            skity::raii::Context::BackendType::kNone);
}

TEST(WrapperHpp, MetalApiValidation) {
  // Deterministic without a Metal device: argument / null-handle paths hold
  // on every backend (builds without SKITY_MTL_BACKEND return
  // SKITY_ERROR_NOT_SUPPORTED from creation, the getters return NULL).
  skity::raii::Context context;  // empty
  EXPECT_EQ(skity::raii::CreateMtlContext(nullptr, nullptr, nullptr),
            SKITY_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(skity::raii::GetMtlDevice(context), nullptr);
  EXPECT_EQ(skity::raii::GetMtlCommandQueue(context), nullptr);

  skity_surface_create_info base = {};
  skity_surface_create_info_mtl ext = {};
  skity::raii::Surface surface;
  EXPECT_EQ(skity::raii::CreateMtlSurface(context, base, ext, &surface),
            SKITY_ERROR_INVALID_HANDLE);
  EXPECT_FALSE(surface);

  skity_backend_texture_info tex_base = {};
  skity_backend_texture_info_mtl tex_ext = {};
  EXPECT_FALSE(skity::raii::WrapMtlTexture(context, tex_base, tex_ext));
}

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

TEST(WrapperHpp, ValueTypeCopySemantics) {
  // Paint copy duplicates parameters and shares attached effects, matching
  // the copy semantics of the value-type skity::Paint.
  const skity::raii::Color4f stops[] = {
      {0.f, 0.f, 0.f, 1.f},
      {1.f, 1.f, 1.f, 1.f},
  };
  const float pos[] = {0.f, 1.f};
  const skity::raii::Point pts[] = {
      {0.f, 0.f, 0.f, 1.f},
      {100.f, 0.f, 0.f, 1.f},
  };
  Paint original;
  original.SetStyle(Paint::Style::kStroke);
  original.SetStrokeWidth(3.f);
  original.SetShader(Shader::MakeLinear(pts, stops, pos, 2));

  Paint copied = original;  // copy constructor
  EXPECT_EQ(copied.GetStyle(), Paint::Style::kStroke);
  copied.SetStrokeWidth(7.f);
  EXPECT_FLOAT_EQ(original.GetStrokeWidth(), 3.f);  // params are independent
  EXPECT_FLOAT_EQ(copied.GetStrokeWidth(), 7.f);

  // Attached effects are shared: mutating through one copy is visible via
  // the other (same underlying shader object).
  copied.GetShader().SetLocalMatrix(Matrix::Translate(5.f, 0.f));
  EXPECT_FLOAT_EQ(original.GetShader().GetLocalMatrix().GetTranslateX(), 5.f);

  Paint assigned;
  assigned = original;  // copy assignment
  EXPECT_FLOAT_EQ(assigned.GetStrokeWidth(), 3.f);

  Paint moved = std::move(assigned);  // move stays cheap
  EXPECT_FLOAT_EQ(moved.GetStrokeWidth(), 3.f);
  EXPECT_FALSE(assigned);  // moved-out wrapper is empty

  // Path copy is a deep geometry copy.
  Path path;
  path.MoveTo(0.f, 0.f).LineTo(10.f, 0.f);
  Path path_copy = path;
  path_copy.LineTo(10.f, 10.f);
  EXPECT_EQ(path.CountPoints(), 2u);
  EXPECT_EQ(path_copy.CountPoints(), 3u);
  Path path_assigned;
  path_assigned = path;
  EXPECT_TRUE(path_assigned.IsEqual(path));

  // Font copy duplicates parameters and shares the typeface.
  Font font;
  font.SetSize(12.f);
  Font font_copy = font;
  font_copy.SetSize(20.f);
  EXPECT_FLOAT_EQ(font.GetSize(), 12.f);
  EXPECT_FLOAT_EQ(font_copy.GetSize(), 20.f);

  Typeface tf = font.GetTypeface();
  if (tf) {  // default typeface is platform-dependent (may be absent)
    Font with_tf(tf.get(), 12.f);
    Font with_tf_copy = with_tf;
    EXPECT_EQ(with_tf_copy.GetTypeface().GetUniqueId(),
              with_tf.GetTypeface().GetUniqueId());
  }
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
      0.f,  0.f,  0.f,  1.f, 0.f,
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

TEST(WrapperHpp, PaintIntrospection) {
  Paint paint;
  paint.SetColor(ColorPackRGBA(0.25f, 0.5f, 0.75f, 1.f));
  skity::raii::Color4f color = paint.GetColor4f();
  // SetColor stores 8-bit; GetColor4f unpacks, so allow one 8-bit step.
  EXPECT_NEAR(color.e[0], 0.25f, 1.f / 255.f);
  EXPECT_NEAR(color.e[1], 0.5f, 1.f / 255.f);
  EXPECT_NEAR(color.e[2], 0.75f, 1.f / 255.f);
  EXPECT_NEAR(color.e[3], 1.f, 1.f / 255.f);

  paint.SetAlphaF(0.5f);
  EXPECT_FLOAT_EQ(paint.GetAlphaF(), 0.5f);

  EXPECT_FLOAT_EQ(paint.GetFontThreshold(), 256.f);
  paint.SetFontThreshold(18.f);
  EXPECT_FLOAT_EQ(paint.GetFontThreshold(), 18.f);
  EXPECT_FALSE(paint.IsSDFForSmallText());
  paint.SetSDFForSmallText(true);
  EXPECT_TRUE(paint.IsSDFForSmallText());

  // GPU backend query compiles everywhere; the value is build-config
  // dependent, so only exercise it.
  (void)skity::raii::Context::IsGPUSupported(
      skity::raii::Context::BackendType::kVulkan);
  (void)skity::raii::Context::IsGPUSupported(
      skity::raii::Context::BackendType::kMetal);
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
  EXPECT_TRUE(Shader::MakeShader(
      image, skity::raii::SamplingOptions(skity::raii::FilterMode::kLinear,
                                          skity::raii::MipmapMode::kNone)));

  // Gradient introspection: recover the linear gradient's stops / geometry.
  Shader linear = Shader::MakeLinear(pts, stops, nullptr, 2);
  ASSERT_TRUE(linear);
  Shader::GradientInfo info{};
  std::vector<skity::raii::Color4f> out_colors;
  std::vector<float> out_offsets;
  EXPECT_EQ(linear.AsGradient(&info, &out_colors, &out_offsets),
            Shader::GradientType::kLinear);
  ASSERT_EQ(out_colors.size(), 2u);
  EXPECT_FLOAT_EQ(out_colors[0].e[1], 0.f);  // blue stop
  EXPECT_FLOAT_EQ(out_colors[1].e[0], 1.f);  // red stop
  ASSERT_EQ(out_offsets.size(), 2u);
  EXPECT_EQ(info.color_count, 2);
  EXPECT_FLOAT_EQ(info.point[0].e[0], 0.f);
  EXPECT_FLOAT_EQ(info.point[1].e[0], 100.f);
  // Both stops are fully opaque, so the gradient is opaque.
  EXPECT_TRUE(linear.IsOpaque());

  // Image introspection on the raster image above.
  EXPECT_EQ(image.GetAlphaType(), skity::raii::AlphaType::kUnpremul_AlphaType);
  EXPECT_FALSE(image.IsTextureBackend());
  EXPECT_EQ(image.GetImageType(), Image::ImageType::kPixmap);
  EXPECT_FALSE(image.IsLazy());
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
  arc.ArcTo(30.f, 30.f, 0.f, Path::ArcSize::kLarge, Path::Direction::kCW, 100.f,
            100.f);
  arc.ArcTo(0.f, 0.f, 100.f, 0.f, 25.f);
  EXPECT_FALSE(arc.IsEmpty());

  Path round;
  const Vec2 radii[4] = {Vec2{4.f, 4.f}, Vec2{8.f, 8.f}, Vec2{12.f, 12.f},
                         Vec2{16.f, 16.f}};
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

  // operator== compares contents (verbs / points / conic weights / fill
  // type): a deep clone is equal, a different path is not.
  Path copy = rect_path.Clone();
  EXPECT_TRUE(copy.IsEqual(rect_path));
  EXPECT_TRUE(rect_path.IsEqual(rect_path));
  EXPECT_FALSE(line_path.IsEqual(rect_path));
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

  Data font_data = typeface.GetData();
  EXPECT_GT(font_data.GetSize(), 0u);
  EXPECT_NE(font_data.GetData(), nullptr);
  EXPECT_EQ(typeface.GetUnitsPerEm(), 2048u);  // Roboto's head.unitsPerEm.
  skity_font_style style = typeface.GetFontStyle();
  EXPECT_EQ(style.weight, 400);
  EXPECT_EQ(style.slant, SKITY_FONT_SLANT_UPRIGHT);
  const uint32_t unique_id = typeface.GetUniqueId();
  EXPECT_NE(unique_id, 0u);

  std::string family;
  skity_font_descriptor descriptor = typeface.GetFontDescriptor(&family);
  EXPECT_EQ(family, "Roboto");
  EXPECT_EQ(descriptor.style.weight, 400);
  EXPECT_EQ(descriptor.collection_index, 0);

  // Coverage / style / table introspection (Roboto-Regular: static, upright).
  EXPECT_FALSE(typeface.IsBold());
  EXPECT_FALSE(typeface.IsItalic());
  EXPECT_TRUE(typeface.ContainGlyph('A'));
  EXPECT_FALSE(typeface.ContainGlyph(0x10FFFF));
  EXPECT_FALSE(typeface.ContainsColorTable());
  const int32_t table_count = typeface.CountTables();
  EXPECT_GT(table_count, 0);
  std::vector<uint32_t> tags((size_t)table_count);
  EXPECT_EQ(typeface.GetTableTags(tags.data(), table_count), table_count);
  const uint32_t kHeadTag = 0x68656164;  // 'head'
  EXPECT_NE(std::find(tags.begin(), tags.end(), kHeadTag), tags.end());
  const size_t head_size = typeface.GetTableSize(kHeadTag);
  EXPECT_EQ(head_size, 54u);  // TrueType head table is fixed-size.
  std::vector<uint8_t> head_bytes(head_size);
  EXPECT_EQ(typeface.GetTableData(kHeadTag, 0, head_size, head_bytes.data()),
            head_size);
  // unitsPerEm lives at offset 18 of head, big endian: 2048.
  EXPECT_EQ((head_bytes[18] << 8) | head_bytes[19], 2048);
  // Roboto declares fvar axes, so a non-instantiated face still reports its
  // default design coordinates: one coordinate per axis, in axis order,
  // each at the axis default (FreeType initializes blend coords to the
  // defaults when nothing is set).
  const std::vector<skity_variation_coordinate> position =
      typeface.GetVariationPosition();
  const std::vector<skity_variation_axis> axes = typeface.GetVariationAxes();
  EXPECT_EQ(position.size(), axes.size());
  for (size_t i = 0; i < position.size(); i++) {
    EXPECT_EQ(position[i].axis, axes[i].tag);
    EXPECT_FLOAT_EQ(position[i].value, axes[i].def);
  }

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
  // MakeWithSize copies the typeface shared_ptr: same instance, same id.
  EXPECT_EQ(bigger.GetTypeface().GetUniqueId(), unique_id);

  skity_font_metrics metrics{};
  font.GetMetrics(&metrics);
  const uint16_t ids[] = {glyphs[0], glyphs[1]};
  float widths[2] = {};
  font.GetWidths(ids, 2, widths);
  EXPECT_GT(widths[0], 0.f);
  Rect glyph_bounds[2];
  font.GetBounds(ids, 2, glyph_bounds);
  EXPECT_GT(glyph_bounds[0].right, glyph_bounds[0].left);
  EXPECT_GT(glyph_bounds[0].bottom, glyph_bounds[0].top);

  Paint paint;
  paint.SetTypeface(typeface.get());
  paint.SetTextSize(32.f);
  TextBlob blob = TextBlob::Make("AB", paint);
  ASSERT_TRUE(blob);
  EXPECT_GT(blob.GetBounds().Width(), 0.f);

  TextBlob glyphs_blob =
      TextBlob::MakeFromGlyphs(font, ids, nullptr, nullptr, 2);
  ASSERT_TRUE(glyphs_blob);
  const float pos_x[] = {0.f, 24.f};
  const float pos_y[] = {0.f, 0.f};
  Rect run_bounds =
      TextBlob::ComputeRunBounds(2, ids, pos_x, pos_y, font, paint);
  EXPECT_GT(run_bounds.Width(), 0.f);
}

TEST(WrapperHpp, TypefaceVariationAndGlyphLoading) {
  // Variable font: axes are readable and MakeVariation instantiates a face.
  Typeface flex = Typeface::MakeFromFile(
      SKITY_FONT_DIR "fonts/resources/RobotoFlex-Regular.ttf");
  ASSERT_TRUE(flex);
  auto axes = flex.GetVariationAxes();
  ASSERT_FALSE(axes.empty());
  const uint32_t kWght = 0x77676874;  // 'wght'
  bool has_wght = false;
  for (const auto& axis : axes) {
    has_wght = has_wght || axis.tag == kWght;
  }
  EXPECT_TRUE(has_wght);
  const skity_variation_coordinate wght700[] = {{kWght, 700.f}};
  Typeface bold = flex.MakeVariation(wght700, 1);
  EXPECT_TRUE(bold);
  if (bold) {
    EXPECT_TRUE(bold.IsBold());
  }

  // Glyph loading family on the static face.
  Typeface typeface = Typeface::MakeFromFile(
      SKITY_FONT_DIR "fonts/resources/Roboto-Regular.ttf");
  ASSERT_TRUE(typeface);
  uint16_t glyph = typeface.UnicharToGlyph('A');
  ASSERT_NE(glyph, 0);
  Font font(typeface.get(), 32.f);

  auto metrics = font.LoadGlyphMetrics(&glyph, 1);
  ASSERT_EQ(metrics.size(), 1u);
  ASSERT_TRUE(metrics[0]);
  EXPECT_EQ(metrics[0].GetId(), glyph);
  EXPECT_GT(metrics[0].GetAdvanceX(), 0.f);
  EXPECT_GT(metrics[0].GetWidth(), 0.f);
  // Note: FontSize stays 0 on FreeType metrics loads (only ScaleToFontSize
  // sets it), so it is deliberately not asserted here.
  EXPECT_GT(metrics[0].GetHoriBearingY(), 0.f);
  EXPECT_LT(metrics[0].GetYMin(), 0.f);
  EXPECT_FLOAT_EQ(metrics[0].GetAdvanceY(), 0.f);

  auto paths = font.LoadGlyphPath(&glyph, 1);
  ASSERT_EQ(paths.size(), 1u);
  ASSERT_TRUE(paths[0]);
  Path outline = paths[0].GetPath();
  ASSERT_TRUE(outline);
  EXPECT_FALSE(outline.IsEmpty());

  Paint paint;  // default fill style
  auto bitmaps = font.LoadGlyphBitmap(&glyph, 1, paint, 1.f);
  ASSERT_EQ(bitmaps.size(), 1u);
  ASSERT_TRUE(bitmaps[0]);
  GlyphFormat format = GlyphFormat::kA8;
  EXPECT_TRUE(bitmaps[0].GetFormat(&format));
  EXPECT_EQ(format, GlyphFormat::kA8);
  GlyphBitmap bitmap{};
  EXPECT_TRUE(bitmaps[0].GetBitmap(&bitmap));
  EXPECT_GT(bitmap.width, 0.f);
  EXPECT_GT(bitmap.height, 0.f);
  EXPECT_NE(bitmap.buffer, nullptr);
  EXPECT_GE(bitmap.row_bytes, static_cast<size_t>(bitmap.width));
  EXPECT_EQ(bitmap.format, SKITY_BITMAP_FORMAT_GRAY8);

  auto infos = font.LoadGlyphBitmapInfo(&glyph, 1, paint, 1.f);
  ASSERT_EQ(infos.size(), 1u);
  ASSERT_TRUE(infos[0]);
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

TEST(WrapperHpp, CameraAndQuaternion) {
  Camera camera(100.f, 100.f);
  ASSERT_TRUE(camera);
  camera.SetPosition(Vec4{0.f, 0.f, -100.f, 1.f});
  camera.LookAt(Vec4{0.f, 0.f, 0.f, 1.f});
  camera.SetCameraDist(200.f);
  camera.SetRotation(Matrix{});
  Matrix view = camera.GetCamera();
  EXPECT_TRUE(std::isfinite(view.m[0]) && std::isfinite(view.m[15]));
  // The fixed camera shares the distance setting (it differs only in
  // rotation / translation animation), so just sanity-check the projection
  // diagonal.
  Matrix fixed = camera.GetFixedCamera();
  EXPECT_GT(fixed.m[0], 0.f);

  // Quaternion helpers: zero Euler angles are the identity transform.
  Matrix identity = QuaternionEulerToMatrix(0.f, 0.f, 0.f);
  EXPECT_FLOAT_EQ(identity.m[0], 1.f);
  EXPECT_FLOAT_EQ(identity.m[5], 1.f);
  EXPECT_FLOAT_EQ(identity.m[10], 1.f);
  EXPECT_FLOAT_EQ(identity.m[15], 1.f);
  EXPECT_FLOAT_EQ(identity.m[1], 0.f);

  // A quarter turn around +Z maps the X axis onto the Y axis (column-major:
  // first column of m is the image of the X basis vector).
  Vec3 z_axis{0.f, 0.f, 1.f};
  Matrix quarter = QuaternionAxisAngleToMatrix(z_axis, 3.14159265f / 2.f);
  EXPECT_NEAR(quarter.m[0], 0.f, 1e-5f);
  EXPECT_NEAR(quarter.m[1], 1.f, 1e-5f);
  EXPECT_NEAR(quarter.m[4], -1.f, 1e-5f);
  EXPECT_NEAR(quarter.m[5], 0.f, 1e-5f);
}

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
    const auto* px =
        static_cast<const uint32_t*>(bitmap.GetPixmap().GetPixels());
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
  const Vec2 radii[4] = {Vec2{1.f, 1.f}, Vec2{1.f, 1.f}, Vec2{1.f, 1.f},
                         Vec2{1.f, 1.f}};
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

  // Global clip bounds: an unclipped software canvas reports an (effectively)
  // infinite device rect; clip to the device and re-check.
  canvas.ClipRect(Rect::MakeWH(8.f, 8.f));
  Rect device_clip = canvas.GetGlobalClipBounds();
  EXPECT_FLOAT_EQ(device_clip.Width(), 8.f);
  EXPECT_FLOAT_EQ(device_clip.Height(), 8.f);

  // Per-corner-radii clip + drrect draw still render sanely after the
  // pixel assertions above.
  const Vec2 corner_radii[4] = {{1.f, 1.f}, {2.f, 2.f}, {1.f, 2.f}, {2.f, 1.f}};
  canvas.ClipRRect(Rect::MakeWH(8.f, 8.f), corner_radii);
  canvas.DrawDRRect(Rect::MakeWH(8.f, 8.f), corner_radii,
                    Rect::MakeWH(4.f, 4.f), corner_radii, paint);
}
