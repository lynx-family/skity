// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/ports/darwin/scaler_context_darwin.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <skity/geometry/stroke.hpp>
#include <utility>

#include "src/text/ports/darwin/typeface_darwin.hpp"

namespace {

uint16_t ReadBigEndianUInt16(const uint8_t *data) {
  return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) |
                               static_cast<uint16_t>(data[1]));
}

void ApplyOS2StrikeoutMetrics(skity::Typeface *typeface, CTFontRef ct_font,
                              skity::FontMetrics *metrics) {
  if (typeface == nullptr || ct_font == nullptr || metrics == nullptr) {
    return;
  }

  static constexpr skity::FontTableTag kOS2TableTag =
      skity::SetFourByteTag('O', 'S', '/', '2');
  static constexpr size_t kStrikeoutSizeOffset = 26;
  static constexpr size_t kStrikeoutPositionOffset = 28;
  static constexpr size_t kStrikeoutFieldSize = 2;
  static constexpr size_t kRequiredOS2TableSize =
      kStrikeoutPositionOffset + kStrikeoutFieldSize;

  std::array<uint8_t, kRequiredOS2TableSize> os2_table{};
  size_t copied = typeface->GetTableData(kOS2TableTag, 0, os2_table.size(),
                                         os2_table.data());
  if (copied < os2_table.size()) {
    return;
  }

  uint32_t upem = CTFontGetUnitsPerEm(ct_font);
  if (upem == 0) {
    return;
  }

  CGFloat font_size = CTFontGetSize(ct_font);
  uint32_t max_sane_height = upem * 2;

  uint16_t strikeout_size =
      ReadBigEndianUInt16(os2_table.data() + kStrikeoutSizeOffset);
  if (strikeout_size != 0 && strikeout_size < max_sane_height) {
    metrics->strikeout_thickness_ = strikeout_size * font_size / upem;
  }

  uint16_t strikeout_position =
      ReadBigEndianUInt16(os2_table.data() + kStrikeoutPositionOffset);
  if (strikeout_position != 0 && strikeout_position < max_sane_height) {
    metrics->strikeout_position_ = -strikeout_position * font_size / upem;
  }
}

float compute_fake_bold_scale(float text_size) {
  static const std::array<float, 2> keys = {9.0f, 36.0f};
  // Local-space stroke-width ratios. text_scale_ is applied later when the
  // stroke is converted to device space.
  static const std::array<float, 2> values = {1.0f / 24.0f, 1.0f / 32.0f};

  if (text_size <= keys.front()) {
    return values.front();
  }
  if (text_size >= keys.back()) {
    return values.back();
  }

  auto it = std::lower_bound(keys.begin(), keys.end(), text_size);
  std::size_t right = std::distance(keys.begin(), it);

  std::size_t left = right - 1;

  float left_key = keys[left];
  float right_key = keys[right];

  float t = (text_size - left_key) / (right_key - left_key);

  return values[left] + t * (values[right] - values[left]);
}

skity::StrokeDesc fake_bold_if_needed(const skity::StrokeDesc &stroke_desc,
                                      const skity::ScalerContextDesc &desc,
                                      bool is_color) {
  if (desc.fake_bold && !is_color) {
    skity::StrokeDesc working_stroke_desc = stroke_desc;
    if (stroke_desc.is_stroke) {
      working_stroke_desc.is_stroke = true;
      working_stroke_desc.stroke_width =
          stroke_desc.stroke_width +
          desc.text_size * compute_fake_bold_scale(desc.text_size);
      working_stroke_desc.cap = stroke_desc.cap;
      working_stroke_desc.join = stroke_desc.join;
      working_stroke_desc.miter_limit = stroke_desc.miter_limit;
    } else {
      working_stroke_desc.is_stroke = true;
      working_stroke_desc.stroke_width =
          desc.text_size * compute_fake_bold_scale(desc.text_size);
      working_stroke_desc.cap = skity::Paint::Cap::kDefault_Cap;
      working_stroke_desc.join = skity::Paint::Join::kDefault_Join;
      working_stroke_desc.miter_limit = skity::Paint::kDefaultMiterLimit;
    }
    return working_stroke_desc;
  }

  return stroke_desc;
}

}  // namespace

namespace skity {

OffScreenContext::OffScreenContext(Color foreground_color) {
  rgb_color_space_.reset(CGColorSpaceCreateDeviceRGB());
  gray_color_space_.reset(CGColorSpaceCreateDeviceGray());
  foreground_color_.reset(
      ColorToCGColor(rgb_color_space_.get(), foreground_color));
}

OffScreenContext::~OffScreenContext() {
  cg_context_.reset();
  pixel_data_.reset();
}

uint32_t OffScreenContext::RoundSize(uint32_t dimension) {
  uint32_t rounded = 1;
  while (rounded < dimension &&
         rounded <= std::numeric_limits<uint32_t>::max() / 2) {
    rounded *= 2;
  }
  return rounded < dimension ? dimension : rounded;
}

void OffScreenContext::ClearActiveRect(const Target &target,
                                       size_t bytes_per_pixel) {
  const size_t active_row_bytes =
      static_cast<size_t>(target.active_width) * bytes_per_pixel;

  // Clear the backing storage directly. CGContextClearRect would be affected
  // by the cached CTM and would also put clearing back on the Core Graphics
  // path. A full-width active rect is contiguous even when it is bottom
  // aligned, so clear it with a single memset.
  if (active_row_bytes == target.row_bytes) {
    std::memset(target.pixels, 0, active_row_bytes * target.active_height);
    return;
  }

  for (uint32_t y = 0; y < target.active_height; ++y) {
    std::memset(target.pixels + static_cast<size_t>(y) * target.row_bytes, 0,
                active_row_bytes);
  }
}

OffScreenContext::Target OffScreenContext::PrepareContext(uint32_t width,
                                                          uint32_t height,
                                                          bool need_color) {
  if (width == 0 || height == 0) {
    return {};
  }

  const bool context_fits = cg_context_ && need_color_ == need_color &&
                            context_width_ >= width &&
                            context_height_ >= height;
  bool context_was_created = false;

  if (!context_fits) {
    const uint32_t context_width = std::max(context_width_, RoundSize(width));
    const uint32_t context_height =
        std::max(context_height_, RoundSize(height));
    const size_t bytes_per_pixel = need_color ? 4 : 1;
    if (context_width > std::numeric_limits<size_t>::max() / bytes_per_pixel) {
      return {};
    }

    const size_t row_bytes =
        static_cast<size_t>(context_width) * bytes_per_pixel;
    if (context_height > std::numeric_limits<size_t>::max() / row_bytes) {
      return {};
    }

    const size_t byte_size = static_cast<size_t>(context_height) * row_bytes;
    std::shared_ptr<Data> pixel_data = pixel_data_;
    if (!pixel_data || pixel_data->Size() < byte_size) {
      void *pixels = std::malloc(byte_size);
      if (!pixels) {
        return {};
      }
      pixel_data = Data::MakeFromMalloc(pixels, byte_size);
    }

    const CGBitmapInfo bitmap_info =
        need_color ? kCGImageByteOrder32Little | kCGImageAlphaPremultipliedFirst
                   : kCGImageAlphaOnly;
    UniqueCFRef<CGContextRef> cg_context(
        CGBitmapContextCreate(const_cast<void *>(pixel_data->RawData()),
                              context_width, context_height, 8, row_bytes,
                              GetCGColorSpace(need_color), bitmap_info));
    if (!cg_context) {
      return {};
    }

    // Release the old CGContext before replacing the storage it borrows.
    cg_context_ = std::move(cg_context);
    pixel_data_ = std::move(pixel_data);
    context_width_ = context_width;
    context_height_ = context_height;
    context_row_bytes_ = row_bytes;
    need_color_ = need_color;
    draw_state_ = {};
    context_was_created = true;
  }

  auto *storage =
      reinterpret_cast<uint8_t *>(const_cast<void *>(pixel_data_->RawData()));
  auto *pixels = storage + static_cast<size_t>(context_height_ - height) *
                               context_row_bytes_;

  Target target = {cg_context_.get(),  storage,         pixels,
                   context_row_bytes_, width,           height,
                   context_width_,     context_height_, context_was_created};
  ClearActiveRect(target, need_color ? 4 : 1);
  return target;
}

bool OffScreenContext::SetTextDrawingMode(CGTextDrawingMode mode) {
  if (!cg_context_ || (draw_state_.text_drawing_mode_valid &&
                       draw_state_.text_drawing_mode == mode)) {
    return false;
  }

  CGContextSetTextDrawingMode(cg_context_.get(), mode);
  draw_state_.text_drawing_mode = mode;
  draw_state_.text_drawing_mode_valid = true;
  return true;
}

bool OffScreenContext::SetLineWidth(CGFloat width) {
  if (!cg_context_ ||
      (draw_state_.line_width_valid && draw_state_.line_width == width)) {
    return false;
  }

  CGContextSetLineWidth(cg_context_.get(), width);
  draw_state_.line_width = width;
  draw_state_.line_width_valid = true;
  return true;
}

bool OffScreenContext::SetLineCap(CGLineCap cap) {
  if (!cg_context_ ||
      (draw_state_.line_cap_valid && draw_state_.line_cap == cap)) {
    return false;
  }

  CGContextSetLineCap(cg_context_.get(), cap);
  draw_state_.line_cap = cap;
  draw_state_.line_cap_valid = true;
  return true;
}

bool OffScreenContext::SetLineJoin(CGLineJoin join) {
  if (!cg_context_ ||
      (draw_state_.line_join_valid && draw_state_.line_join == join)) {
    return false;
  }

  CGContextSetLineJoin(cg_context_.get(), join);
  draw_state_.line_join = join;
  draw_state_.line_join_valid = true;
  return true;
}

bool OffScreenContext::SetMiterLimit(CGFloat limit) {
  if (!cg_context_ ||
      (draw_state_.miter_limit_valid && draw_state_.miter_limit == limit)) {
    return false;
  }

  CGContextSetMiterLimit(cg_context_.get(), limit);
  draw_state_.miter_limit = limit;
  draw_state_.miter_limit_valid = true;
  return true;
}

CGColorSpaceRef OffScreenContext::GetCGColorSpace(bool need_color) const {
  return need_color ? rgb_color_space_.get() : gray_color_space_.get();
}

CGColorRef OffScreenContext::GetCGColor() const {
  return foreground_color_.get();
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

  CFStringRef optical_size_attribute = CFSTR("NSCTFontOpticalSizeAttribute");
  UniqueCFRef<CFTypeRef> optical_size(
      CTFontCopyAttribute(base, optical_size_attribute));
  double optical_size_value = 0.0;
  if (!optical_size || CFGetTypeID(optical_size.get()) != CFNumberGetTypeID() ||
      !CFNumberGetValue(static_cast<CFNumberRef>(optical_size.get()),
                        kCFNumberDoubleType, &optical_size_value) ||
      optical_size_value <= 0.0) {
    optical_size_value = CTFontGetSize(base);
  }
  UniqueCFRef<CFNumberRef> optical_size_number(CFNumberCreate(
      kCFAllocatorDefault, kCFNumberDoubleType, &optical_size_value));
  CFDictionarySetValue(attr.get(), optical_size_attribute,
                       optical_size_number.get());

  int zero_tracking = 0;
  UniqueCFRef<CFNumberRef> tracking_number(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &zero_tracking));
  CFDictionarySetValue(attr.get(), CFSTR("NSCTFontUnscaledTrackingAttribute"),
                       tracking_number.get());

  UniqueCFRef<CTFontDescriptorRef> desc(
      CTFontDescriptorCreateWithAttributes(attr.get()));

  return UniqueCFRef<CTFontRef>(
      CTFontCreateCopyWithAttributes(base, text_size, nullptr, desc.get()));
}

ScalerContextDarwin::ScalerContextDarwin(
    std::shared_ptr<TypefaceDarwin> typeface, const ScalerContextDesc *desc)
    : ScalerContext(typeface, desc), os_context_(desc->foreground_color) {
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

  bool is_color = GetTypeface()->ContainsColorTable();
  glyph->format_ = is_color ? GlyphFormat::BGRA32 : GlyphFormat::A8;
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

void ScalerContextDarwin::InitializeCGContext(CGContextRef context,
                                              bool is_color) {
  CGContextScaleCTM(context, context_scale_, context_scale_);
  CGContextSetTextMatrix(context, transform_);

  /**
   When Core Graphics draws non-emoji glyphs into a bitmap context, it will
   round up the vertical glyph position (assuming an upper-left origin) such
   that the baseline Y-coordinate falls on a pixel boundary, except if the
   text is rotated or the context has been configured to allow vertical
   subpixel positioning by explicitly setting both
   setShouldSubpixelPositionFonts(true) and
   setShouldSubpixelQuantizeFonts(false)
   **/
  CGContextSetAllowsFontSubpixelQuantization(context, false);

  if (is_color) {
    CGContextSetFillColorWithColor(context, os_context_.GetCGColor());
  } else {
    CGContextSetGrayFillColor(context, 0.0f, 1.0f);
  }
}

void ScalerContextDarwin::DrawGlyphWithState(CGContextRef context,
                                             CGGlyph glyph, CGPoint point,
                                             const StrokeDesc *stroke_desc) {
  if (stroke_desc) {
    os_context_.SetTextDrawingMode(kCGTextStroke);
    os_context_.SetLineWidth(stroke_desc->stroke_width * text_scale_);
    os_context_.SetLineCap(ToCGCap(stroke_desc->cap));
    os_context_.SetLineJoin(ToCGJoin(stroke_desc->join));
    os_context_.SetMiterLimit(stroke_desc->miter_limit);
  } else {
    os_context_.SetTextDrawingMode(kCGTextFill);
  }

  CTFontDrawGlyphs(ct_font_.get(), &glyph, &point, 1, context);
}

void ScalerContextDarwin::GenerateImage(GlyphData *glyph,
                                        const StrokeDesc &stroke_desc) {
  // The returned pixels borrow OffScreenContext storage. Clear the previously
  // published view before generating the next single-glyph bitmap.
  glyph->image_.buffer = nullptr;
  glyph->image_.row_bytes = 0;
  glyph->image_.need_free = false;

  GenerateImageInfo(glyph, stroke_desc);
  if (glyph->image_.width == 0 || glyph->image_.height == 0.0) {
    return;
  }
  CGGlyph cg_glyph = glyph->Id();

  const uint32_t width = static_cast<uint32_t>(glyph->image_.width);
  const uint32_t height = static_cast<uint32_t>(glyph->image_.height);

  const bool is_color = glyph->image_.format == BitmapFormat::kBGRA8;

  OffScreenContext::Target target =
      os_context_.PrepareContext(width, height, is_color);
  if (!target) {
    return;
  }

  CGContextRef cg_context = target.context;
  if (target.context_was_created) {
    InitializeCGContext(cg_context, is_color);
  }

  CGPoint point = CGPointMake(glyph->image_.origin_x_for_raster,
                              glyph->image_.origin_y_for_raster);

  if (desc_.fake_bold && !is_color) {
    StrokeDesc working_stroke_desc =
        fake_bold_if_needed(stroke_desc, desc_, is_color);
    DrawGlyphWithState(cg_context, cg_glyph, point, &working_stroke_desc);
    DrawGlyphWithState(cg_context, cg_glyph, point, nullptr);
  } else {
    DrawGlyphWithState(cg_context, cg_glyph, point,
                       stroke_desc.is_stroke ? &stroke_desc : nullptr);
  }

  glyph->image_.buffer = target.pixels;
  glyph->image_.row_bytes = target.row_bytes;
}

void ScalerContextDarwin::GenerateImageInfo(GlyphData *glyph,
                                            const StrokeDesc &stroke_desc) {
  CGGlyph cg_glyph = glyph->Id();

  if (!cg_glyph) {
    return;
  }

  bool is_color = GetTypeface()->ContainsColorTable();

  StrokeDesc working_stroke_desc =
      fake_bold_if_needed(stroke_desc, desc_, is_color);

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
  uint32_t width = std::ceil(cg_bounds.size.width * context_scale_) + 2;
  uint32_t height = std::ceil(cg_bounds.size.height * context_scale_) + 2;

  if (working_stroke_desc.is_stroke) {
    if (glyph->GetPath().IsEmpty()) {
      GeneratePath(glyph);
    }
    Paint paint;
    paint.SetStyle(Paint::kStroke_Style);
    paint.SetStrokeWidth(working_stroke_desc.stroke_width * text_scale_);
    paint.SetStrokeCap(working_stroke_desc.cap);
    paint.SetStrokeJoin(working_stroke_desc.join);
    paint.SetStrokeMiter(working_stroke_desc.miter_limit);
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

  // Core Graphics snaps the glyph baseline to the device pixel grid while
  // rasterizing into the bitmap context. Keep the atlas origin in the same
  // coordinate system; otherwise the fractional glyph bound is applied again
  // when DirectGlyphRun places the bitmap, producing glyph-dependent vertical
  // offsets. An X-axis skew (transform_.c), as used by synthetic italic text,
  // does not affect the device-space Y coordinate and still needs the same
  // alignment. Only transforms that mix X into Y (transform_.b) are excluded.
  if (transform_.b == 0) {
    point.y = std::floor(point.y * context_scale_) / context_scale_;
  }

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
  metrics->underline_position_ = -CTFontGetUnderlinePosition(ct_font_.get());
  metrics->strikeout_thickness_ = 0;
  metrics->strikeout_position_ = 0;
  ApplyOS2StrikeoutMetrics(GetTypeface().get(), ct_font_.get(), metrics);
}

}  // namespace skity
