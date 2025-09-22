// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_CANVAS_HPP
#define SRC_RENDER_HW_HW_CANVAS_HPP

#include <map>
#include <memory>
#include <skity/geometry/point.hpp>
#include <skity/render/canvas.hpp>
#include <skity/text/text_run.hpp>
#include <string>
#include <vector>

#include "src/render/hw/hw_pipeline_lib.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/render/hw/hw_static_buffer.hpp"
#include "src/render/hw/layer/hw_root_layer.hpp"
#include "src/utils/arena_allocator.hpp"
#include "src/utils/vector_cache.hpp"

namespace skity {

class GPUSurfaceImpl;

/**
 * @class HWCanvas
 *  Base class for all hardware canvas implementation, use MSAA for anti-alias
 */
class HWCanvas : public Canvas {
 public:
  explicit HWCanvas(GPUSurfaceImpl* surface);

  ~HWCanvas() override;

  void BeginNewFrame(HWRootLayer* root_layer);

 protected:
  void OnDrawLine(float x0, float y0, float x1, float y1,
                  Paint const& paint) override;

  void OnDrawRect(Rect const& rect, Paint const& paint) override;

  void OnClipRect(const Rect& rect, ClipOp op) override;

  void OnClipPath(const Path& path, ClipOp op) override;

  void OnDrawPath(const Path& path, const Paint& paint) override;

  void OnDrawPaint(const Paint& paint) override;

  void OnSaveLayer(const Rect& bounds, const Paint& paint) override;

  void OnDrawBlob(const TextBlob* blob, float x, float y,
                  Paint const& paint) override;

  void OnDrawImageRect(std::shared_ptr<Image> image, const Rect& src,
                       const Rect& dst, const SamplingOptions& sampling,
                       Paint const* paint) override;

  void OnDrawGlyphs(uint32_t count, const GlyphID* glyphs,
                    const float* position_x, const float* position_y,
                    const Font& font, const Paint& paint) override;

  void OnSave() override;

  void OnRestore() override;

  void OnRestoreToCount(int saveCount) override;

  void OnFlush() override;

  uint32_t OnGetWidth() const override;

  uint32_t OnGetHeight() const override;

  void OnUpdateViewport(uint32_t width, uint32_t height) override;

 private:
  void Init();

  uint32_t GetCanvasSampleCount();

  void UploadMesh();

  HWLayer* CurrentLayer();

  HWLayer* GenLayer(const Paint& paint, Rect layer_bounds,
                    const Matrix& local_to_layer);

  void DrawGlyphsInternal(uint32_t count, const GlyphID* glyphs,
                          const Point& origin, const float* position_x,
                          const float* position_y, const Font& font,
                          const Paint& paint, const Matrix& transform);

  void DrawPathInternal(const Path& path, const Paint& paint,
                        const Matrix& transform);

  const Matrix& CurrentMatrix() const {
    return GetCanvasState()->CurrentLayerMatrix();
  }

  void SetupLayerSpaceBoundsForDraw(HWDraw* draw, Rect bounds,
                                    bool is_local = true) {
    Matrix transform = is_local ? CurrentMatrix() : Matrix{};
    draw->SetLayerSpaceBounds(
        CurrentLayer()->CalculateLayerSpaceBounds(bounds, transform));
  }

  bool NeesOffScreenLayer(const Paint& paint) const;

 private:
  GPUSurfaceImpl* surface_ = nullptr;
  float ctx_scale_ = 1.f;
  bool enable_msaa_ = false;
  bool enable_fxaa_ = false;
  HWStageBuffer* gpu_buffer_ = {};

  HWPipelineLib* pipeline_lib_ = {};
  std::unique_ptr<VectorCache<float>> vertex_vector_cache_;
  std::unique_ptr<VectorCache<uint32_t>> index_vector_cache_;
  HWRootLayer* root_layer_;

  ArrayList<HWLayer*, 8> layer_stack_ = {};
  ArenaAllocator* arena_allocator_ = {};
  HWStaticBuffer* static_buffer_ = {};
};

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_CANVAS_HPP
