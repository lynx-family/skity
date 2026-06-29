// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <memory>
#include <skity/effect/color_filter.hpp>
#include <skity/effect/shader.hpp>
#include <skity/geometry/rrect.hpp>
#include <skity/graphic/color.hpp>
#include <skity/graphic/image.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>
#include <skity/io/pixmap.hpp>
#include <skity/render/canvas.hpp>
#include <skity/render/precompile_context.hpp>
#include <utility>
#include <vector>

#include "src/effect/pixmap_shader.hpp"
#include "src/gpu/gpu_context_impl.hpp"
#include "src/gpu/gpu_sampler.hpp"
#include "src/graphic/blend_mode_priv.hpp"
#include "src/logging.hpp"
#include "src/render/hw/draw/fragment/wgsl_gradient_fragment.hpp"
#include "src/render/hw/draw/fragment/wgsl_solid_color.hpp"
#include "src/render/hw/draw/fragment/wgsl_solid_vertex_color.hpp"
#include "src/render/hw/draw/fragment/wgsl_stencil_fragment.hpp"
#include "src/render/hw/draw/fragment/wgsl_text_fragment.hpp"
#include "src/render/hw/draw/fragment/wgsl_texture_fragment.hpp"
#include "src/render/hw/draw/geometry/wgsl_clip_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_path_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_rrect_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_tess_path_fill_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_tess_path_stroke_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_text_geometry.hpp"
#include "src/render/hw/draw/hw_draw_step.hpp"
#include "src/render/hw/draw/step/clip_step.hpp"
#include "src/render/hw/draw/step/color_step.hpp"
#include "src/render/hw/draw/step/stencil_step.hpp"
#include "src/render/hw/draw/wgx_filter.hpp"
#include "src/render/hw/draw/wgx_programmable_blending.hpp"
#include "src/render/hw/dst_read_strategy.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/tracing.hpp"
#include "src/utils/arena_allocator.hpp"
#include "src/utils/batch_group.hpp"
#include "src/utils/vector_cache.hpp"

namespace skity {
namespace {

constexpr Rect kPrecompileBounds = {0.f, 0.f, 32.f, 32.f};

Path MakePrecompilePath() {
  Path path;
  path.MoveTo(0.f, 0.f);
  path.LineTo(16.f, 0.f);
  path.LineTo(0.f, 16.f);
  path.LineTo(16.f, 16.f);
  path.Close();
  return path;
}

RRect MakePrecompileRRect() {
  return RRect::MakeRectXY(kPrecompileBounds, 8.f, 8.f);
}

Paint MakeTwoColorLinearGradientPaint(const float* pos) {
  Paint paint;
  paint.SetStyle(Paint::kFill_Style);

  Point pts[2] = {{0.f, 0.f, 0.f, 1.f}, {32.f, 32.f, 0.f, 1.f}};
  Vec4 colors[2] = {Colors::kRed, Colors::kBlue};
  paint.SetShader(Shader::MakeLinear(pts, colors, pos, 2));
  return paint;
}

GPUTextureFormat ResolvePrecompileColorFormat(PrecompileColorType color_type) {
  switch (color_type) {
    case PrecompileColorType::kRGBA:
      return GPUTextureFormat::kRGBA8Unorm;
    case PrecompileColorType::kBGRA:
      return GPUTextureFormat::kBGRA8Unorm;
  }
  return GPUTextureFormat::kInvalid;
}

bool PrecompileStep(HWDrawStep* step, HWDrawStepContext* ctx,
                    BlendMode blend_mode) {
  DEBUG_CHECK(step != nullptr);
  DEBUG_CHECK(ctx != nullptr);

  auto success =
      step->PrecompilePipeline(ctx->context, ctx->state, ctx->color_format,
                               ctx->sample_count, blend_mode);
  if (!success) {
    LOGE("Precompile shader failed: failed to create render pipeline");
  }

  ctx->context->pipelineLib->ResetCompileFailedPipelines();
  return success;
}

HWWGSLFragment* ApplyPaintEffects(HWWGSLFragment* fragment, const Paint& paint,
                                  GPUContextImpl* gpu_context) {
  if (paint.GetColorFilter() != nullptr) {
    fragment->SetFilter(WGXFilterFragment::Make(paint.GetColorFilter().get()));
  }

  if (IsAdvancedBlendMode(paint.GetBlendMode())) {
    const auto& caps = gpu_context->GetGPUDevice()->GetCaps();
    auto strategy = ResolveDstReadStrategy(paint.GetBlendMode(), caps);
    if (strategy == DstReadStrategy::kNativeBlend) {
      if (caps.native_blend_shader_variant) {
        fragment->SetUsesNativeAdvancedBlend(true);
      }
    } else {
      fragment->SetProgrammableBlending(
          WGXProgrammableBlending::Make(paint.GetBlendMode(), strategy));
    }
  }

  return fragment;
}

HWWGSLFragment* MakeColorFragment(HWDrawStepContext* ctx, const Paint& paint,
                                  bool has_color = true) {
  auto* arena = ctx->context->arena_allocator;
  HWWGSLFragment* fragment = nullptr;
  if (paint.GetShader() != nullptr) {
    Shader::GradientInfo info = {};
    auto type = paint.GetShader()->AsGradient(&info);
    if (type != Shader::GradientType::kNone) {
      fragment = arena->Make<WGSLGradientFragment>(
          info, type, paint.GetAlphaF(), paint.GetShader()->GetLocalMatrix());
      return ApplyPaintEffects(fragment, paint, ctx->context->gpuContext);
    }
  }

  if (!has_color) {
    fragment = arena->Make<WGSLSolidVertexColor>();
  } else {
    fragment = arena->Make<WGSLSolidColor>(paint.GetFillColor());
  }

  return ApplyPaintEffects(fragment, paint, ctx->context->gpuContext);
}

HWWGSLFragment* MakeTextureFragment(HWDrawStepContext* ctx, const Paint& paint,
                                    std::shared_ptr<PixmapShader> shader,
                                    const Matrix& matrix) {
  auto* fragment = ctx->context->arena_allocator->Make<WGSLTextureFragment>(
      std::move(shader), nullptr, nullptr, 1.f, matrix, 1.f, 1.f);
  return ApplyPaintEffects(fragment, paint, ctx->context->gpuContext);
}

void PrecompilePathStep(HWDrawStepContext* ctx, const Paint& paint,
                        bool is_stroke) {
  auto* arena = ctx->context->arena_allocator;
  auto path = MakePrecompilePath();
  auto coverage = is_stroke ? CoverageType::kNoZero : CoverageType::kWinding;

  if (paint.IsAntiAlias()) {
    auto* geometry = arena->Make<WGSLPathAAGeometry>(path, paint);
    auto* fragment = MakeColorFragment(ctx, paint);
    auto* aa_step = arena->Make<ColorAAStep>(geometry, fragment, coverage);
    PrecompileStep(aa_step, ctx, paint.GetBlendMode());
  } else if (!is_stroke) {
    auto* geometry = arena->Make<WGSLPathGeometry>(path, paint, false);
    auto* fragment = MakeColorFragment(ctx, paint);
    auto* color_step =
        arena->Make<ColorStep>(geometry, fragment, CoverageType::kNone);
    // Single pass fill geometry is precompiled as fill geometry.
    PrecompileStep(color_step, ctx, paint.GetBlendMode());
  }

  auto* stencil_step = arena->Make<StencilStep>(
      arena->Make<WGSLPathGeometry>(path, paint, is_stroke),
      arena->Make<WGSLStencilFragment>(), is_stroke);
  PrecompileStep(stencil_step, ctx, BlendMode::kDefault);
  auto* geometry = arena->Make<WGSLPathGeometry>(path, paint, is_stroke);
  auto* fragment = MakeColorFragment(ctx, paint);
  auto* color_step = arena->Make<ColorStep>(geometry, fragment, coverage);
  PrecompileStep(color_step, ctx, paint.GetBlendMode());
}

void PrecompileTessPathStep(HWDrawStepContext* ctx, const Paint& paint,
                            bool is_stroke) {
  auto* arena = ctx->context->arena_allocator;
  auto path = MakePrecompilePath();
  HWWGSLGeometry* geometry = nullptr;
  if (is_stroke) {
    geometry = arena->Make<WGSLTessPathStrokeGeometry>(path, paint);
  } else {
    geometry = arena->Make<WGSLTessPathFillGeometry>(path, paint);
  }
  auto* fragment = MakeColorFragment(ctx, paint);
  auto coverage = is_stroke ? CoverageType::kNoZero : CoverageType::kWinding;
  auto* stencil_step = arena->Make<StencilStep>(
      geometry, arena->Make<WGSLStencilFragment>(), is_stroke);
  auto* color_step = arena->Make<ColorStep>(geometry, fragment, coverage);
  if (!is_stroke && !paint.IsAntiAlias()) {
    auto* single_pass_step =
        arena->Make<ColorStep>(geometry, fragment, CoverageType::kNone);
    // Single pass fill geometry is precompiled as fill geometry.
    PrecompileStep(single_pass_step, ctx, paint.GetBlendMode());
  }
  PrecompileStep(stencil_step, ctx, BlendMode::kDefault);
  PrecompileStep(color_step, ctx, paint.GetBlendMode());
}

void PrecompileRRectStep(HWDrawStepContext* ctx, const Paint& paint) {
  auto* arena = ctx->context->arena_allocator;
  std::vector<BatchGroup<RRect>> batch_group;
  batch_group.emplace_back(BatchGroup<RRect>{
      MakePrecompileRRect(),
      paint,
      Matrix{},
  });

  auto* geometry = arena->Make<WGSLRRectGeometry>(batch_group);
  auto* fragment = MakeColorFragment(ctx, paint, false);
  auto* step = arena->Make<ColorStep>(geometry, fragment, CoverageType::kNone);
  PrecompileStep(step, ctx, paint.GetBlendMode());
}

void PrecompileImageStep(HWDrawStepContext* ctx, const Paint& paint,
                         bool use_rrect = false) {
  auto* arena = ctx->context->arena_allocator;
  auto path = MakePrecompilePath();
  auto matrix = Matrix{};
  auto work_paint = paint;
  work_paint.SetStyle(Paint::kFill_Style);

  auto pixmap = std::make_shared<Pixmap>(1, 1, AlphaType::kPremul_AlphaType,
                                         ColorType::kRGBA);
  auto image = Image::MakeImage(std::move(pixmap));
  auto shader = std::make_shared<PixmapShader>(
      std::move(image), SamplingOptions{}, TileMode::kDecal, TileMode::kDecal,
      matrix);

  HWWGSLGeometry* geometry = nullptr;
  if (use_rrect) {
    std::vector<BatchGroup<RRect>> batch_group;
    batch_group.emplace_back(BatchGroup<RRect>{
        MakePrecompileRRect(),
        work_paint,
        matrix,
    });
    geometry = arena->Make<WGSLRRectGeometry>(batch_group);
  } else {
    geometry = arena->Make<WGSLPathGeometry>(path, work_paint, false);
  }

  auto* fragment =
      MakeTextureFragment(ctx, work_paint, std::move(shader), matrix);
  auto* step = arena->Make<ColorStep>(geometry, fragment, CoverageType::kNone);

  PrecompileStep(step, ctx, work_paint.GetBlendMode());
}

ArrayList<GlyphRect, 16> MakePrecompileGlyphRects(ArenaAllocator* arena) {
  ArrayList<GlyphRect, 16> glyph_rects;
  glyph_rects.SetArenaAllocator(arena);
  glyph_rects.emplace_back(Vec4{0.f, 0.f, 16.f, 16.f}, Vec2{0.f, 0.f},
                           Vec2{1.f, 1.f});
  return glyph_rects;
}

HWWGSLFragment* MakeTextFragment(HWDrawStepContext* ctx, const Paint& paint,
                                 WGSLTextFragment::BatchedTexture textures,
                                 std::shared_ptr<GPUSampler> sampler, bool sdf,
                                 bool emoji, bool swizzle_rb) {
  auto* arena = ctx->context->arena_allocator;
  HWWGSLFragment* fragment = nullptr;

  if (emoji) {
    fragment = arena->Make<WGSLColorEmojiFragment>(
        std::move(textures), std::move(sampler), swizzle_rb, paint.GetAlphaF());
  } else if (paint.GetShader() != nullptr && !sdf &&
             paint.GetShader()->AsGradient(nullptr) !=
                 Shader::GradientType::kNone) {
    Shader::GradientInfo info = {};
    auto type = paint.GetShader()->AsGradient(&info);
    fragment = arena->Make<WGSLGradientTextFragment>(
        std::move(textures), std::move(sampler), info, type, paint.GetAlphaF());
  } else if (sdf) {
    fragment = arena->Make<WGSLSdfColorTextFragment>(
        std::move(textures), std::move(sampler), paint.GetFillColor());
  } else {
    fragment = arena->Make<WGSLColorTextFragment>(std::move(textures),
                                                  std::move(sampler));
  }

  return ApplyPaintEffects(fragment, paint, ctx->context->gpuContext);
}

void PrecompileTextStep(HWDrawStepContext* ctx, const Paint& paint, bool sdf,
                        bool emoji, bool swizzle_rb = false) {
  auto* arena = ctx->context->arena_allocator;
  auto glyph_rects = MakePrecompileGlyphRects(arena);
  WGSLTextFragment::BatchedTexture textures = {};
  std::shared_ptr<GPUSampler> sampler;

  HWWGSLGeometry* geometry = nullptr;
  if (paint.GetShader() != nullptr && !sdf &&
      paint.GetShader()->AsGradient(nullptr) != Shader::GradientType::kNone) {
    geometry = arena->Make<WGSLTextGradientGeometry>(
        Matrix{}, std::move(glyph_rects), paint.GetShader()->GetLocalMatrix(),
        Matrix{});
  } else {
    geometry = arena->Make<WGSLTextSolidColorGeometry>(
        Matrix{}, std::move(glyph_rects), paint);
  }

  auto* fragment = MakeTextFragment(ctx, paint, std::move(textures),
                                    std::move(sampler), sdf, emoji, swizzle_rb);
  auto* step = arena->Make<ColorStep>(geometry, fragment, CoverageType::kNone);
  PrecompileStep(step, ctx, paint.GetBlendMode());
}

}  // namespace

class PrecompileContextImpl {
 public:
  PrecompileContextImpl(GPUContextImpl* gpu_context,
                        GPUTextureFormat color_format, uint32_t sample_count);

  void PrecompileDefaultShaders();

  void PrecompileDraw(PrecompileDrawType draw_type, const Paint& paint);

 private:
  void Init();

  GPUContextImpl* gpu_context_ = nullptr;
  bool valid_ = false;
  GPUTextureFormat color_format_ = GPUTextureFormat::kInvalid;
  uint32_t sample_count_ = 1;
  std::shared_ptr<BlockCacheAllocator> block_cache_allocator_ =
      std::make_shared<BlockCacheAllocator>();
  ArenaAllocator arena_{block_cache_allocator_};
  VectorCache<float> vertex_vector_cache_;
  VectorCache<uint32_t> index_vector_cache_;
  HWDrawContext draw_context_ = {};
  HWDrawStepContext step_context_ = {};
};

PrecompileContextImpl::PrecompileContextImpl(GPUContextImpl* gpu_context,
                                             GPUTextureFormat color_format,
                                             uint32_t sample_count)
    : gpu_context_(gpu_context),
      color_format_(color_format),
      sample_count_(sample_count) {
  Init();
}

void PrecompileContextImpl::Init() {
  if (gpu_context_ == nullptr || gpu_context_->GetPipelineLib() == nullptr) {
    return;
  }

  if (color_format_ == GPUTextureFormat::kInvalid) {
    return;
  }

  draw_context_.pipelineLib = gpu_context_->GetPipelineLib();
  draw_context_.gpuContext = gpu_context_;
  draw_context_.vertex_vector_cache = &vertex_vector_cache_;
  draw_context_.index_vector_cache = &index_vector_cache_;
  draw_context_.arena_allocator = &arena_;
  draw_context_.scale = {1.f, 1.f};
  draw_context_.ctx_scale = 1.f;

  step_context_.context = &draw_context_;
  step_context_.state = kDrawStateDepth | kDrawStateStencil;
  step_context_.color_format = color_format_;
  step_context_.sample_count = sample_count_;
  valid_ = true;
}

void PrecompileContextImpl::PrecompileDefaultShaders() {
  if (!valid_) {
    return;
  }

  Paint solid_paint;
  solid_paint.SetStyle(Paint::kFill_Style);
  Paint stroke_paint;
  stroke_paint.SetStyle(Paint::kStroke_Style);

  PrecompileDraw(PrecompileDrawType::kDrawPath, solid_paint);
  PrecompileDraw(PrecompileDrawType::kDrawPath, stroke_paint);
  PrecompileDraw(PrecompileDrawType::kDrawRRect, solid_paint);
  PrecompileDraw(PrecompileDrawType::kDrawText, solid_paint);
  PrecompileDraw(PrecompileDrawType::kDrawImage, solid_paint);
  PrecompileDraw(PrecompileDrawType::kDrawImageRRect, solid_paint);

  PrecompileDraw(PrecompileDrawType::kDrawPath,
                 MakeTwoColorLinearGradientPaint(nullptr));
  float color_fast_pos[2] = {0.f, 1.f};
  PrecompileDraw(PrecompileDrawType::kDrawPath,
                 MakeTwoColorLinearGradientPaint(color_fast_pos));
  float pos[2] = {0.25f, 0.75f};
  PrecompileDraw(PrecompileDrawType::kDrawPath,
                 MakeTwoColorLinearGradientPaint(pos));
}

void PrecompileContextImpl::PrecompileDraw(PrecompileDrawType draw_type,
                                           const Paint& paint) {
  if (!valid_) {
    return;
  }

  auto work_paint = paint;
  if (gpu_context_->IsEnableSimpleShapePipeline() &&
      (draw_type == PrecompileDrawType::kDrawRRect ||
       (draw_type == PrecompileDrawType::kDrawRect &&
        work_paint.GetStyle() == Paint::kStroke_Style))) {
    PrecompileRRectStep(&step_context_, work_paint);
    return;
  }

  if (draw_type == PrecompileDrawType::kDrawText ||
      draw_type == PrecompileDrawType::kDrawSDFText ||
      draw_type == PrecompileDrawType::kDrawEmojiText) {
    bool is_emoji = draw_type == PrecompileDrawType::kDrawEmojiText;
    bool is_sdf = draw_type == PrecompileDrawType::kDrawSDFText;
    if (is_emoji) {
      PrecompileTextStep(&step_context_, work_paint, false, true, false);
      PrecompileTextStep(&step_context_, work_paint, false, true, true);
    } else {
      PrecompileTextStep(&step_context_, work_paint, is_sdf, false, false);
    }
    return;
  }

  if (draw_type == PrecompileDrawType::kDrawImage ||
      draw_type == PrecompileDrawType::kDrawImageRRect) {
    PrecompileImageStep(&step_context_, work_paint,
                        draw_type == PrecompileDrawType::kDrawImageRRect);
    return;
  }

  if (draw_type == PrecompileDrawType::kClipPath) {
    auto path = MakePrecompilePath();
    auto* step = arena_.Make<ClipStep>(
        arena_.Make<WGSLClipGeometry>(path, work_paint, false,
                                      Canvas::ClipOp::kIntersect),
        arena_.Make<WGSLStencilFragment>(), path.GetFillType(),
        Canvas::ClipOp::kIntersect);
    PrecompileStep(step, &step_context_, BlendMode::kDefault);
    return;
  }

  if (gpu_context_->IsEnableGPUTessellation() &&
      (sample_count_ > 1 || !gpu_context_->IsEnableContourAA() ||
       !work_paint.IsAntiAlias())) {
    if (work_paint.GetStyle() != Paint::kStroke_Style) {
      Paint fill_paint(work_paint);
      fill_paint.SetStyle(Paint::kFill_Style);
      fill_paint.SetAntiAlias(false);
      PrecompileTessPathStep(&step_context_, fill_paint, false);
    }

    if (work_paint.GetStyle() != Paint::kFill_Style) {
      Paint stroke_paint(work_paint);
      stroke_paint.SetStyle(Paint::kStroke_Style);
      stroke_paint.SetAntiAlias(false);
      PrecompileTessPathStep(&step_context_, stroke_paint, true);
    }
    return;
  }

  bool need_contour_aa = sample_count_ == 1 &&
                         gpu_context_->IsEnableContourAA() &&
                         work_paint.IsAntiAlias();

  if (need_contour_aa) {
    work_paint.SetAntiAlias(true);
    // Contour AA stroke is precompiled as fill geometry.
    PrecompilePathStep(&step_context_, work_paint, false);
    return;
  }

  work_paint.SetAntiAlias(false);
  if (work_paint.GetStyle() != Paint::kStroke_Style) {
    PrecompilePathStep(&step_context_, work_paint, false);
  }
  if (work_paint.GetStyle() != Paint::kFill_Style) {
    PrecompilePathStep(&step_context_, work_paint, true);
  }
}

PrecompileContext::PrecompileContext(GPUContext* gpu_context,
                                     PrecompileColorType color_type,
                                     bool enable_msaa) {
  auto gpu_context_impl = static_cast<GPUContextImpl*>(gpu_context);

  auto color_format = ResolvePrecompileColorFormat(color_type);
  if (color_format == GPUTextureFormat::kInvalid) {
    LOGE("Precompile shader failed: precompile color type {} is not supported",
         static_cast<int>(color_type));
  }
  if (color_format != GPUTextureFormat::kInvalid) {
    gpu_context_impl->SetForceDepthStencilPipelineState(true);
  }
  impl_ = std::make_unique<PrecompileContextImpl>(
      gpu_context_impl, color_format, enable_msaa ? 4u : 1u);
}

PrecompileContext::~PrecompileContext() = default;

void PrecompileContext::PrecompileDefaultShaders() const {
  SKITY_TRACE_EVENT(PrecompileContext_PrecompileDefaultShaders);
  impl_->PrecompileDefaultShaders();
}

void PrecompileContext::PrecompileDraw(PrecompileDrawType draw_type,
                                       const Paint& paint) const {
  SKITY_TRACE_EVENT(PrecompileContext_PrecompileDraw);
  impl_->PrecompileDraw(draw_type, paint);
}

}  // namespace skity
