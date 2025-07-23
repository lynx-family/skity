// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cstring>
#include <memory>
#include <skity/render/canvas.hpp>
#include <skity/text/font.hpp>
#include <skity/text/text_blob.hpp>
#include <skity/text/utf.hpp>

#include "src/geometry/math.hpp"
#include "src/graphic/path_priv.hpp"
#include "src/logging.hpp"
#include "src/render/canvas_state.hpp"

namespace skity {

Canvas::Canvas(Rect cull_rect) {
  canvas_state_ = std::make_unique<CanvasState>();
  global_clip_bounds_stack_.push_back(cull_rect);
}

Canvas::~Canvas() = default;

int Canvas::Save() {
  save_count_ += 1;

  if (tracing_canvas_state_) {
    global_clip_bounds_stack_.push_back(global_clip_bounds_stack_.back());
    canvas_state_->Save();
  }

  this->InternalSave();
  return this->GetSaveCount() - 1;
}

void Canvas::Restore() {
  if (save_count_ > 1) {
    save_count_ -= 1;
    if (tracing_canvas_state_) {
      global_clip_bounds_stack_.pop_back();
      canvas_state_->Restore();
    }
    this->InternalRestore();
  }
}

int Canvas::GetSaveCount() const { return save_count_; }

void Canvas::RestoreToCount(int count) {
  if (count < 1) {
    count = 1;
  }

  if (count >= save_count_) {
    return;
  }

  if (count < 1) {
    count = 1;
  }

  int n = save_count_ - count;
  for (int i = 0; i < n; ++i) {
    this->Restore();
  }
}

void Canvas::Translate(float dx, float dy) {
  if (tracing_canvas_state_) {
    canvas_state_->Translate(dx, dy);
  }
  OnTranslate(dx, dy);
}

void Canvas::Scale(float sx, float sy) {
  if (tracing_canvas_state_) {
    canvas_state_->Scale(sx, sy);
  }
  OnScale(sx, sy);
}

void Canvas::Rotate(float degrees) {
  if (tracing_canvas_state_) {
    canvas_state_->Rotate(degrees);
  }
  OnRotate(degrees);
}

void Canvas::Rotate(float degrees, float px, float py) {
  if (tracing_canvas_state_) {
    canvas_state_->Rotate(degrees, px, py);
  }
  OnRotate(degrees, px, py);
}

void Canvas::Skew(float sx, float sy) {
  if (tracing_canvas_state_) {
    canvas_state_->Skew(sx, sy);
  }
  OnSkew(sx, sy);
}

void Canvas::Concat(const Matrix &matrix) {
  if (tracing_canvas_state_) {
    canvas_state_->Concat(matrix);
  }
  OnConcat(matrix);
}

void Canvas::SetMatrix(const Matrix &matrix) {
  if (tracing_canvas_state_) {
    canvas_state_->SetMatrix(matrix);
  }
  OnSetMatrix(matrix);
}

void Canvas::ResetMatrix() {
  if (tracing_canvas_state_) {
    canvas_state_->ResetMatrix();
  }
  OnResetMatrix();
}

Matrix Canvas::GetTotalMatrix() const {
  return GetCanvasState()->GetTotalMatrix();
}

void Canvas::ClipRect(const Rect &rect, ClipOp op) {
  if (tracing_canvas_state_) {
    CalculateGlobalClipBounds(rect, op);
  }
  this->OnClipRect(rect, op);
}

void Canvas::OnClipRect(const Rect &rect, ClipOp op) {
  Path path;
  path.AddRect(rect);
  path.SetConvexityType(Path::ConvexityType::kConvex);

  this->OnClipPath(path, op);
}

void Canvas::ClipPath(const Path &path, ClipOp op) {
  if (tracing_canvas_state_) {
    CalculateGlobalClipBounds(path.GetBounds(), op);
  }
  this->OnClipPath(path, op);
}

void Canvas::DrawLine(float x0, float y0, float x1, float y1,
                      const Paint &paint) {
  // specify call draw line can only have stroke style.
  // If pass default paint need to force change paint.style to stroke
  Paint wp{paint};
  if (wp.GetStyle() != Paint::kStroke_Style) {
    wp.SetStyle(Paint::kStroke_Style);
  }

  this->OnDrawLine(x0, y0, x1, y1, wp);
}

void Canvas::DrawCircle(float cx, float cy, float radius, Paint const &paint) {
  this->OnDrawCircle(cx, cy, radius, paint);
}

void Canvas::DrawArc(Rect const &oval, float startAngle, float sweepAngle,
                     bool useCenter, Paint const &paint) {
  if (FloatNearlyZero(sweepAngle)) {
    return;
  }

  if (FloatNearlyZero(oval.Width()) || FloatNearlyZero(oval.Height())) {
    return;
  }

  Path path;
  bool isFillNoPathEffect =
      paint.GetStyle() == Paint::kFill_Style && !paint.GetPathEffect();

  PathPriv::CreateDrawArcPath(&path, oval, startAngle, sweepAngle, useCenter,
                              isFillNoPathEffect);

  this->OnDrawPath(path, paint);
}

void Canvas::DrawOval(Rect const &oval, Paint const &paint) {
  this->OnDrawOval(oval, paint);
}

void Canvas::DrawRect(Rect const &rect, Paint const &paint) {
  this->OnDrawRect(rect, paint);
}

void Canvas::DrawRRect(RRect const &rrect, Paint const &paint) {
  this->OnDrawRRect(rrect, paint);
}

void Canvas::DrawRoundRect(Rect const &rect, float rx, float ry,
                           Paint const &paint) {
  this->OnDrawRoundRect(rect, rx, ry, paint);
}

void Canvas::DrawPath(const Path &path, const Paint &paint) {
  this->OnDrawPath(path, paint);
}

void Canvas::DrawColor(Color color, BlendMode mode) {
  this->DrawColor(Color4fFromColor(color), mode);
}

void Canvas::DrawColor(Color4f color, BlendMode mode) {
  Paint paint;
  paint.SetFillColor(color);
  paint.SetBlendMode(mode);

  this->DrawPaint(paint);
}

void Canvas::DrawPaint(const Paint &paint) { this->OnDrawPaint(paint); }

int Canvas::SaveLayer(const Rect &bounds, const Paint &paint) {
  if (bounds.IsEmpty() || QuickReject(bounds)) {
    LOGW(
        "Bounds of saving layer is empty or outside global clip bounds, no "
        "layer is saving and no other draw "
        "will effect until restore() is called");

    int depth = Save();
    ClipRect(Rect::MakeEmpty());
    return depth;
  }

  save_count_++;
  if (tracing_canvas_state_) {
    global_clip_bounds_stack_.push_back(global_clip_bounds_stack_.back());
    CalculateGlobalClipBounds(bounds, ClipOp::kIntersect);
  }

  OnSaveLayer(bounds, paint);
  if (tracing_canvas_state_) {
    canvas_state_->SaveLayer(bounds, paint);
  }
  return save_count_ - 1;
}

void Canvas::Flush() { this->OnFlush(); }

void Canvas::DrawSimpleText(const char *text, float x, float y,
                            Paint const &paint) {
  this->DrawSimpleText2(text, x, y, paint);
}

void Canvas::DrawSimpleText2(const char *text, float x, float y,
                             const Paint &paint) {
  auto typeface = Typeface::GetDefaultTypeface();
  if (paint.GetTypeface()) {
    typeface = paint.GetTypeface();
  }
  if (!typeface) {
    return;
  }

  Paint work_paint{paint};
  work_paint.SetTypeface(typeface);

  skity::TextBlobBuilder builder;

  auto blob = builder.BuildTextBlob(text, work_paint);

  this->DrawTextBlob(blob.get(), x, y, work_paint);
}

Vec2 Canvas::SimpleTextBounds(const char *text, const Paint &paint) {
  std::vector<const GlyphData *> glyphs;
  std::vector<Unichar> code_points;
  if (!UTF::UTF8ToCodePoint(text, std::strlen(text), code_points)) {
    return {0.f, 0.f};
  }

  auto typeface = paint.GetTypeface() ? paint.GetTypeface()
                                      : Typeface::GetDefaultTypeface();
  if (!typeface) {
    return {0.f, 0.f};
  }

  std::vector<GlyphID> glyph_id(code_points.size());
  typeface->UnicharsToGlyphs(code_points.data(), code_points.size(),
                             glyph_id.data());

  glyphs.resize(glyph_id.size());

  Font font(typeface, paint.GetTextSize());
  font.LoadGlyphMetrics(glyph_id.data(), glyph_id.size(), glyphs.data(), paint);
  float total_width = 0.f;
  float max_height = 0.f;
  for (auto &glyph : glyphs) {
    total_width += glyph->AdvanceX();
    max_height = std::max(max_height, glyph->GetHeight());
  }
  return {total_width, max_height};
}

void Canvas::DrawTextBlob(const TextBlob *blob, float x, float y,
                          const Paint &paint) {
  if (blob == nullptr) {
    // prevent null pointer abort when text renderer is not enable
    return;
  }

  this->OnDrawBlob(blob, x, y, paint);
}

void Canvas::DrawImage(const std::shared_ptr<Image> &image, float x, float y) {
  this->DrawImage(image, x, y, SamplingOptions());
}

void Canvas::DrawImage(const std::shared_ptr<Image> &image, float x, float y,
                       const SamplingOptions &sampling, const Paint *paint) {
  if (!image) {
    return;
  }
  auto src = Rect::MakeWH(image->Width(), image->Height());
  auto dst = Rect::MakeXYWH(x, y, src.Width(), src.Height());
  this->DrawImageRect(image, src, dst, sampling, paint);
}

void Canvas::DrawImage(const std::shared_ptr<Image> &image, const Rect &rect,
                       const Paint *paint) {
  this->DrawImage(image, rect, SamplingOptions{}, paint);
}

void Canvas::DrawImage(const std::shared_ptr<Image> &image, const Rect &rect,
                       const SamplingOptions &sampling, const Paint *paint) {
  if (!image) {
    return;
  }
  auto src = Rect::MakeWH(image->Width(), image->Height());
  this->OnDrawImageRect(image, src, rect, sampling, paint);
}

void Canvas::DrawImageRect(const std::shared_ptr<Image> &image, const Rect &src,
                           const Rect &dst, const SamplingOptions &sampling,
                           const Paint *paint) {
  if (!image) {
    return;
  }
  this->OnDrawImageRect(image, src, dst, sampling, paint);
}

void Canvas::DrawGlyphs(int count, const GlyphID *glyphs,
                        const float *position_x, const float *position_y,
                        const Font &font, const Paint &paint) {
  this->OnDrawGlyphs(count, glyphs, position_x, position_y, font, paint);
}

void Canvas::UpdateViewport(uint32_t width, uint32_t height) {
  this->OnUpdateViewport(width, height);
}

uint32_t Canvas::Width() const { return this->OnGetWidth(); }

uint32_t Canvas::Height() const { return this->OnGetHeight(); }

void Canvas::InternalSave() { this->OnSave(); }

void Canvas::InternalRestore() { this->OnRestore(); }

void Canvas::OnDrawLine(float x0, float y0, float x1, float y1,
                        Paint const &paint) {
  Path path;
  path.MoveTo(x0, y0);
  path.LineTo(x1, y1);

  this->OnDrawPath(path, paint);
}

void Canvas::OnDrawCircle(float cx, float cy, float radius,
                          Paint const &paint) {
  if (radius < 0) {
    radius = 0;
  }

  Rect r;
  r.SetLTRB(cx - radius, cy - radius, cx + radius, cy + radius);
  this->DrawOval(r, paint);
}

void Canvas::OnDrawOval(Rect const &oval, Paint const &paint) {
  RRect rrect;
  rrect.SetOval(oval);
  this->DrawRRect(rrect, paint);
}

void Canvas::OnDrawRRect(RRect const &rrect, Paint const &paint) {
  Path path;
  path.AddRRect(rrect);
  path.SetConvexityType(Path::ConvexityType::kConvex);

  this->DrawPath(path, paint);
}

void Canvas::OnDrawRect(Rect const &rect, Paint const &paint) {
  Path path;
  path.AddRect(rect);

  this->DrawPath(path, paint);
}

void Canvas::OnDrawRoundRect(Rect const &rect, float rx, float ry,
                             Paint const &paint) {
  if (rx > 0 && ry > 0) {
    RRect rrect;
    rrect.SetRectXY(rect, rx, ry);
    this->DrawRRect(rrect, paint);
  } else {
    this->DrawRect(rect, paint);
  }
}

bool Canvas::NeedGlyphPath(Paint const &paint) {
  return paint.GetStyle() != Paint::kFill_Style;
}

void Canvas::CalculateGlobalClipBounds(const Rect &local_clip_bounds,
                                       ClipOp op) {
  if (!tracing_canvas_state_) {
    return;
  }

  if (op == ClipOp::kIntersect) {
    Rect mapped_rect;
    GetTotalMatrix().MapRect(&mapped_rect, local_clip_bounds);
    if (!global_clip_bounds_stack_.back().Intersect(mapped_rect)) {
      global_clip_bounds_stack_.back().SetEmpty();
    }
  }
}

bool Canvas::QuickReject(const Rect &local_rect) const {
  if (!tracing_canvas_state_) {
    return false;
  }

  Rect mapped_rect;
  GetTotalMatrix().MapRect(&mapped_rect, local_rect);
  return !Rect::Intersect(mapped_rect, GetGlobalClipBounds());
}

Rect Canvas::GetLocalClipBounds() const {
  auto &global_clip_bounds = GetGlobalClipBounds();
  if (global_clip_bounds.IsEmpty()) {
    return Rect::MakeEmpty();
  }

  Matrix inverse;
  if (!GetTotalMatrix().Invert(&inverse)) {
    return Rect::MakeEmpty();
  }

  Rect bounds;
  inverse.MapRect(&bounds, global_clip_bounds);
  return bounds;
}

}  // namespace skity
