// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_SW_SW_CANVAS_HPP
#define SRC_RENDER_SW_SW_CANVAS_HPP

#include <glm/gtc/matrix_transform.hpp>
#include <skity/render/canvas.hpp>

#include "src/render/canvas_state.hpp"
#include "src/render/sw/sw_subpixel.hpp"

#ifndef SKITY_CPU
#error "NOT Enable CPU Backend"
#endif

namespace skity {

class Bitmap;

class SWSpanBrush;
class SWRaster;

class SWCanvas : public Canvas {
  struct State {
    std::vector<Span> clip_spans_ = {};
    ClipOp op = ClipOp::kIntersect;

    bool has_layer = false;

    State() : clip_spans_() {}
    State(State const&) = default;
    State& operator=(State const&) = default;

    bool HasClip() const { return !clip_spans_.empty(); }

    std::vector<Span> PerformClip(std::vector<Span> const& spans);

    std::vector<Span> RecursiveClip(std::vector<Span> const& spans, ClipOp op);

   private:
    std::vector<Span> FindSpan(Span const& span);

    std::vector<Span> PerformMerge(std::vector<Span> const& spans);
  };

  struct LayerState {
    Rect rel_bounds = {};
    Rect log_bounds = {};

    std::unique_ptr<Bitmap> bitmap = {};
    std::unique_ptr<SWCanvas> canvas = {};

    Paint paint = {};

    LayerState(Rect const& rel_bounds, Rect const& log_bounds)
        : rel_bounds(rel_bounds), log_bounds(log_bounds) {}

    ~LayerState() = default;

    void Init(SWCanvas* parent_canvas, Vec2 offset);
  };

 public:
  explicit SWCanvas(Bitmap* bitmap);
  ~SWCanvas() override = default;

 protected:
  void OnDrawLine(float x0, float y0, float x1, float y1,
                  Paint const& paint) override;

  void OnClipRect(const Rect& rect, ClipOp op) override;

  void OnClipPath(const Path& path, ClipOp op) override;

  void OnDrawPath(const Path& path, const Paint& paint) override;

  void OnDrawPaint(const Paint& paint) override;

  void OnSaveLayer(const Rect& bounds, const Paint& paint) override;

  void OnDrawBlob(const TextBlob* blob, float x, float y,
                  Paint const& paint) override;

  void OnDrawGlyphs(uint32_t count, const GlyphID* glyphs,
                    const float* position_x, const float* position_y,
                    const Font& font, const Paint& paint) override;

  void OnDrawImageRect(std::shared_ptr<Image> image, const Rect& src,
                       const Rect& dst, const SamplingOptions& sampling,
                       Paint const* paint) override;

  void OnSave() override;

  void OnRestore() override;

  void OnRestoreToCount(int saveCount) override;

  void OnFlush() override;

  uint32_t OnGetWidth() const override;

  uint32_t OnGetHeight() const override;

  void OnUpdateViewport(uint32_t width, uint32_t height) override;

  CanvasState* GetCanvasState() const override {
    if (parent_canvas_) {
      return parent_canvas_->GetCanvasState();
    }
    return Canvas::GetCanvasState();
  }

 private:
  std::unique_ptr<SWSpanBrush> GenerateBrush(std::vector<Span> const& spans,
                                             skity::Paint const& paint,
                                             bool stroke, Rect const& bounds);

  State* CurrentState() { return &state_stack_.back(); }

  void DoBrush(const SWRaster& raster, const Paint& paint, bool stroke);

  void DrawGlyphsInternal(uint32_t count, const GlyphID* glyphs,
                          const float* position_x, const float* position_y,
                          const Font& font, const Paint& paint);

  void FillGlyphs(uint32_t count, const GlyphID* glyphs,
                  const float* position_x, const float* position_y,
                  const Font& font, const Paint& paint);
  void StrokeGlyphs(uint32_t count, const GlyphID* glyphs,
                    const float* position_x, const float* position_y,
                    const Font& font, const Paint& paint);

  void HandleFilter(uint32_t count, const GlyphID* glyphs,
                    const float* position_x, const float* position_y,
                    const Font& font, const Paint& paint);

  void HandleFilter(Path const& path, Paint const& paint);

  LayerState* PeekLayerStack();

  std::unique_ptr<LayerState> PopLayerStack();

  LayerState* GenerateLayer(Rect const& rel_bounds, Rect const& log_bounds,
                            Vec2 const& offset);

  void OnLayerRestore();

  std::unique_ptr<SWCanvas> CreateSubCanvas(Bitmap* bitmap, const Vec2& offset);

  const Rect& GetGlobalClipBounds() const override {
    if (parent_canvas_) {
      return parent_canvas_->GetGlobalClipBounds();
    }
    return Canvas::GetGlobalClipBounds();
  }

  Rect GetScanClipBounds() const {
    Rect clip_bounds = GetGlobalClipBounds();
    clip_bounds.Offset(-global_offset_.x, -global_offset_.y);
    return clip_bounds;
  }

  bool IsDrawingLayer() const {
    if (parent_canvas_) {
      return parent_canvas_->drawing_layer_;
    }
    return drawing_layer_;
  }

  void SetDrawingLayer(bool drawing_layer) {
    if (parent_canvas_) {
      parent_canvas_->drawing_layer_ = drawing_layer;

    } else {
      drawing_layer_ = drawing_layer;
    }
  }

  Matrix CurrentTransform() {
    return Matrix::Translate(-global_offset_.x, -global_offset_.y) *
           GetTotalMatrix();
  }

 private:
  Bitmap* bitmap_;
  // TODO(tangruiwen) state can use copy on write time template
  std::vector<State> state_stack_;
  std::vector<std::unique_ptr<LayerState>> layer_stack_;
  SWCanvas* parent_canvas_ = nullptr;
  Vec2 global_offset_ = Vec2{0.f, 0.f};
  bool drawing_layer_ = false;
};

}  // namespace skity

#endif  // SRC_RENDER_SW_SW_CANVAS_HPP
