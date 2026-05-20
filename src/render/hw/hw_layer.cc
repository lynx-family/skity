// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_layer.hpp"

#include <algorithm>
#include <cassert>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <skity/effect/shader.hpp>
#include <utility>

#include "src/geometry/glm_helper.hpp"
#include "src/gpu/gpu_blit_pass.hpp"
#include "src/gpu/gpu_context_impl.hpp"
#include "src/gpu/gpu_render_pass.hpp"
#include "src/gpu/gpu_sampler.hpp"
#include "src/gpu/gpu_texture.hpp"
#include "src/gpu/texture_impl.hpp"
#include "src/logging.hpp"
#include "src/render/hw/draw/hw_dynamic_path_draw.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_draw_pass.hpp"
#include "src/render/hw/hw_texture_copy_utils.hpp"
#include "src/tracing.hpp"

namespace skity {

HWLayer::HWLayer(Matrix matrix, int32_t depth, Rect bounds, uint32_t width,
                 uint32_t height)
    : HWDraw(matrix),
      state_(depth),
      bounds_(bounds),
      width_(width),
      height_(height),
      world_matrix_(Matrix{}),
      bounds_to_physical_matrix_(
          Matrix::Scale(width_ / bounds_.Width(), height_ / bounds_.Height()) *
          Matrix::Translate(-bounds_.Left(), -bounds_.Top())) {
  state_.SaveClipBounds(Rect::MakeWH(width_, height_), true);
}

bool HWLayer::CopyRegionToDstTexture(GPUCommandBuffer* cmd,
                                     std::shared_ptr<GPUTexture> src_texture,
                                     std::shared_ptr<GPUTexture> dst_texture,
                                     GPURegion copy_region) const {
  auto blit_pass = cmd->BeginBlitPass();
  GPUBlitPass::TextureCopyRegion region;
  region.src_x = copy_region.x;
  region.src_y = copy_region.y;
  region.dst_x = 0;
  region.dst_y = 0;
  region.width = copy_region.width;
  region.height = copy_region.height;
  blit_pass->CopyTextureToTexture(std::move(src_texture),
                                  std::move(dst_texture), region);
  blit_pass->End();
  return true;
}

void HWLayer::Draw(GPURenderPass* render_pass, GPUCommandBuffer* cmd) {
  SKITY_TRACE_EVENT(HWLayer_Draw);

  bool force_load = false;
  for (auto pass : draw_passes_) {
    auto self_pass = OnBeginRenderPass(cmd, force_load);

    self_pass->SetArenaAllocator(arena_allocator_);
    const HWDraw* emulated_load_draw =
        pass->emulated_load_info ? pass->emulated_load_info->draw : nullptr;
    if (emulated_load_draw) {
      pass->emulated_load_info->draw->Draw(self_pass.get(), cmd);
    }
    for (auto draw : pass->clip_replay_draws) {
      draw->Draw(self_pass.get(), cmd);
    }
    for (auto draw : pass->draw_ops) {
      draw->Draw(self_pass.get(), cmd);
    }

    self_pass->EncodeCommands(GetViewport());

    /**
     * FIXME: avoid crash on VIVO Y77
     *
     * Didn't know why, but it seems that if delete framebuffer before draw to
     * scrren, will avoid crash on VIVO Y77
     */
    self_pass = nullptr;

    if (pass->dst_texture_copy_info) {
      const auto& copy_info = pass->dst_texture_copy_info.value();
      OnCopyToDstTexture(cmd, copy_info.texture, copy_info.copy_region);
    }

    force_load = true;
  }

  OnPostDraw(render_pass, cmd);

  draw_passes_.clear();
}

HWLayerState* HWLayer::GetState() { return &state_; }

void HWLayer::AddDraw(HWDraw* draw) {
  FlushPendingClip();

  draw->SetColorFormat(GetColorFormat());

  const auto& clip_bounds = state_.CurrentClipBounds();
  draw->SetScissorBox(clip_bounds);

  draw->SetClipDraw(state_.LastClipDraw());

  Rect rect = draw->GetLayerSpaceBounds();
  if (!rect.Intersect(Rect::MakeWH(width_, height_))) {
    rect.SetEmpty();
  }
  draw->SetLayerSpaceBounds(rect);

  if (draw->GetDstReadStrategy() == DstReadStrategy::kTextureCopy) {
    auto dst_texture_copy_info = BuildDstTextureCopyInfo(rect);
    if (!dst_texture_copy_info) {
      return;
    }

    auto copy_source_pass = draw_passes_.back();
    auto& copy_info = copy_source_pass->dst_texture_copy_info.emplace(
        std::move(dst_texture_copy_info.value()));
    auto new_draw_pass = arena_allocator_->Make<HWDrawPass>();
    new_draw_pass->dst_read_texture_copy_info = &copy_info;
    new_draw_pass->clip_replay_count = state_.GetRecordedClipCount();
    draw_passes_.push_back(new_draw_pass);
    if (GetSampleCount() > 1) {
      auto load_info = CreateEmulatedLoadInfo();
      HWDraw* load_draw = load_info.draw;
      new_draw_pass->emulated_load_info = std::move(load_info);
      load_draw->SetClipDepth(state_.GetNextDrawDepth());
      // Emulated load must restore the full layer color contents before clip
      // replay runs, otherwise clip-outside pixels remain transparent.
      load_draw->SetScissorBox(Rect::MakeWH(width_, height_));
      load_draw->SetLayerSpaceBounds(Rect::MakeSize(Vec2{width_, height_}));
    }
  } else {
    if (enable_merging_draw_call_) {
      bool merged = TryMerge(draw);
      if (merged) {
        return;
      }
    }
  }

  draw->SetClipDepth(state_.GetNextDrawDepth());
  auto* current_pass = draw_passes_.back();
  if (current_pass->first_draw_depth == 0) {
    current_pass->first_draw_depth = draw->GetClipDepth();
  }
  current_pass->draw_ops.emplace_back(draw);
}

bool HWLayer::TryMerge(HWDraw* draw) {
  auto& draw_ops = draw_passes_.back()->draw_ops;
  size_t max_count = std::min(draw_ops.size(), size_t(5));

  for (auto it = draw_ops.rbegin(); it != draw_ops.rbegin() + max_count; it++) {
    auto cadidate = *it;
    bool merged = cadidate->MergeIfPossible(draw);
    if (merged) {
      return true;
    }

    if (Rect::Intersect(cadidate->GetLayerSpaceBounds(),
                        draw->GetLayerSpaceBounds())) {
      break;
    }
  }

  return false;
}

std::optional<DstTextureCopyInfo> HWLayer::BuildDstTextureCopyInfo(
    const Rect& layer_space_bounds) const {
  return hw_texture_copy_utils::BuildDstTextureCopyInfo(
      layer_space_bounds, width_, height_,
      rt_origin_ == LayerRTOrigin::kBottomLeft);
}

Vec4 HWLayer::BuildDstUVMapping(const Rect& copy_rect) const {
  return hw_texture_copy_utils::BuildDstUVMapping(
      copy_rect, height_, rt_origin_ == LayerRTOrigin::kBottomLeft);
}

void HWLayer::AddClip(HWDraw* draw) {
  const auto& clip_bounds = state_.CurrentClipBounds();

  draw->SetScissorBox(clip_bounds);
  draw->SetColorFormat(GetColorFormat());
  pending_clip_.emplace_back(draw);
  state_.SaveClipOp(draw);
}

void HWLayer::AddRectClip(const skity::Rect& local_rect, const Matrix& matrix) {
  Rect transformed_rect;
  GetLayerPhysicalMatrix(matrix).MapRect(&transformed_rect, local_rect);
  state_.SaveClipBounds(transformed_rect);
}

void HWLayer::Restore() { state_.Restore(); }

void HWLayer::RestoreToCount(int32_t count) { state_.RestoreToCount(count); }

void HWLayer::FlushPendingClip() {
  draw_passes_.back()->draw_ops.insert(draw_passes_.back()->draw_ops.end(),
                                       pending_clip_.begin(),
                                       pending_clip_.end());

  pending_clip_.clear();
}

HWDrawState HWLayer::OnPrepare(HWDrawContext* context) {
  state_.FlushClipDepth();

  gpu_device_ = context->gpuContext->GetGPUDevice();

  HWRenderTargetCache::Pool pool(context->gpuContext->GetRenderTargetCache());

  HWDrawContext sub_context;
  sub_context.ctx_scale = context->ctx_scale;
  sub_context.stageBuffer = context->stageBuffer;
  sub_context.static_buffer = context->static_buffer;
  sub_context.pipelineLib = context->pipelineLib;
  sub_context.gpuContext = context->gpuContext;
  sub_context.mvp = FromGLM(glm::ortho(bounds_.Left(), bounds_.Right(),
                                       bounds_.Bottom(), bounds_.Top()));
  sub_context.pool = &pool;
  sub_context.vertex_vector_cache = context->vertex_vector_cache;
  sub_context.index_vector_cache = context->index_vector_cache;
  sub_context.total_clip_depth = state_.GetDrawDepth() + 1;
  sub_context.arena_allocator = context->arena_allocator;
  sub_context.scale = scale_;

  for (auto pass : draw_passes_) {
    CollectClipReplayDraws(pass);

    if (pass->emulated_load_info) {
      auto resolve_color_texture = GetResolveColorTexture();
      DEBUG_CHECK(resolve_color_texture != nullptr);
      pass->emulated_load_info->resolve_image->SetTexture(
          std::make_shared<InternalTexture>(resolve_color_texture,
                                            AlphaType::kPremul_AlphaType));
    }

    const HWDraw* emulated_load_draw =
        pass->emulated_load_info ? pass->emulated_load_info->draw : nullptr;
    if (emulated_load_draw) {
      layer_state_ |= pass->emulated_load_info->draw->Prepare(&sub_context);
    }
    for (auto draw : pass->draw_ops) {
      layer_state_ |= draw->Prepare(&sub_context);
    }

    if (pass->dst_texture_copy_info) {
      auto& copy_info = pass->dst_texture_copy_info.value();
      GPUTextureDescriptor desc;
      desc.width = copy_info.copy_region.width;
      desc.height = copy_info.copy_region.height;
      desc.format = GetColorFormat();
      desc.sample_count = 1;
      desc.usage =
          static_cast<GPUTextureUsageMask>(GPUTextureUsage::kCopyDst) |
          static_cast<GPUTextureUsageMask>(GPUTextureUsage::kTextureBinding);
      desc.storage_mode = GPUTextureStorageMode::kPrivate;
      copy_info.texture = gpu_device_->CreateTexture(desc);
      GPUSamplerDescriptor sampler_desc;
      copy_info.sampler = gpu_device_->CreateSampler(sampler_desc);
    }
  }

  // abstract layer no need stencil test and depth for itself
  return HWDrawState::kDrawStateNone;
}

void HWLayer::OnGenerateCommand(HWDrawContext* context, HWDrawState state) {
  HWRenderTargetCache::Pool pool(context->gpuContext->GetRenderTargetCache());

  HWDrawContext sub_context;
  sub_context.ctx_scale = context->ctx_scale;
  sub_context.stageBuffer = context->stageBuffer;
  sub_context.static_buffer = context->static_buffer;
  sub_context.pipelineLib = context->pipelineLib;
  sub_context.gpuContext = context->gpuContext;
  sub_context.mvp = FromGLM(glm::ortho(bounds_.Left(), bounds_.Right(),
                                       bounds_.Bottom(), bounds_.Top()));
  sub_context.pool = &pool;
  sub_context.vertex_vector_cache = context->vertex_vector_cache;
  sub_context.index_vector_cache = context->index_vector_cache;
  sub_context.total_clip_depth = state_.GetDrawDepth() + 1;
  sub_context.arena_allocator = context->arena_allocator;
  sub_context.scale = scale_;

  for (auto pass : draw_passes_) {
    const HWDraw* emulated_load_draw =
        pass->emulated_load_info ? pass->emulated_load_info->draw : nullptr;
    if (emulated_load_draw) {
      sub_context.dst_read_texture_copy_info = nullptr;
      pass->emulated_load_info->draw->GenerateCommand(&sub_context,
                                                      layer_state_);
    }
    for (auto draw : pass->draw_ops) {
      sub_context.dst_read_texture_copy_info =
          draw->GetDstReadStrategy() == DstReadStrategy::kTextureCopy
              ? pass->dst_read_texture_copy_info
              : nullptr;
      draw->GenerateCommand(&sub_context, layer_state_);
    }
  }
  sub_context.dst_read_texture_copy_info = nullptr;
}

void HWLayer::CollectClipReplayDraws(HWDrawPass* pass) {
  if (pass->clip_replay_count == 0) {
    return;
  }

  const auto& replay_records = state_.GetClipReplayRecords();
  DEBUG_CHECK(replay_records.size() >= pass->clip_replay_count);

  pass->clip_replay_draws.reserve(pass->clip_replay_count);
  for (size_t i = 0; i < pass->clip_replay_count; ++i) {
    auto* source_draw = replay_records[i].source_draw;
    DEBUG_CHECK(source_draw != nullptr);

    // A clip restored before this split pass gets a smaller depth than any
    // real draw recorded into the new pass. Replaying it is unnecessary
    // because it cannot affect later draws, so we can skip it here.
    if (pass->first_draw_depth != 0 &&
        source_draw->GetClipDepth() < pass->first_draw_depth) {
      continue;
    }

    pass->clip_replay_draws.push_back(source_draw);
  }
}

Matrix HWLayer::GetLayerPhysicalMatrix(const Matrix& matrix) const {
  return bounds_to_physical_matrix_ * matrix;
}

Rect HWLayer::CalculateLayerSpaceBounds(const Rect& local_rect,
                                        const Matrix& matrix) const {
  Rect layer_space_bounds;
  GetLayerPhysicalMatrix(matrix).MapRect(&layer_space_bounds, local_rect);
  return layer_space_bounds;
}

std::shared_ptr<Shader> HWLayer::CreateDrawLayerShader(
    GPUContext* gpu_context, std::shared_ptr<GPUTexture> gpu_texture,
    const Rect& bounds) const {
  (void)gpu_context;
  auto texture = std::make_shared<InternalTexture>(
      gpu_texture, AlphaType::kPremul_AlphaType);

  auto image = Image::MakeHWImage(texture);
  return CreateDrawLayerShader(image, bounds);
}

std::shared_ptr<Shader> HWLayer::CreateDrawLayerShader(
    std::shared_ptr<Image> image, const Rect& bounds) const {
  Matrix local_matrix;
  if (rt_origin_ == LayerRTOrigin::kBottomLeft) {
    local_matrix =
        Matrix::Translate(bounds.Left(), bounds.Height() + bounds.Top()) *
        Matrix::Scale(bounds.Width() / image->Width(),
                      -(bounds.Height() / image->Height()));
  } else {
    local_matrix = Matrix::Translate(bounds.Left(), bounds.Top()) *
                   Matrix::Scale(bounds.Width() / image->Width(),
                                 bounds.Height() / image->Height());
  }

  return Shader::MakeShader(image, SamplingOptions{}, TileMode::kDecal,
                            TileMode::kDecal, local_matrix);
}

EmulatedLoadInfo HWLayer::CreateEmulatedLoadInfo() {
  // prepare layer back draw
  const auto& bounds = GetBounds();
  Path path;
  path.AddRect(bounds);

  Paint paint;
  paint.SetStyle(Paint::kFill_Style);
  std::shared_ptr<DeferredTextureImage> image =
      DeferredTextureImage::MakeDeferredTextureImage(
          FromGPUTextureFormat(GetColorFormat()), width_, height_,
          AlphaType::kPremul_AlphaType);
  paint.SetShader(CreateDrawLayerShader(image, bounds));
  paint.SetBlendMode(BlendMode::kSrc);
  auto draw = arena_allocator_->Make<HWDynamicPathDraw>(
      GetTransform(), std::move(path), std::move(paint), false, false);

  draw->SetSampleCount(GetSampleCount());
  draw->SetColorFormat(GetColorFormat());

  EmulatedLoadInfo load_info;
  load_info.draw = draw;
  load_info.resolve_image = image;
  return load_info;
}

}  // namespace skity
