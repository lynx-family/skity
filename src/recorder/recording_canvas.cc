// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cstddef>
#include <cstring>
#include <skity/recorder/recording_canvas.hpp>

#include "src/recorder/display_list_builder.hpp"
#include "src/recorder/recorded_op.hpp"
#include "src/render/canvas_state.hpp"

namespace skity {

#define DL_BUILDER_PAGE 4096

static constexpr inline bool is_power_of_two(int value) {
  return (value & (value - 1)) == 0;
}

template <typename T>
static constexpr T Align4(T x) {
  return (x + 3) >> 2 << 2;
}
template <typename T>
static constexpr T Align8(T x) {
  return (x + 7) >> 3 << 3;
}

template <typename T>
static constexpr T AlignPtr(T x) {
  return sizeof(void*) == 8 ? Align8(x) : Align4(x);
}

template <typename T, typename... Args>
void RecordingCanvas::Push(Args&&... args) {
  size_t size = AlignPtr(sizeof(T));
  if (dp_builder_->used_ + size > dp_builder_->allocated_) {
    static_assert(is_power_of_two(DL_BUILDER_PAGE),
                  "This math needs updating for non-pow2.");
    dp_builder_->allocated_ =
        (dp_builder_->used_ + size + DL_BUILDER_PAGE) & ~(DL_BUILDER_PAGE - 1);
    dp_builder_->storage_.realloc(dp_builder_->allocated_);
    std::memset(dp_builder_->storage_.get() + dp_builder_->used_, 0,
                dp_builder_->allocated_ - dp_builder_->used_);
  }

  auto op =
      reinterpret_cast<T*>(dp_builder_->storage_.get() + dp_builder_->used_);
  dp_builder_->last_op_offset_ = dp_builder_->used_;
  dp_builder_->used_ += size;

  new (op) T{std::forward<Args>(args)...};

  op->size = size;

  dp_builder_->render_op_count_++;
}

RecordingCanvas::RecordingCanvas() {}

RecordingCanvas::~RecordingCanvas() {}

RecordedOpOffset RecordingCanvas::GetLastOpOffset() const {
  if (dp_builder_ == nullptr) {
    return RecordedOpOffset(-1);
  }
  return RecordedOpOffset(dp_builder_->last_op_offset_);
}

void RecordingCanvas::BindDisplayListBuilder(DisplayListBuilder* dp_builder) {
  dp_builder_ = dp_builder;
  CalculateGlobalClipBounds(dp_builder_->cull_rect_, ClipOp::kIntersect);
}

void RecordingCanvas::OnClipRect(Rect const& rect, ClipOp op) {
  Push<ClipRectOp>(rect, op);
}

void RecordingCanvas::OnClipPath(Path const& path, ClipOp op) {
  Push<ClipPathOp>(path, op);
}

void RecordingCanvas::OnDrawRect(Rect const& rect, Paint const& paint) {
  Push<DrawRectOp>(rect, paint);
  AccumulateOpBounds(rect, &paint);
}

void RecordingCanvas::OnDrawRRect(RRect const& rrect, Paint const& paint) {
  Push<DrawRRectOp>(rrect, paint);
  AccumulateOpBounds(rrect.GetBounds(), &paint);
}

void RecordingCanvas::OnDrawPath(Path const& path, Paint const& paint) {
  Push<DrawPathOp>(path, paint);
  AccumulateOpBounds(path.GetBounds(), &paint);
}

void RecordingCanvas::OnSaveLayer(const Rect& bounds, const Paint& paint) {
  Push<SaveLayerOp>(bounds, paint);
}

void RecordingCanvas::OnDrawBlob(const TextBlob* blob, float x, float y,
                                 Paint const& paint) {
  Push<DrawTextBlobOp>(blob, x, y, paint);
  auto bounds = blob->GetBoundsRect().MakeOffset(x, y);
  // Expand outward a little to prevent incomplete display of text content
  bounds = bounds.MakeOutset(1, 1);
  AccumulateOpBounds(bounds, &paint);
}

void RecordingCanvas::OnDrawImageRect(std::shared_ptr<Image> image,
                                      const Rect& src, const Rect& dst,
                                      const SamplingOptions& sampling,
                                      Paint const* paint) {
  Push<DrawImageOp>(std::move(image), src, dst, sampling, paint);
  AccumulateOpBounds(dst, paint);
}
void RecordingCanvas::OnDrawGlyphs(uint32_t count, const GlyphID glyphs[],
                                   const float position_x[],
                                   const float position_y[], const Font& font,
                                   const Paint& paint) {
  Push<DrawGlyphsOp>(count, glyphs, position_x, position_y, font, paint);
  // Since we only receive glyphs IDs, we cannot quickly calculate the bounds of
  // the drawing area.
  AccumulateOpBounds(GetGlobalClipBounds(), nullptr);
}
void RecordingCanvas::OnDrawPaint(Paint const& paint) {
  Push<DrawPaintOp>(paint);
  AccumulateOpBounds(GetGlobalClipBounds(), nullptr);
}
void RecordingCanvas::OnSave() { Push<SaveOp>(); }
void RecordingCanvas::OnRestore() { Push<RestoreOp>(); }
void RecordingCanvas::OnRestoreToCount(int saveCount) {
  Push<RestoreToCountOp>(saveCount);
}
void RecordingCanvas::OnTranslate(float dx, float dy) {
  Push<TranslateOp>(dx, dy);
}
void RecordingCanvas::OnScale(float sx, float sy) { Push<ScaleOp>(sx, sy); }
void RecordingCanvas::OnRotate(float degree) { Push<RotateByDegreeOp>(degree); }
void RecordingCanvas::OnRotate(float degree, float px, float py) {
  Push<RotateByPointOp>(degree, px, py);
}
void RecordingCanvas::OnSkew(float sx, float sy) { Push<SkewOp>(sx, sy); }
void RecordingCanvas::OnConcat(Matrix const& matrix) { Push<ConcatOp>(matrix); }
void RecordingCanvas::OnSetMatrix(Matrix const& matrix) {
  Push<SetMatrixOp>(matrix);
}
void RecordingCanvas::OnResetMatrix() { Push<ResetMatrixOp>(); }
void RecordingCanvas::OnFlush() {}
uint32_t RecordingCanvas::OnGetWidth() const { return 0; }
uint32_t RecordingCanvas::OnGetHeight() const { return 0; }
void RecordingCanvas::OnUpdateViewport(uint32_t width, uint32_t height) {
  // Do nothing since 'Canvas::UpdateViewport' is deprecated.
}

void RecordingCanvas::AccumulateOpBounds(const Rect& raw_bounds,
                                         const Paint* paint) {
  Rect bounds =
      paint != nullptr ? paint->ComputeFastBounds(raw_bounds) : raw_bounds;
  Rect mapped_bounds;
  GetTotalMatrix().MapRect(&mapped_bounds, bounds);
  if (!mapped_bounds.Intersect(GetGlobalClipBounds())) {
    return;
  }
  dp_builder_->bounds_.Join(mapped_bounds);
}

}  // namespace skity
