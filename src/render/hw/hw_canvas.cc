// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_canvas.hpp"

#include <skity/effect/mask_filter.hpp>
#include <skity/effect/path_effect.hpp>
#include <skity/effect/shader.hpp>
#include <skity/geometry/stroke.hpp>
#include <skity/text/font.hpp>
#include <skity/text/text_blob.hpp>

#include "src/effect/image_filter_base.hpp"
#include "src/gpu/gpu_surface_impl.hpp"
#include "src/render/canvas_state.hpp"
#include "src/render/hw/draw/hw_dynamic_path_clip.hpp"
#include "src/render/hw/draw/hw_dynamic_path_draw.hpp"
#include "src/render/hw/filters/hw_filters.hpp"
#include "src/render/hw/layer/hw_filter_layer.hpp"
#include "src/render/text/glyph_run.hpp"
#include "src/tracing.hpp"
#include "src/utils/arena_allocator.hpp"

namespace skity {

HWCanvas::HWCanvas(GPUSurfaceImpl* surface)
    : Canvas(Rect::MakeWH(surface->GetWidth(), surface->GetHeight())),
      surface_(surface),
      ctx_scale_(surface->ContentScale()),
      enable_msaa_(surface->GetSampleCount() > 1),
      enable_fxaa_(surface->UseFxaa()),
      gpu_buffer_(surface->GetStageBuffer()),
      arena_allocator_(surface_->GetArenaAllocator()) {
  Init();
}

HWCanvas::~HWCanvas() {}

void HWCanvas::BeginNewFrame(HWRootLayer* root_layer) {
  SKITY_TRACE_EVENT(HWCanvas_BeginNewFrame);

  root_layer_ = root_layer;

  layer_stack_.clear();

  layer_stack_.emplace_back(root_layer_);
}

void HWCanvas::Init() {
  SKITY_TRACE_EVENT(HWCanvas_Init);

  enable_msaa_ = surface_->GetSampleCount() > 1;
  enable_fxaa_ = surface_->UseFxaa();
  pipeline_lib_ = surface_->GetGPUContext()->GetPipelineLib();

  vertex_vector_cache_ = std::make_unique<VectorCache<float>>();
  index_vector_cache_ = std::make_unique<VectorCache<uint32_t>>();
  layer_stack_.SetArenaAllocator(arena_allocator_);
}

uint32_t HWCanvas::OnGetWidth() const { return surface_->GetWidth(); }

uint32_t HWCanvas::OnGetHeight() const { return surface_->GetHeight(); }

void HWCanvas::OnUpdateViewport(uint32_t width, uint32_t height) {}

void HWCanvas::OnClipRect(const Rect& rect, ClipOp op) {
  SKITY_TRACE_EVENT(HWCanvas_OnClipRect);

  if (CurrentLayer() == nullptr) {
    return;
  }

  if (op == ClipOp::kDifference || CurrentMatrix().HasRotation()) {
    Canvas::OnClipRect(rect, op);
    return;
  }

  CurrentLayer()->AddRectClip(rect, CurrentMatrix());
}

void HWCanvas::OnClipPath(const Path& path, ClipOp op) {
  SKITY_TRACE_EVENT(HWCanvas_OnClipPath);

  auto layer = CurrentLayer();

  if (layer == nullptr) {
    return;
  }

  HWDraw* clip = arena_allocator_->Make<HWDynamicPathClip>(
      CurrentMatrix(), path, op, layer->GetBounds());

  if (clip == nullptr) {
    return;
  }

  clip->SetSampleCount(GetCanvasSampleCount());

  if (op != Canvas::ClipOp::kDifference) {
    CurrentLayer()->AddRectClip(path.GetBounds(), CurrentMatrix());
  }

  layer->AddClip(clip);
}

void HWCanvas::OnDrawLine(float x0, float y0, float x1, float y1,
                          Paint const& paint) {
  SKITY_TRACE_EVENT(HWCanvas_OnDrawLine);

  if (paint.GetStyle() != Paint::kStroke_Style) {
    return;
  }

  Path path;
  path.MoveTo(x0, y0);
  path.LineTo(x1, y1);

  this->OnDrawPath(path, paint);
}

void HWCanvas::OnDrawPath(const Path& path, const Paint& paint) {
  SKITY_TRACE_EVENT(HWCanvas_OnDrawPath);

  if (CurrentLayer() == nullptr) {
    return;
  }

  // early fail
  if (const auto* image_ptr =
          paint.GetShader() ? paint.GetShader()->AsImage() : nullptr;
      image_ptr && (*image_ptr)->IsTextureBackend() &&
      !(*image_ptr)->IsLazy() && (*image_ptr)->GetTexture() == nullptr) {
    return;
  }

  if (QuickReject(paint.ComputeFastBounds(path.GetBounds()))) {
    return;
  }

  bool has_layer = false;
  Matrix current_matrix = CurrentMatrix();
  Paint working_paint{paint};

  if (NeesOffScreenLayer(paint)) {
    Paint restore_paint{working_paint};
    restore_paint.SetAlphaF(1.0);

    working_paint.SetImageFilter(nullptr);
    working_paint.SetMaskFilter(nullptr);
    working_paint.SetColorFilter(nullptr);

    auto layer_bounds = working_paint.ComputeFastBounds(path.GetBounds());

    auto result = GenLayer(restore_paint, layer_bounds, current_matrix);
    if (!result) {
      return;
    }
    CurrentLayer()->AddDraw(result);
    layer_stack_.push_back(result);
    has_layer = true;
    current_matrix = Matrix{};
    working_paint.SetBlendMode(BlendMode::kSrcOver);
  }

  DrawPathInternal(path, working_paint, current_matrix);
  if (has_layer) {
    layer_stack_.pop_back();
  }
}

void HWCanvas::OnDrawPaint(const Paint& paint) {
  SKITY_TRACE_EVENT(HWCanvas_OnDrawPaint);

  if (CurrentLayer() == nullptr) {
    return;
  }

  auto rect = CurrentLayer()->GetBounds();

  this->OnDrawRect(rect, paint);
}

void HWCanvas::OnSaveLayer(const Rect& bounds, const Paint& paint) {
  SKITY_TRACE_EVENT(HWCanvas_OnSaveLayer);

  if (CurrentLayer() == nullptr) {
    return;
  }

  Paint restore_paint{paint};
  restore_paint.SetStyle(Paint::kFill_Style);
  auto layer = GenLayer(restore_paint, bounds, CurrentMatrix());
  if (!layer) {
    this->OnSave();
    this->OnClipRect(bounds, ClipOp::kIntersect);
    return;
  }

  CurrentLayer()->AddDraw(layer);
  layer_stack_.emplace_back(layer);
}

void HWCanvas::OnDrawRect(Rect const& rect, Paint const& paint) {
  SKITY_TRACE_EVENT(HWCanvas_OnDrawRect);

  Path path;
  path.AddRect(rect);

  this->OnDrawPath(path, paint);
}

void HWCanvas::OnDrawBlob(const TextBlob* blob, float x, float y,
                          Paint const& paint) {
  SKITY_TRACE_EVENT(HWCanvas_OnDrawBlob);

  if (CurrentLayer() == nullptr) {
    return;
  }

  bool has_layer = false;
  Matrix current_matrix = CurrentMatrix();
  Paint working_paint(paint);

  if (NeesOffScreenLayer(paint)) {
    Paint restore_paint(paint);
    restore_paint.SetAlphaF(1.0f);

    working_paint.SetMaskFilter(nullptr);
    working_paint.SetImageFilter(nullptr);
    working_paint.SetColorFilter(nullptr);
    auto bounds_size = blob->GetBoundSize();
    auto bounds =
        Rect::MakeXYWH(x, y - bounds_size.y, bounds_size.x, bounds_size.y);
    auto layer_bounds = working_paint.ComputeFastBounds(bounds);
    auto result = GenLayer(restore_paint, layer_bounds, current_matrix);
    if (!result) {
      return;
    }

    CurrentLayer()->AddDraw(result);
    layer_stack_.push_back(result);

    has_layer = true;
    current_matrix = Matrix{};
    working_paint.SetBlendMode(BlendMode::kSrcOver);
  }

  float advance_x = 0;
  for (auto const& run : blob->GetTextRun()) {
    auto const& glyphs = run.GetGlyphInfo();
    const Font& font = run.GetFont();
    if (!font.GetTypeface() || glyphs.empty()) {
      continue;
    }

    std::vector<float> pos_y{};
    std::vector<float> pos_x{};
    if (run.GetPosX().empty()) {
      std::vector<const GlyphData*> glyph_data(glyphs.size());
      pos_x.reserve(glyphs.size());
      pos_y.reserve(glyphs.size());

      font.LoadGlyphMetrics(glyphs.data(), glyphs.size(), glyph_data.data(),
                            working_paint);

      for (auto const& glyph : glyph_data) {
        pos_x.emplace_back(advance_x);
        pos_y.emplace_back(0);

        advance_x += glyph->AdvanceX();
      }
    } else {
      // Y in run is not necessary cause we can infer y from position of blob.
      pos_y.resize(run.GetPosX().size(), 0);
      for (size_t i = 0; i < pos_y.size(); ++i) {
        // Use default y value if posY is not provided.
        if (i < run.GetPosY().size()) {
          pos_y[i] += run.GetPosY()[i];
        }
      }

      for (auto rx : run.GetPosX()) {
        pos_x.emplace_back(rx);
      }
    }

    const Point origin{x, y, 0, 1};
    DrawGlyphsInternal(static_cast<uint32_t>(glyphs.size()), glyphs.data(),
                       origin, pos_x.data(), pos_y.data(), font, working_paint,
                       current_matrix);
  }

  if (has_layer) {
    layer_stack_.pop_back();
  }
}

void HWCanvas::OnDrawGlyphs(uint32_t count, const GlyphID* glyphs,
                            const float* position_x, const float* position_y,
                            const Font& font, const Paint& paint) {
  SKITY_TRACE_EVENT(HWCanvas_OnDrawGlyphs);

  if (CurrentLayer() == nullptr) {
    return;
  }

  const Point origin{0, 0, 0, 1};
  DrawGlyphsInternal(count, glyphs, origin, position_x, position_y, font, paint,
                     CurrentMatrix());
}

void HWCanvas::DrawGlyphsInternal(uint32_t count, const GlyphID* glyphs,
                                  const Point& origin, const float* position_x,
                                  const float* position_y, const Font& font,
                                  const Paint& paint, const Matrix& transform) {
  SKITY_TRACE_EVENT(HWCanvas_DrawGlyphsInternal);

  GlyphRunList glyph_runs = GlyphRun::Make(
      count, glyphs, origin, position_x, position_y, font, paint, ctx_scale_,
      transform, surface_->GetGPUContext()->GetAtlasManager(), arena_allocator_,
      [this, transform](const Path& path, const Paint& paint) {
        this->DrawPathInternal(path, paint, transform);
      });
  for (auto& glyph_run : glyph_runs) {
    auto draw =
        glyph_run->Draw(transform, arena_allocator_, ctx_scale_,
                        surface_->GetGPUContext()->IsEnableTextLinearFilter());
    if (draw) {
      draw->SetSampleCount(GetCanvasSampleCount());
      SetupLayerSpaceBoundsForDraw(
          draw, paint.ComputeFastBounds(glyph_run->GetBounds()), false);
      CurrentLayer()->AddDraw(draw);
    }
  }
}

void HWCanvas::DrawPathInternal(const Path& path, const Paint& paint,
                                const Matrix& transform) {
  SKITY_TRACE_EVENT(HWCanvas_DrawPathInternal);

  bool need_fill = paint.GetStyle() != Paint::kStroke_Style;
  bool need_stroke = paint.GetStyle() != Paint::kFill_Style;
  bool need_contour_aa = false;
  if (surface_->GetGPUContext()->IsEnableContourAA()) {
    // Fall back to Contour AA while surface disables MSAA and paint enables AA
    need_contour_aa = !enable_msaa_ && !enable_fxaa_ && paint.IsAntiAlias();
  }

  auto draw_op_handler = [&](const Path& path, const Paint& paint,
                             bool use_stroke) {
    HWDraw* draw = arena_allocator_->Make<HWDynamicPathDraw>(transform, path,
                                                             paint, use_stroke);

    if (draw == nullptr) {
      return;
    }

    draw->SetSampleCount(GetCanvasSampleCount());
    auto bounds = use_stroke ? paint.ComputeFastBounds(path.GetBounds())
                             : path.GetBounds();
    SetupLayerSpaceBoundsForDraw(draw, bounds);
    CurrentLayer()->AddDraw(draw);
  };

  if (need_fill) {
    Paint work_paint(paint);
    work_paint.SetStyle(Paint::kFill_Style);
    work_paint.SetAntiAlias(need_contour_aa);
    Path effect_path;
    const Path* dst = &path;

    if (paint.GetPathEffect() && paint.GetPathEffect()->FilterPath(
                                     &effect_path, path, false, work_paint)) {
      dst = &effect_path;
    }

    draw_op_handler(*dst, work_paint, false);
  }

  if (need_stroke) {
    Paint work_paint(paint);
    work_paint.SetStyle(Paint::kStroke_Style);
    work_paint.SetAntiAlias(need_contour_aa);
    Path effect_path;
    Path outline;
    const Path* dst = &path;

    if (paint.GetPathEffect() && paint.GetPathEffect()->FilterPath(
                                     &effect_path, path, true, work_paint)) {
      dst = &effect_path;
    }

    if (work_paint.IsAntiAlias()) {
      // Enable anti-aliasing will convert Stroke into Fill Draw
      work_paint.SetFillColor(work_paint.GetStrokeColor());
      Stroke stroke(work_paint);
      Path quad;
      stroke.QuadPath(*dst, &quad);
      stroke.StrokePath(quad, &outline);
      dst = &outline;
      draw_op_handler(*dst, work_paint, false);
    } else {
      draw_op_handler(*dst, work_paint, true);
    }
  }
}

void HWCanvas::OnSave() {
  SKITY_TRACE_EVENT(HWCanvas_OnSave);

  if (CurrentLayer() == nullptr) {
    return;
  }

  CurrentLayer()->GetState()->Save();
}

void HWCanvas::OnDrawImageRect(std::shared_ptr<Image> image, const Rect& src,
                               const Rect& dst, const SamplingOptions& sampling,
                               Paint const* paint) {
  SKITY_TRACE_EVENT(HWCanvas_OnDrawImageRect);

  if (!image || (image->IsTextureBackend() && !image->IsLazy() &&
                 image->GetTexture() == nullptr)) {
    return;
  }

  if (CurrentLayer() == nullptr) {
    return;
  }

  if (src.Width() == 0 || src.Height() == 0 || dst.Width() == 0 ||
      dst.Height() == 0) {
    return;
  }

  Paint work_paint = (paint == nullptr) ? Paint() : *paint;
  work_paint.SetStyle(Paint::kFill_Style);
  Matrix local_matrix =
      Matrix::Translate(dst.Left(), dst.Top()) *
      Matrix::Scale(dst.Width() / src.Width(), dst.Height() / src.Height()) *
      Matrix::Translate(-src.Left(), -src.Top());
  auto shader = Shader::MakeShader(std::move(image), sampling, TileMode::kDecal,
                                   TileMode::kDecal, local_matrix);
  work_paint.SetShader(std::move(shader));

  Path path;
  path.AddRect(dst);

  OnDrawPath(path, work_paint);
}

void HWCanvas::OnRestore() {
  SKITY_TRACE_EVENT(HWCanvas_OnRestore);

  if (CurrentLayer() == nullptr) {
    return;
  }

  if (layer_stack_.size() > 1 &&
      layer_stack_.back()->GetState()->GetSelfDepth() == 1) {
    layer_stack_.pop_back();
    return;
  }
  CurrentLayer()->Restore();
}

void HWCanvas::OnRestoreToCount(int saveCount) {
  SKITY_TRACE_EVENT(HWCanvas_OnRestoreToCount);

  if (CurrentLayer() == nullptr) {
    return;
  }

  while (layer_stack_.size() > 1) {
    if (saveCount >= layer_stack_.back()->GetState()->GetStartDepth()) {
      break;
    }

    layer_stack_.pop_back();
  }

  // TODO(tt) handle save layer
  CurrentLayer()->RestoreToCount(saveCount);
}

void HWCanvas::OnFlush() {
  SKITY_TRACE_EVENT(HWCanvas_OnFlush);

  if (layer_stack_.empty()) {
    return;
  }
  if (root_layer_->IsValid()) {
    HWRenderTargetCache::Pool pool(
        surface_->GetGPUContext()->GetRenderTargetCache());

    HWDrawContext draw_context;
    draw_context.ctx_scale = ctx_scale_;
    draw_context.stageBuffer = gpu_buffer_;
    draw_context.pipelineLib = pipeline_lib_;
    draw_context.gpuContext = surface_->GetGPUContext();
    draw_context.pool = &pool;
    draw_context.mvp = glm::ortho(
        root_layer_->GetBounds().Left(), root_layer_->GetBounds().Right(),
        root_layer_->GetBounds().Bottom(), root_layer_->GetBounds().Top());
    draw_context.vertex_vector_cache = vertex_vector_cache_.get();
    draw_context.index_vector_cache = index_vector_cache_.get();
    draw_context.total_clip_depth = root_layer_->GetState()->GetDrawDepth() + 1;
    draw_context.arena_allocator = arena_allocator_;

    auto state = root_layer_->Prepare(&draw_context);

    // currently root layer must contain stencil attachment
    root_layer_->GenerateCommand(&draw_context, state);

    UploadMesh();

    root_layer_->Draw(nullptr);
  }

  layer_stack_.clear();
}

uint32_t HWCanvas::GetCanvasSampleCount() {
  if (!enable_msaa_) {
    return 1;
  }

  return surface_->GetSampleCount();
}

void HWCanvas::UploadMesh() {
  SKITY_TRACE_EVENT(HWCanvas_UploadMesh);
  gpu_buffer_->Flush();
}

HWLayer* HWCanvas::CurrentLayer() {
  if (layer_stack_.empty()) {
    return nullptr;
  }

  return layer_stack_.back();
}

HWLayer* HWCanvas::GenLayer(const Paint& paint, Rect layer_bounds,
                            const Matrix& local_to_layer) {
  SKITY_TRACE_EVENT(HWCanvas_GenLayer);
  Rect clip_bounds = CurrentLayer()->GetState()->CurrentClipBounds();
  auto state = CurrentLayer()->GetState();

  Matrix world_matrix = GetCanvasState()->GetTotalMatrix();

  auto layer_matrix = CurrentLayer()->GetLayerPhysicalMatrix(local_to_layer);
  Rect transformed_bounds;
  layer_matrix.MapRect(&transformed_bounds, layer_bounds);
  if (!transformed_bounds.Intersect(clip_bounds)) {
    transformed_bounds.SetEmpty();
  }

  // In some cases, our layer size needs to exceed clip bounds, such as this
  // scene: the drawing content is not on the layer, but the edge blur effect
  // needs to be drawn to the layer. Therefore, if the image filter or mask
  // filter exists, we do not adjust the layer bounds.
  if (paint.GetImageFilter() == nullptr && paint.GetMaskFilter() == nullptr) {
    Matrix layer_matrix_invert{};
    Rect new_layer_bounds;
    layer_matrix.Invert(&layer_matrix_invert);
    layer_matrix_invert.MapRect(&new_layer_bounds, transformed_bounds);
    if (!layer_bounds.Intersect(new_layer_bounds)) {
      layer_bounds.SetEmpty();
    }
  }

  float sx = glm::length(Vec2{world_matrix.Get(Matrix::kMScaleX),
                              world_matrix.Get(Matrix::kMSkewY)});
  float sy = glm::length(Vec2{world_matrix.Get(Matrix::kMSkewX),
                              world_matrix.Get(Matrix::kMScaleY)});
  Vec2 scale{sx * ctx_scale_, sy * ctx_scale_};

  float width_f = layer_bounds.Width() * scale.x;
  float height_f = layer_bounds.Height() * scale.y;

  // check if size is inf or nan
  if (std::isinf(width_f) || std::isnan(width_f) || std::isinf(height_f) ||
      std::isnan(height_f)) {
    return nullptr;
  }

  uint32_t width = std::abs(std::round(width_f));
  uint32_t height = std::abs(std::round(height_f));

  if (width == 0 || height == 0) {
    return nullptr;
  }

  auto max_texture_size =
      surface_->GetGPUContext()->GetGPUDevice()->GetMaxTextureSize();

  if (width > max_texture_size || height > max_texture_size) {
    // will generate GPU validation if the layer size exceeds the maximum
    // texture size
    return nullptr;
  }

  HWSubLayer* layer = nullptr;

  auto hw_filter = HWFilters::ConvertPaintToHWFilter(paint, scale);
  if (hw_filter) {
    layer = arena_allocator_->Make<HWFilterLayer>(
        local_to_layer, state->GetCurrentDepth() + 1, layer_bounds, width,
        height, hw_filter, scale);
  } else {
    layer = arena_allocator_->Make<HWSubLayer>(local_to_layer,
                                               state->GetCurrentDepth() + 1,
                                               layer_bounds, width, height);
  }
  layer->SetArenaAllocator(arena_allocator_);
  layer->SetColorFormat(surface_->GetGPUFormat());
  layer->SetAlpha(paint.GetAlphaF());
  layer->SetBlendMode(paint.GetBlendMode());

  if (surface_->GetGPUContext()->GetGPUDevice()->CanUseMSAA()) {
    layer->SetSampleCount(GetCanvasSampleCount());
  }
  layer->SetWorldMatrix(world_matrix);
  layer->SetLayerSpaceBounds(transformed_bounds);
  layer->SetEnableMergingDrawCall(
      surface_->GetGPUContext()->IsEnableMergingDrawCall());

  return layer;
}

bool HWCanvas::NeesOffScreenLayer(const Paint& paint) const {
  if (paint.GetImageFilter() != nullptr) {
    return true;
  }

  if (paint.GetMaskFilter() != nullptr) {
    return true;
  }

  if (paint.GetColorFilter() != nullptr) {
    // Color Filter can be handled by dynamic generated WGSL shader
    // So don't need offscreen layer
    return false;
  }

  return false;
}

}  // namespace skity
