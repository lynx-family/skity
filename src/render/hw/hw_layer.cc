// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_layer.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <skity/effect/shader.hpp>

#include "src/geometry/glm_helper.hpp"
#include "src/gpu/gpu_context_impl.hpp"
#include "src/logging.hpp"
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

void HWLayer::Draw(GPURenderPass* render_pass) {
  SKITY_TRACE_EVENT(HWLayer_Draw);
  auto cmd = CreateCommandBuffer();
  if (!cmd) {
    LOGE("Failed to create command buffer");
    return;
  }

  auto self_pass = OnBeginRenderPass(cmd.get());
  if (!self_pass) {
    LOGE("OnBeginRenderPass() returned null");
    return;
  }

  self_pass->SetArenaAllocator(arena_allocator_);
  for (auto draw : draw_ops_) {
    draw->Draw(self_pass.get());
  }

  self_pass->EncodeCommands(GetViewport());
  cmd->Submit();

  /**
   * FIXME: avoid crash on VIVO Y77
   *
   * Didn't know why, but it seems that if delete framebuffer before draw to
   * scrren, will avoid crash on VIVO Y77
   */
  self_pass = nullptr;

  OnPostDraw(render_pass, cmd.get());

  draw_ops_.clear();
}

HWLayerState* HWLayer::GetState() { return &state_; }

void HWLayer::AddDraw(HWDraw* draw) {
  FlushPendingClip();

  draw->SetColorFormat(GetColorFormat());

  const auto& clip_bounds = state_.CurrentClipBounds();
  draw->SetScissorBox(clip_bounds);

  draw->SetClipDraw(state_.LastClipDraw());
  draw->SetClipDepth(state_.GetNextDrawDepth());

  Rect rect = draw->GetLayerSpaceBounds();
  if (!rect.Intersect(Rect::MakeWH(width_, height_))) {
    rect.SetEmpty();
  }
  draw->SetLayerSpaceBounds(rect);

  if (enable_merging_draw_call_) {
    bool merged = TryMerge(draw);
    if (merged) {
      return;
    }
  }

  draw_ops_.emplace_back(draw);
}

bool HWLayer::TryMerge(HWDraw* draw) {
  size_t max_count = std::min(draw_ops_.size(), size_t(5));

  for (auto it = draw_ops_.rbegin(); it != draw_ops_.rbegin() + max_count;
       it++) {
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

std::shared_ptr<GPUCommandBuffer> HWLayer::CreateCommandBuffer() {
  return gpu_device_->CreateCommandBuffer();
}

void HWLayer::FlushPendingClip() {
  draw_ops_.insert(draw_ops_.end(), pending_clip_.begin(), pending_clip_.end());

  pending_clip_.clear();
}

HWDrawState HWLayer::OnPrepare(HWDrawContext* context) {
  state_.FlushClipDepth();

  gpu_device_ = context->gpuContext->GetGPUDevice();

  HWRenderTargetCache::Pool pool(context->gpuContext->GetRenderTargetCache());

  HWDrawContext sub_context;
  sub_context.ctx_scale = context->ctx_scale;
  sub_context.stageBuffer = context->stageBuffer;
  sub_context.pipelineLib = context->pipelineLib;
  sub_context.gpuContext = context->gpuContext;
  // For Vulkan backend, adjust coordinate system to account for Y-axis
  // differences
  if (context->gpuContext &&
      context->gpuContext->GetBackendType() == GPUBackendType::kVulkan) {
    sub_context.mvp = FromGLM(glm::ortho(bounds_.Left(), bounds_.Right(),
                                         bounds_.Top(), bounds_.Bottom()));
  } else {
    sub_context.mvp = FromGLM(glm::ortho(bounds_.Left(), bounds_.Right(),
                                         bounds_.Bottom(), bounds_.Top()));
  }
  sub_context.pool = &pool;
  sub_context.vertex_vector_cache = context->vertex_vector_cache;
  sub_context.index_vector_cache = context->index_vector_cache;
  sub_context.total_clip_depth = state_.GetDrawDepth() + 1;
  sub_context.arena_allocator = context->arena_allocator;
  sub_context.scale = scale_;

  // if one draw needs stencil we need create a stencil attachment
  for (auto draw : draw_ops_) {
    layer_state_ |= draw->Prepare(&sub_context);
  }

  // abstract layer no need stencil test and depth for itself
  return HWDrawState::kDrawStateNone;
}

void HWLayer::OnGenerateCommand(HWDrawContext* context, HWDrawState state) {
  HWRenderTargetCache::Pool pool(context->gpuContext->GetRenderTargetCache());

  HWDrawContext sub_context;
  sub_context.ctx_scale = context->ctx_scale;
  sub_context.stageBuffer = context->stageBuffer;
  sub_context.pipelineLib = context->pipelineLib;
  sub_context.gpuContext = context->gpuContext;

  // Vulkan has Y-axis pointing down, while OpenGL has Y-axis pointing up
  // Use flipped parameters for Vulkan
  if (context->gpuContext &&
      context->gpuContext->GetBackendType() == GPUBackendType::kVulkan) {
    sub_context.mvp = FromGLM(glm::ortho(bounds_.Left(), bounds_.Right(),
                                         bounds_.Top(), bounds_.Bottom()));
  } else {
    sub_context.mvp = FromGLM(glm::ortho(bounds_.Left(), bounds_.Right(),
                                         bounds_.Bottom(), bounds_.Top()));
  }

  sub_context.pool = &pool;
  sub_context.vertex_vector_cache = context->vertex_vector_cache;
  sub_context.index_vector_cache = context->index_vector_cache;
  sub_context.total_clip_depth = state_.GetDrawDepth() + 1;
  sub_context.arena_allocator = context->arena_allocator;
  sub_context.scale = scale_;

  for (auto draw : draw_ops_) {
    draw->GenerateCommand(&sub_context, layer_state_);
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
  auto texture = std::make_shared<InternalTexture>(
      gpu_texture, AlphaType::kPremul_AlphaType);

  auto image = Image::MakeHWImage(texture);

  Matrix local_matrix;
  // FIXME: GL/GLES fbo texture need to flip the Y coordinate when drawing
  // back to screen
  if (gpu_context->GetBackendType() == GPUBackendType::kOpenGL ||
      gpu_context->GetBackendType() == GPUBackendType::kWebGL2) {
    local_matrix =
        Matrix::Translate(bounds.Left(), bounds.Height() + bounds.Top()) *
        Matrix::Scale(bounds.Width() / texture->Width(),
                      -(bounds.Height() / texture->Height()));
  } else {
    local_matrix = Matrix::Translate(bounds.Left(), bounds.Top()) *
                   Matrix::Scale(bounds.Width() / texture->Width(),
                                 bounds.Height() / texture->Height());
  }

  return Shader::MakeShader(image, SamplingOptions{}, TileMode::kClamp,
                            TileMode::kClamp, local_matrix);
}

}  // namespace skity
