// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/sw/sw_canvas.hpp"

#include <algorithm>
#include <cstring>
#include <skity/effect/mask_filter.hpp>
#include <skity/effect/path_effect.hpp>
#include <skity/geometry/stroke.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/text/font.hpp>
#include <skity/text/text_blob.hpp>
#include <skity/text/text_run.hpp>

#include "src/effect/image_filter_base.hpp"
#include "src/effect/mask_filter_priv.hpp"
#include "src/effect/pixmap_shader.hpp"
#include "src/render/sw/sw_raster.hpp"
#include "src/render/sw/sw_span_brush.hpp"
#include "src/render/sw/sw_stack_blur.hpp"
#include "src/tracing.hpp"

namespace skity {
namespace {
constexpr ColorType ToColorType(BitmapFormat bitmap_format) {
  switch (bitmap_format) {
    case BitmapFormat::kBGRA8:
      return ColorType::kBGRA;
    case BitmapFormat::kRGBA8:
      return ColorType::kRGBA;
    case BitmapFormat::kGray8:
      return ColorType::kA8;
    case BitmapFormat::kUnknown:
      return ColorType::kUnknown;
  }
}
}  // namespace

static std::vector<Span> find_span_y(std::vector<Span> const& spans,
                                     int32_t y) {
  std::vector<Span> ret;

  for (Span const& span : spans) {
    if (span.y == y) {
      ret.emplace_back(span);
    }
  }

  return ret;
}

static std::vector<Span> spans_subtraction(std::vector<Span> const& subtrahend,
                                           std::vector<Span> const& minuend) {
  std::vector<Span> ret;

  for (Span const& span : subtrahend) {
    auto ms = find_span_y(minuend, span.y);

    // no spans in this line means minus zero
    if (ms.empty()) {
      ret.emplace_back(span);
      continue;
    }

    std::sort(ms.begin(), ms.end(),
              [](Span const& a, Span const& b) { return a.x < b.x; });

    int32_t curr_x = span.x;
    int32_t curr_len = span.len;

    for (Span const& m : ms) {
      if (m.x + m.len < curr_x || m.x > curr_x + curr_len) {
        continue;
      }

      if (m.x < curr_x) {
        if (m.x + m.len > curr_x + curr_len) {
          // complete subtracted
          curr_len = 0;
          break;
        }

        int32_t last = curr_x + curr_len;

        int32_t len = m.x + m.len - curr_x;

        if (len == 0) {
          continue;
        }

        ret.emplace_back(Span{curr_x, span.y, len, span.cover});

        curr_x += len;

        curr_len = last - curr_x;

      } else {  // m.x > curr_x && m.x < curr_x + curr_len
        if (m.x + m.len < curr_x + curr_len) {
          int32_t last = curr_x + curr_len;
          int32_t x = curr_x;
          int32_t len = m.x - curr_x;

          ret.emplace_back(Span{x, span.y, len, span.cover});

          curr_x = m.x + m.len;
          curr_len = last - curr_x;
        } else {
          int32_t x = curr_x;

          int32_t len = m.x - curr_x;

          ret.emplace_back(Span{x, span.y, len, span.cover});

          curr_len = 0;
        }
      }

      if (curr_len <= 0) {
        break;
      }
    }

    if (curr_len > 0) {
      ret.emplace_back(Span{curr_x, span.y, curr_len, span.cover});
    }
  }

  return ret;
}

static Rect ComputeBoundsIfStroke(Rect bounds, const Paint& paint) {
  if (paint.GetStyle() != Paint::kFill_Style) {
    float stroke_width = paint.GetStrokeWidth();
    bounds.SetLTRB(glm::floor(bounds.Left() - stroke_width),
                   glm::floor(bounds.Top() - stroke_width),
                   glm::floor(bounds.Right() + stroke_width),
                   glm::floor(bounds.Bottom() + stroke_width));
  }
  return bounds;
}

std::unique_ptr<Canvas> Canvas::MakeSoftwareCanvas(Bitmap* bitmap) {
  if (bitmap == nullptr) {
    return {};
  }

  if (bitmap->Width() == 0 || bitmap->Height() == 0) {
    return {};
  }

  return std::make_unique<SWCanvas>(bitmap);
}

std::vector<Span> SWCanvas::State::PerformClip(const std::vector<Span>& spans) {
  if (this->op == Canvas::ClipOp::kDifference) {
    return spans_subtraction(spans, clip_spans_);
  }

  std::vector<Span> ret;

  for (Span const& span : spans) {
    auto sub_spans = FindSpan(span);

    if (sub_spans.empty()) {
      continue;
    }

    ret.insert(ret.end(), sub_spans.begin(), sub_spans.end());
  }

  return ret;
}

std::vector<Span> SWCanvas::State::RecursiveClip(std::vector<Span> const& spans,
                                                 ClipOp clip_op) {
  if (this->op == clip_op) {
    if (clip_op == Canvas::ClipOp::kIntersect) {
      return PerformClip(spans);
    } else {
      return PerformMerge(spans);
    }
  } else {
    if (this->op == Canvas::ClipOp::kDifference) {
      return spans_subtraction(spans, clip_spans_);
    } else {
      return spans_subtraction(clip_spans_, spans);
    }
  }
}

std::vector<Span> SWCanvas::State::PerformMerge(
    std::vector<Span> const& spans) {
  // TODO(tangruiwen) this function is not correct need refact
  std::vector<Span> ret;
  ret.insert(ret.end(), spans.begin(), spans.end());
  ret.insert(ret.end(), clip_spans_.begin(), clip_spans_.end());

  std::sort(ret.begin(), ret.end(), [](Span const& a, Span const& b) {
    if (a.y < b.y) {
      return true;
    } else if (a.y == b.y) {
      if (a.x != b.x) {
        return a.x < b.x;
      } else {
        return a.cover > b.cover;
      }
    } else {
      return false;
    }
  });

  return ret;
}

std::vector<Span> SWCanvas::State::FindSpan(Span const& span) {
  std::vector<Span> ret;

  for (Span clip : clip_spans_) {
    if (clip.y != span.y) {
      continue;
    }

    if (clip.x < span.x) {
      if (clip.x + clip.len < span.x) {
        continue;
      }

      int32_t last = std::min(clip.x + clip.len, span.x + span.len);

      Span sub_span{};
      sub_span.x = span.x;
      sub_span.y = span.y;
      sub_span.len = last - span.x;
      sub_span.cover = std::min(clip.cover, span.cover);

      ret.emplace_back(sub_span);
    } else if (clip.x == span.x) {
      Span sub_span{};
      sub_span.x = span.x;
      sub_span.y = span.y;
      sub_span.len = std::min(span.len, clip.len);
      sub_span.cover = std::min(clip.cover, span.cover);

      ret.emplace_back(sub_span);
    } else if (clip.x > span.x) {
      if (span.x + span.len < clip.x) {
        continue;
      }

      Span sub_span{};
      sub_span.x = clip.x;
      sub_span.y = span.y;
      sub_span.len = std::min(span.x + span.len - clip.x + 1, clip.len);
      sub_span.cover = std::min(clip.cover, span.cover);

      ret.emplace_back(sub_span);
    }
  }

  return ret;
}

void SWCanvas::LayerState::Init(SWCanvas* parent_canvas, Vec2 offset) {
  this->bitmap = std::make_unique<Bitmap>(
      static_cast<uint32_t>(std::ceil(rel_bounds.Width())),
      static_cast<uint32_t>(std::ceil(rel_bounds.Height())),
      AlphaType::kPremul_AlphaType);

  this->canvas = parent_canvas->CreateSubCanvas(
      bitmap.get(), offset + Vec2{rel_bounds.Left(), rel_bounds.Top()});

  std::memset(bitmap->GetPixelAddr(), 0, bitmap->RowBytes() * bitmap->Height());
}

SWCanvas::SWCanvas(Bitmap* bitmap) : Canvas(), bitmap_(bitmap) {
  state_stack_.emplace_back(State());
}

void SWCanvas::OnDrawLine(float x0, float y0, float x1, float y1,
                          Paint const& paint) {
  SKITY_TRACE_EVENT(SWCanvas_OnDrawLine);

  Path path;
  path.MoveTo(x0, y0);
  path.LineTo(x1, y1);

  this->OnDrawPath(path, paint);
}

void SWCanvas::OnClipRect(const Rect& rect, ClipOp op) {
  SKITY_TRACE_EVENT(SWCanvas_OnClipRect);

  if (PeekLayerStack()) {
    PeekLayerStack()->canvas->ClipRect(rect, op);
    return;
  }

  if (op == ClipOp::kDifference || CurrentTransform().HasRotation()) {
    Canvas::OnClipRect(rect, op);
    return;
  }

  // We do nothing here because this clip is already handled in calculating
  // global clip bounds
}

void SWCanvas::OnClipPath(const Path& path, ClipOp op) {
  SKITY_TRACE_EVENT(SWCanvas_OnClipPath);

  if (PeekLayerStack()) {
    PeekLayerStack()->canvas->ClipPath(path, op);
    return;
  }

  SWRaster raster;
  raster.RastePath(path, CurrentTransform(), GetScanClipBounds());

  if (state_stack_.back().HasClip()) {
    auto spans = state_stack_.back().RecursiveClip(raster.CurrentSpans(), op);
    state_stack_.back().clip_spans_ = spans;
    if (state_stack_.back().op != op) {
      state_stack_.back().op = Canvas::ClipOp::kIntersect;
    }
  } else {
    state_stack_.back().clip_spans_ = raster.CurrentSpans();
    state_stack_.back().op = op;
  }
}

void SWCanvas::DoBrush(const SWRaster& raster, const Paint& paint,
                       bool stroke) {
  SKITY_TRACE_EVENT(SWCanvas_DoBrush);

  std::unique_ptr<SWSpanBrush> brush;
  if (state_stack_.back().HasClip()) {
    auto clip_spans = state_stack_.back().PerformClip(raster.CurrentSpans());
    if (clip_spans.empty()) {
      return;
    }
    brush = GenerateBrush(clip_spans, paint, stroke, raster.GetBounds());
    brush->Brush();
  } else {
    brush =
        GenerateBrush(raster.CurrentSpans(), paint, stroke, raster.GetBounds());
    brush->Brush();
  }
}

void SWCanvas::OnDrawPath(const Path& path, const Paint& paint) {
  SKITY_TRACE_EVENT(SWCanvas_OnDrawPath);

  if (PeekLayerStack()) {
    PeekLayerStack()->canvas->DrawPath(path, paint);
    return;
  }

  if (paint.GetMaskFilter() || paint.GetImageFilter()) {
    // post processing
    HandleFilter(path, paint);
    return;
  }

  bool need_fill = paint.GetStyle() != Paint::kStroke_Style;
  bool need_stroke = paint.GetStyle() != Paint::kFill_Style;

  // Fill first
  if (need_fill) {
    SWRaster raster;

    Path temp;
    if (paint.GetPathEffect() &&
        paint.GetPathEffect()->FilterPath(&temp, path, false, paint)) {
      raster.RastePath(temp, CurrentTransform(), GetScanClipBounds());
    } else {
      raster.RastePath(path, CurrentTransform(), GetScanClipBounds());
    }

    DoBrush(raster, paint, false);
  }

  if (need_stroke) {
    Stroke stroke(paint);

    Path temp;
    Path quad;
    Path outline;
    if (paint.GetPathEffect() &&
        paint.GetPathEffect()->FilterPath(&temp, path, true, paint)) {
      stroke.QuadPath(temp, &quad);
      stroke.StrokePath(quad, &outline);
    } else {
      stroke.QuadPath(path, &quad);
      stroke.StrokePath(quad, &outline);
    }

    SWRaster raster;
    raster.RastePath(outline, CurrentTransform(), GetScanClipBounds());

    DoBrush(raster, paint, true);
  }
}

void SWCanvas::OnDrawPaint(const Paint& paint) {
  SKITY_TRACE_EVENT(SWCanvas_OnDrawPaint);

  if (PeekLayerStack()) {
    PeekLayerStack()->canvas->DrawPaint(paint);
    return;
  }

  skity::Rect bounds = skity::Rect::MakeWH(Width(), Height());

  Path path;
  path.AddRect(bounds);

  SWRaster raster;
  raster.RastePath(path, Matrix{});

  std::unique_ptr<SWSpanBrush> brush;
  // no clip
  if (state_stack_.empty() || !state_stack_.back().HasClip()) {
    brush = GenerateBrush(raster.CurrentSpans(), paint, false, bounds);
    brush->Brush();
  } else {
    auto spans = state_stack_.back().PerformClip(raster.CurrentSpans());
    brush = GenerateBrush(spans, paint, false, bounds);
    brush->Brush();
  }
}

void SWCanvas::OnSaveLayer(const Rect& bounds, const Paint& paint) {
  SKITY_TRACE_EVENT(SWCanvas_OnSaveLayer);

  state_stack_.emplace_back(state_stack_.back());
  if (PeekLayerStack()) {
    PeekLayerStack()->canvas->OnSave();
  }

  CurrentState()->has_layer = true;

  Paint work_paint{paint};
  work_paint.SetStyle(Paint::kFill_Style);
  auto layer_bounds = work_paint.ComputeFastBounds(bounds);

  Matrix canvas_matrix;

  Vec2 offset{0.0f, 0.0f};
  if (PeekLayerStack()) {
    canvas_matrix = PeekLayerStack()->canvas->CurrentTransform();
    offset = PeekLayerStack()->canvas->global_offset_;
  } else {
    canvas_matrix = CurrentTransform();
  }

  Rect rel_bounds{};

  canvas_matrix.MapRect(&rel_bounds, layer_bounds);

  auto layer = GenerateLayer(rel_bounds, layer_bounds, offset);

  layer->paint = paint;
}

void SWCanvas::OnDrawBlob(const TextBlob* blob, float x, float y,
                          Paint const& paint) {
  SKITY_TRACE_EVENT(SWCanvas_OnDrawBlob);

  if (PeekLayerStack()) {
    PeekLayerStack()->canvas->OnDrawBlob(blob, x, y, paint);
    return;
  }

  for (auto const& run : blob->GetTextRun()) {
    auto typeface = run.LockTypeface();
    auto const& glyphs = run.GetGlyphInfo();
    if (!typeface || glyphs.empty()) {
      continue;
    }

    Font font(typeface, run.GetFontSize());
    std::vector<float> pos_y{};
    std::vector<float> pos_x{};
    if (run.GetPosX().empty()) {
      // this this slow, but cpu raster is not performance sensitive
      // std::vector<GlyphData> glyphs(run.GetGlyphInfo().size());
      std::vector<const GlyphData*> glyph_data(glyphs.size());
      font.LoadGlyphMetrics(glyphs.data(), glyphs.size(), glyph_data.data(),
                            paint);

      for (auto const& glyph : glyph_data) {
        pos_x.emplace_back(x + glyph->GetHoriBearingX());
        pos_y.emplace_back(y);

        x += glyph->AdvanceX();
      }
    } else {
      pos_y.resize(run.GetPosX().size(), y);
      for (auto rx : run.GetPosX()) {
        pos_x.emplace_back(rx + x);
      }
    }

    this->DrawGlyphsInternal(static_cast<int32_t>(glyphs.size()), glyphs.data(),
                             pos_x.data(), pos_y.data(), font, paint);
  }
}

void SWCanvas::OnDrawGlyphs(uint32_t count, const GlyphID* glyphs,
                            const float* position_x, const float* position_y,
                            const Font& font, const Paint& paint) {
  if (PeekLayerStack()) {
    PeekLayerStack()->canvas->OnDrawGlyphs(count, glyphs, position_x,
                                           position_y, font, paint);
  } else {
    this->DrawGlyphsInternal(count, glyphs, position_x, position_y, font,
                             paint);
  }
}

void SWCanvas::DrawGlyphsInternal(uint32_t count, const GlyphID* glyphs,
                                  const float* position_x,
                                  const float* position_y, const Font& font,
                                  const Paint& paint) {
  SKITY_TRACE_EVENT(SWCanvas_DrawGlyphsInternal);

  bool need_fill = paint.GetStyle() != Paint::kStroke_Style;
  bool need_stroke = paint.GetStyle() != Paint::kFill_Style;

  if (paint.GetMaskFilter() || paint.GetImageFilter()) {
    HandleFilter(count, glyphs, position_x, position_y, font, paint);
    return;
  }

  if (need_fill || font.GetTypeface()->ContainsColorTable()) {
    FillGlyphs(count, glyphs, position_x, position_y, font, paint);
  }

  if (need_stroke) {
    StrokeGlyphs(count, glyphs, position_x, position_y, font, paint);
  }
}

void SWCanvas::FillGlyphs(uint32_t count, const GlyphID* glyphs,
                          const float* position_x, const float* position_y,
                          const Font& font, const Paint& paint) {
  std::vector<const GlyphData*> glyphs_data(count);
  font.LoadGlyphPath(glyphs, count, glyphs_data.data());
  for (uint32_t k = 0; k < count; k++) {
    auto& path = glyphs_data[k]->GetPath();

    auto transform = Matrix::Translate(position_x[k], position_y[k]);

    if (font.GetTypeface()->ContainsColorTable()) {
      float x = position_x[k] + glyphs_data[k]->GetHoriBearingX();
      float y = position_y[k] - glyphs_data[k]->GetHoriBearingY();

      float h = glyphs_data[k]->GetHeight();
      float w = glyphs_data[k]->GetWidth();

      const GlyphID id = glyphs_data[k]->Id();
      font.LoadGlyphBitmap(&id, 1, &glyphs_data[k], Paint(), 1.f,
                           CurrentTransform());
      auto glyph_bitmap = glyphs_data[k]->Image();

      size_t row_bytes = glyph_bitmap.width;
      if (glyph_bitmap.format == BitmapFormat::kBGRA8) {
        row_bytes *= 4;
      }

      auto pixmap = std::make_shared<Pixmap>(
          skity::Data::MakeWithCopy(glyph_bitmap.buffer,
                                    row_bytes * glyph_bitmap.height),
          row_bytes, glyph_bitmap.width, glyph_bitmap.height,
          AlphaType::kOpaque_AlphaType, ToColorType(glyph_bitmap.format));
      if (glyph_bitmap.need_free) {
        std::free(glyph_bitmap.buffer);
      }

      this->DrawImage(Image::MakeImage(pixmap),
                      skity::Rect::MakeXYWH(x, y, w, h), SamplingOptions{},
                      nullptr);
    } else {
      SWRaster raster;

      raster.RastePath(path, CurrentTransform() * transform);

      DoBrush(raster, paint, false);
    }
  }
}
void SWCanvas::StrokeGlyphs(uint32_t count, const GlyphID* glyphs,
                            const float* position_x, const float* position_y,
                            const Font& font, const Paint& paint) {
  std::vector<const GlyphData*> glyphs_data(count);
  font.LoadGlyphPath(glyphs, count, glyphs_data.data());
  for (uint32_t k = 0; k < count; k++) {
    auto& path = glyphs_data[k]->GetPath();
    auto transform = Matrix::Translate(position_x[k], position_y[k]);

    Stroke stroke(paint);
    Path quad;
    Path outline;
    stroke.QuadPath(path, &quad);
    stroke.StrokePath(quad, &outline);

    SWRaster raster;

    raster.RastePath(outline, CurrentTransform() * transform);

    DoBrush(raster, paint, true);
  }
}

void SWCanvas::OnDrawImageRect(std::shared_ptr<Image> image, const Rect& src,
                               const Rect& dst, const SamplingOptions& sampling,
                               Paint const* paint) {
  SKITY_TRACE_EVENT(SWCanvas_OnDrawImageRect);

  if (!image) {
    return;
  }

  if (src.Width() == 0 || src.Height() == 0 || dst.Width() == 0 ||
      dst.Height() == 0) {
    return;
  }

  Paint work_paint = (paint == nullptr) ? Paint() : *paint;
  work_paint.SetStyle(Paint::kFill_Style);
  Matrix local_matrix;

  if (IsDrawingLayer()) {
    local_matrix = Matrix::Scale(1.f / src.Width(), 1.f / src.Height()) *
                   Matrix::Translate(-src.Left(), -src.Top());
  } else {
    local_matrix =
        Matrix::Translate(dst.Left(), dst.Top()) *
        Matrix::Scale(dst.Width() / src.Width(), dst.Height() / src.Height()) *
        Matrix::Translate(-src.Left(), -src.Top());
  }

  auto shader = Shader::MakeShader(std::move(image), sampling, TileMode::kDecal,
                                   TileMode::kDecal, local_matrix);
  work_paint.SetShader(std::move(shader));

  Path path;
  path.AddRect(dst);

  this->OnDrawPath(path, work_paint);
}

void SWCanvas::OnSave() {
  if (PeekLayerStack()) {
    PeekLayerStack()->canvas->Save();
    return;
  }

  state_stack_.emplace_back(state_stack_.back());
}

void SWCanvas::OnRestore() {
  if (state_stack_.size() == 1) {
    return;
  }

  if (PeekLayerStack()) {
    auto sub_canvas = static_cast<SWCanvas*>(PeekLayerStack()->canvas.get());

    if (sub_canvas->state_stack_.size() > 1) {
      sub_canvas->Restore();

      return;
    }
  }

  if (state_stack_.back().has_layer) {
    OnLayerRestore();
  }

  state_stack_.pop_back();
}

void SWCanvas::OnRestoreToCount(int saveCount) {
  if (saveCount < 1) {
    // invalid count
    return;
  }

  while (state_stack_.size() > static_cast<size_t>(saveCount)) {
    this->OnRestore();
  }
}

void SWCanvas::OnFlush() {}

uint32_t SWCanvas::OnGetWidth() const { return bitmap_->Width(); }

uint32_t SWCanvas::OnGetHeight() const { return bitmap_->Height(); }

void SWCanvas::OnUpdateViewport(uint32_t, uint32_t) {}

std::unique_ptr<SWSpanBrush> SWCanvas::GenerateBrush(
    std::vector<Span> const& spans, skity::Paint const& paint, bool stroke,
    Rect const& bounds) {
  auto shader = paint.GetShader();
  if (shader) {
    const auto* image_ptr = paint.GetShader()->AsImage();
    Shader::GradientInfo info{};
    Shader::GradientType type = shader->AsGradient(&info);

    if (type == Shader::kLinear || type == Shader::kRadial ||
        type == Shader::kConical || type == Shader::kSweep) {
      Matrix device_to_local;
      shader->GetLocalMatrix().Invert(&device_to_local);

      if (IsDrawingLayer()) {
        device_to_local =
            device_to_local *
            Matrix::Scale(1.0f / bounds.Width(), 1.0f / bounds.Height()) *
            Matrix::Translate(-bounds.Left(), -bounds.Top());
      } else {
        Matrix layer_to_local;
        CurrentTransform().Invert(&layer_to_local);
        device_to_local = device_to_local * layer_to_local;
      }

      return GradientColorBrush::MakeGradientColorBrush(
          spans, bitmap_, paint.GetColorFilter().get(), paint.GetBlendMode(),
          info, type, device_to_local);
    } else if (image_ptr) {
      const auto& image = *image_ptr;
      assert(image->GetPixmap());
      auto pixmap_shader =
          std::static_pointer_cast<PixmapShader>(paint.GetShader());

      auto pixmap = *(image->GetPixmap());

      uint32_t img_width = pixmap->Width();
      uint32_t img_height = pixmap->Height();

      Matrix inverse;
      shader->GetLocalMatrix().Invert(&inverse);

      Matrix matrix =
          Matrix::Scale(1.f / img_width, 1.f / img_height) * inverse;

      if (IsDrawingLayer()) {
        matrix = matrix *
                 Matrix::Scale(1.0f / bounds.Width(), 1.0f / bounds.Height()) *
                 Matrix::Translate(-bounds.Left(), -bounds.Top());
      } else {
        Matrix layer_to_local;
        CurrentTransform().Invert(&layer_to_local);
        matrix = matrix * layer_to_local;
      }

      return std::make_unique<PixmapBrush>(
          spans, bitmap_, paint.GetColorFilter().get(), paint.GetBlendMode(),
          paint.GetAlphaF(), std::move(pixmap), matrix,
          pixmap_shader->GetSamplingOptions()->filter,
          pixmap_shader->GetXTileMode(), pixmap_shader->GetYTileMode());
    }
  }

  Color4f color = stroke ? paint.GetStrokeColor() : paint.GetFillColor();

  return std::make_unique<SolidColorBrush>(spans, bitmap_,
                                           paint.GetColorFilter().get(),
                                           paint.GetBlendMode(), color);
}

void SWCanvas::HandleFilter(Path const& path, Paint const& paint) {
  Paint work_paint = paint;
  work_paint.SetMaskFilter(nullptr);
  work_paint.SetImageFilter(nullptr);

  auto mask_filter = paint.GetMaskFilter();
  auto image_filter = As_IFB(paint.GetImageFilter().get());
  auto bounds = ComputeBoundsIfStroke(path.GetBounds(), paint);

  float radius_x =
      mask_filter ? mask_filter->GetBlurRadius() : image_filter->GetRadiusX();
  float radius_y =
      mask_filter ? mask_filter->GetBlurRadius() : image_filter->GetRadiusY();
  auto filter_bounds =
      ImageFilterBase::ApproximateFilteredBounds(bounds, radius_x, radius_y);

  Bitmap bitmap(filter_bounds.Width(), filter_bounds.Height(),
                AlphaType::kPremul_AlphaType);

  auto temp_canvas = Canvas::MakeSoftwareCanvas(&bitmap);
  temp_canvas->Translate(-filter_bounds.Left(), -filter_bounds.Top());
  temp_canvas->DrawPath(path, work_paint);

  if (mask_filter) {
    MaskFilterOnFilter(this, bitmap, filter_bounds, work_paint,
                       mask_filter.get());
  } else {
    image_filter->OnFilter(this, bitmap, filter_bounds, work_paint);
  }
}

void SWCanvas::HandleFilter(uint32_t count, const GlyphID* glyphs,
                            const float* position_x, const float* position_y,
                            const Font& font, const Paint& paint) {
  Paint work_paint = paint;
  work_paint.SetMaskFilter(nullptr);
  work_paint.SetImageFilter(nullptr);

  auto mask_filter = paint.GetMaskFilter();
  auto image_filter = As_IFB(paint.GetImageFilter().get());
  auto bounds = TextBlob::ComputeBounds(count, glyphs, position_x, position_y,
                                        font, paint);
  auto radius_x =
      mask_filter ? mask_filter->GetBlurRadius() : image_filter->GetRadiusX();
  auto radius_y =
      mask_filter ? mask_filter->GetBlurRadius() : image_filter->GetRadiusY();
  auto filter_bounds =
      ImageFilterBase::ApproximateFilteredBounds(bounds, radius_x, radius_y);

  Bitmap bitmap(filter_bounds.Width(), filter_bounds.Height(),
                AlphaType::kPremul_AlphaType);

  auto temp_canvas = Canvas::MakeSoftwareCanvas(&bitmap);
  temp_canvas->Translate(-filter_bounds.Left(), -filter_bounds.Top());
  static_cast<SWCanvas*>(temp_canvas.get())
      ->DrawGlyphsInternal(count, glyphs, position_x, position_y, font,
                           work_paint);

  if (mask_filter) {
    ImageFilterBase::BlurBitmapToCanvas(this, bitmap, filter_bounds, work_paint,
                                        mask_filter->GetBlurRadius(),
                                        mask_filter->GetBlurRadius());
  } else {
    image_filter->OnFilter(this, bitmap, filter_bounds, work_paint);
  }
}

SWCanvas::LayerState* SWCanvas::PeekLayerStack() {
  if (layer_stack_.empty()) {
    return nullptr;
  }

  return layer_stack_.back().get();
}

std::unique_ptr<SWCanvas::LayerState> SWCanvas::PopLayerStack() {
  auto layer = std::move(layer_stack_.back());

  layer_stack_.pop_back();

  return layer;
}

SWCanvas::LayerState* SWCanvas::GenerateLayer(Rect const& rel_bounds,
                                              Rect const& log_bounds,
                                              Vec2 const& offset) {
  layer_stack_.emplace_back(
      std::make_unique<LayerState>(rel_bounds, log_bounds));

  layer_stack_.back()->Init(this, offset);

  return layer_stack_.back().get();
}

void SWCanvas::OnLayerRestore() {
  auto layer = PopLayerStack();

  if (PeekLayerStack()) {
    PeekLayerStack()->canvas->OnRestore();
  }

  SetDrawingLayer(layer->paint.GetMaskFilter() == nullptr);
  this->DrawImage(Image::MakeImage(layer->bitmap->GetPixmap()),
                  layer->log_bounds, &layer->paint);
  SetDrawingLayer(false);
}

std::unique_ptr<SWCanvas> SWCanvas::CreateSubCanvas(Bitmap* bitmap,
                                                    const Vec2& global_offset) {
  auto sub_canvas = std::make_unique<SWCanvas>(bitmap);
  sub_canvas->SetTracingCanvasState(false);
  sub_canvas->parent_canvas_ = this;
  sub_canvas->global_offset_ = global_offset;

  return sub_canvas;
}

}  // namespace skity
