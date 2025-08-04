// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/ports/darwin/scaler_context_darwin.hpp"

#include <skity/geometry/stroke.hpp>

#include "src/text/ports/darwin/typeface_darwin.hpp"

namespace skity {

void OffScreenContext::ResizeContext(uint32_t width, uint32_t height,
                                     bool need_color) {
  uint32_t row_bytes = width;
  if (need_color) {
    row_bytes *= 4;
  }

  pixel_data_ =
      Data::MakeFromMalloc(std::malloc(height * row_bytes), height * row_bytes);

  if (need_color) {
    cg_color_space_.reset(CGColorSpaceCreateDeviceRGB());
  } else {
    cg_color_space_.reset(CGColorSpaceCreateDeviceGray());
  }
}

CGColorSpaceRef OffScreenContext::GetCGColorSpace() const {
  return cg_color_space_.get();
}

void *OffScreenContext::GetAddr() const {
  if (!pixel_data_) {
    return nullptr;
  }

  return const_cast<void *>(pixel_data_->RawData());
}

class CGPathConvertor {
 public:
  explicit CGPathConvertor(Path *path) : path_(path) {}
  ~CGPathConvertor() = default;

  bool CurrentIsNot(const CGPoint pt) {
    return current_.x != pt.x || current_.y != pt.y;
  }

  void GoingTo(const CGPoint pt) {
    if (!started_) {
      started_ = true;
      path_->MoveTo(current_.x, -current_.y);
    }
    current_ = pt;
  }

  static void ApplyElement(void *ctx, const CGPathElement *element) {
    auto self = reinterpret_cast<CGPathConvertor *>(ctx);
    CGPoint *points = element->points;

    switch (element->type) {
      case kCGPathElementMoveToPoint:
        self->started_ = false;
        self->current_ = points[0];
        break;
      case kCGPathElementAddLineToPoint:
        if (self->CurrentIsNot(points[0])) {
          self->GoingTo(points[0]);
          self->path_->LineTo(points[0].x, -points[0].y);
        }
        break;
      case kCGPathElementAddQuadCurveToPoint:
        if (self->CurrentIsNot(points[0]) || self->CurrentIsNot(points[1])) {
          self->GoingTo(points[1]);
          self->path_->QuadTo(points[0].x, -points[0].y, points[1].x,
                              -points[1].y);
        }
        break;
      case kCGPathElementAddCurveToPoint:
        if (self->CurrentIsNot(points[0]) || self->CurrentIsNot(points[1]) ||
            self->CurrentIsNot(points[2])) {
          self->GoingTo(points[2]);
          self->path_->CubicTo(points[0].x, -points[0].y, points[1].x,
                               -points[1].y, points[2].x, -points[2].y);
        }
        break;

      case kCGPathElementCloseSubpath:
        if (self->started_) {
          self->path_->Close();
        }
        break;

      default:
        // Unknown path element!
        break;
    }
  }

 private:
  Path *path_;
  CGPoint current_;
  bool started_ = false;
};

UniqueCTFontRef ct_font_copy_with_size(CTFontRef base, CGFloat text_size) {
  UniqueCFRef<CFMutableDictionaryRef> attr(CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));

  UniqueCFRef<CTFontDescriptorRef> desc(
      CTFontDescriptorCreateWithAttributes(attr.get()));

  return UniqueCFRef<CTFontRef>(
      CTFontCreateCopyWithAttributes(base, text_size, nullptr, desc.get()));
}

ScalerContextDarwin::ScalerContextDarwin(
    std::shared_ptr<TypefaceDarwin> typeface, const ScalerContextDesc *desc)
    : ScalerContext(typeface, desc), os_context_() {
  float scaled_size;
  Matrix22 transform;
  desc->DecomposeMatrix(PortScaleType::kVertical, &scaled_size, &scaled_size,
                        &transform);
  context_scale_ = desc->context_scale;
  text_scale_ = scaled_size / desc->text_size;
  transform_ =
      CGAffineTransformMake(transform.GetScaleX(), -transform.GetSkewY(),
                            -transform.GetSkewX(), transform.GetScaleY(), 0, 0);
  invert_transform_ = CGAffineTransformInvert(transform_);
  ct_font_ = ct_font_copy_with_size(typeface->GetCTFont(), scaled_size);
}

ScalerContextDarwin::~ScalerContextDarwin() = default;

void ScalerContextDarwin::GenerateMetrics(GlyphData *glyph) {
  CGGlyph cg_glyph = glyph->Id();

  glyph->ZeroMetrics();

  CGSize cg_advance;

  CTFontGetAdvancesForGlyphs(ct_font_.get(), kCTFontOrientationDefault,
                             &cg_glyph, &cg_advance, 1);
  cg_advance = CGSizeApplyAffineTransform(cg_advance, transform_);

  glyph->advance_x_ = cg_advance.width;
  glyph->advance_y_ = cg_advance.height;

  CGRect cg_bounds;
  CTFontGetBoundingRectsForGlyphs(ct_font_.get(), kCTFontOrientationHorizontal,
                                  &cg_glyph, &cg_bounds, 1);
  cg_bounds = CGRectApplyAffineTransform(cg_bounds, transform_);

  CGSize cg_offset;
  CTFontGetVerticalTranslationsForGlyphs(ct_font_.get(), &cg_glyph, &cg_offset,
                                         1);
  cg_offset = CGSizeApplyAffineTransform(cg_offset, transform_);

  glyph->width_ = cg_bounds.size.width;
  glyph->height_ = cg_bounds.size.height;
  glyph->y_max_ = -cg_offset.height;
  glyph->y_min_ = cg_bounds.origin.y;
  glyph->hori_bearing_x_ = cg_bounds.origin.x;
  glyph->hori_bearing_y_ = cg_bounds.origin.y + cg_bounds.size.height;
}

static CGLineCap ToCGCap(Paint::Cap cap) {
  switch (cap) {
    case Paint::Cap::kButt_Cap:
      return CGLineCap::kCGLineCapButt;
    case Paint::Cap::kRound_Cap:
      return CGLineCap::kCGLineCapRound;
    case Paint::Cap::kSquare_Cap:
      return CGLineCap::kCGLineCapSquare;
  }
}

static CGLineJoin ToCGJoin(Paint::Join join) {
  switch (join) {
    case Paint::Join::kBevel_Join:
      return CGLineJoin::kCGLineJoinBevel;
    case Paint::Join::kRound_Join:
      return CGLineJoin::kCGLineJoinRound;
    case Paint::Join::kMiter_Join:
      return CGLineJoin::kCGLineJoinMiter;
  }
}

void ScalerContextDarwin::GenerateImage(GlyphData *glyph,
                                        const StrokeDesc &stroke_desc) {
  GenerateImageInfo(glyph, stroke_desc);
  if (glyph->image_.width == 0 || glyph->image_.height == 0.0) {
    return;
  }
  CGGlyph cg_glyph = glyph->Id();

  auto width = glyph->image_.width;
  auto height = glyph->image_.height;

  bool is_color = glyph->image_.format == BitmapFormat::kBGRA8;

  os_context_.ResizeContext(width, height, is_color);

  // kCGImageByteOrder32Little + kCGImageAlphaPremultipliedFirst(argb) ===>
  // bgra;
  CGBitmapInfo bitmap_info =
      is_color ? kCGImageByteOrder32Little | kCGImageAlphaPremultipliedFirst
               : kCGImageAlphaOnly;

  CGColorSpaceRef color_space = is_color ? CGColorSpaceCreateDeviceRGB() : NULL;

  static size_t bits_per_component = 8;

  // The number of bytes per pixel is equal to `(bitsPerComponent * number
  // of components + 7)/8'
  size_t bytes_per_pixel =
      (bits_per_component * (is_color ? 4 : 1) + 7) / bits_per_component;

  //  Each row of the bitmap consists of `bytesPerRow' bytes, which must be
  //  at least
  // `width * bytes per pixel' bytes; in addition, `bytesPerRow' must be an
  //  integer multiple of the number of bytes per pixel.
  size_t bytes_per_row = width * bytes_per_pixel;

  UniqueCFRef<CGContextRef> cg_context(CGBitmapContextCreate(
      os_context_.GetAddr(), width, height, bits_per_component, bytes_per_row,
      color_space, bitmap_info));
  // clear the bitmap before set transform
  CGContextClearRect(cg_context.get(), CGRectMake(0, 0, width, height));

  CGContextScaleCTM(cg_context.get(), context_scale_, context_scale_);
  CGContextSetTextMatrix(cg_context.get(), transform_);

  /**
   When Core Graphics draws non-emoji glyphs into a bitmap context, it will
   round up the vertical glyph position (assuming an upper-left origin) such
   that the baseline Y-coordinate falls on a pixel boundary, except if the
   text is rotated or the context has been configured to allow vertical
   subpixel positioning by explicitly setting both
   setShouldSubpixelPositionFonts(true) and
   setShouldSubpixelQuantizeFonts(false)
   **/
  // In my test, it's no need to call setShouldSubpixelPositionFonts(true)
  CGContextSetAllowsFontSubpixelQuantization(cg_context.get(), false);

  CGPoint point = CGPointMake(glyph->image_.origin_x_for_raster,
                              glyph->image_.origin_y_for_raster);

  if (stroke_desc.is_stroke) {
    CGContextSetTextDrawingMode(cg_context.get(), kCGTextStroke);
    CGContextSetLineWidth(cg_context.get(),
                          stroke_desc.stroke_width * text_scale_);
    CGContextSetLineCap(cg_context.get(), ToCGCap(stroke_desc.cap));
    CGContextSetLineJoin(cg_context.get(), ToCGJoin(stroke_desc.join));
    CGContextSetMiterLimit(cg_context.get(), stroke_desc.miter_limit);
    CTFontDrawGlyphs(ct_font_.get(), &cg_glyph, &point, 1, cg_context.get());
  } else {
    CGContextSetTextDrawingMode(cg_context.get(), kCGTextFill);
    CTFontDrawGlyphs(ct_font_.get(), &cg_glyph, &point, 1, cg_context.get());
  }

  glyph->image_.buffer = reinterpret_cast<uint8_t *>(os_context_.GetAddr());
}

void ScalerContextDarwin::GenerateImageInfo(GlyphData *glyph,
                                            const StrokeDesc &stroke_desc) {
  CGGlyph cg_glyph = glyph->Id();
  uint32_t width = glyph->GetWidth();
  uint32_t height = glyph->GetHeight();

  if (!cg_glyph || !width || !height) {
    return;
  }

  bool is_color = GetTypeface()->ContainsColorTable();

  CGRect cg_bounds;

  CTFontGetBoundingRectsForGlyphs(ct_font_.get(), kCTFontOrientationHorizontal,
                                  &cg_glyph, &cg_bounds, 1);
  cg_bounds = CGRectApplyAffineTransform(cg_bounds, transform_);
  CGPoint point = CGPointMake(-cg_bounds.origin.x, -cg_bounds.origin.y);
  Rect rect = Rect::MakeXYWH(cg_bounds.origin.x,
                             -cg_bounds.origin.y - cg_bounds.size.height,
                             cg_bounds.size.width, cg_bounds.size.height);

  // extends one pixel for the bitmap bounds
  // it is used for the AA pixel and it is important
  width = std::ceil(cg_bounds.size.width * context_scale_) + 2;
  height = std::ceil(cg_bounds.size.height * context_scale_) + 2;

  if (stroke_desc.is_stroke) {
    if (glyph->GetPath().IsEmpty()) {
      GeneratePath(glyph);
    }
    Paint paint;
    paint.SetStyle(Paint::kStroke_Style);
    paint.SetStrokeWidth(stroke_desc.stroke_width * text_scale_);
    paint.SetStrokeCap(stroke_desc.cap);
    paint.SetStrokeJoin(stroke_desc.join);
    paint.SetStrokeMiter(stroke_desc.miter_limit);
    Path quad_path;
    Path fill_path;
    Stroke stroke(paint);
    stroke.QuadPath(glyph->GetPath(), &quad_path);
    stroke.StrokePath(quad_path, &fill_path);

    Rect stroke_bound = fill_path.GetBounds();
    point.x = -stroke_bound.Left();
    point.y = stroke_bound.Bottom();
    width = std::ceil(stroke_bound.Width() * context_scale_) + 2;
    height = std::ceil(stroke_bound.Height() * context_scale_) + 2;
  }

  // since bitmap extends one pixel, the origin point needs do the same move
  point.x += 1 / context_scale_;
  point.y += 1 / context_scale_;

  CGPoint src{point.x, point.y};
  CGPoint dst = CGPointApplyAffineTransform(src, invert_transform_);
  glyph->image_.origin_x = -point.x;
  // the CoreGraphic coordinate needs to flip Y axis for our canvas rendering
  glyph->image_.origin_y = -point.y + height / context_scale_;
  glyph->image_.origin_x_for_raster = dst.x;
  glyph->image_.origin_y_for_raster = dst.y;
  glyph->image_.width = width;
  glyph->image_.height = height;
  glyph->image_.format = is_color ? BitmapFormat::kBGRA8 : BitmapFormat::kGray8;
}

bool ScalerContextDarwin::GeneratePath(GlyphData *glyph) {
  CGGlyph cg_glyph = glyph->Id();

  UniqueCFRef<CGPathRef> cg_path(
      CTFontCreatePathForGlyph(ct_font_.get(), cg_glyph, &transform_));

  if (CGPathIsEmpty(cg_path.get())) {
    return false;
  }

  glyph->path_.Reset();

  CGPathConvertor convertor(&glyph->path_);
  CGPathApply(cg_path.get(), &convertor, CGPathConvertor::ApplyElement);

  return true;
}

void ScalerContextDarwin::GenerateFontMetrics(FontMetrics *metrics) {
  CGRect ct_bound = CTFontGetBoundingBox(ct_font_.get());

  metrics->top_ = -(ct_bound.origin.y + ct_bound.size.height);
  metrics->ascent_ = -CTFontGetAscent(ct_font_.get());
  metrics->descent_ = CTFontGetDescent(ct_font_.get());
  metrics->bottom_ = -ct_bound.origin.y;
  metrics->leading_ = CTFontGetLeading(ct_font_.get());
  metrics->avg_char_width_ = ct_bound.size.width;
  metrics->x_min_ = ct_bound.origin.x;
  metrics->x_max_ = ct_bound.origin.x + ct_bound.size.width;
  metrics->max_char_width_ = metrics->x_max_ - metrics->x_min_;
  metrics->x_height_ = CTFontGetXHeight(ct_font_.get());
  metrics->cap_height_ = CTFontGetCapHeight(ct_font_.get());
  metrics->underline_thickness_ = CTFontGetUnderlineThickness(ct_font_.get());
  metrics->underline_position_ = CTFontGetUnderlinePosition(ct_font_.get());
}

}  // namespace skity
