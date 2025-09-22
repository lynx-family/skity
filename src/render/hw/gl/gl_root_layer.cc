// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/gl/gl_root_layer.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <skity/effect/shader.hpp>
#include <skity/graphic/blend_mode.hpp>

#include "src/geometry/glm_helper.hpp"
#include "src/gpu/gl/gpu_buffer_gl.hpp"
#include "src/gpu/gl/gpu_render_pass_gl.hpp"
#include "src/gpu/gl/gpu_texture_gl.hpp"
#include "src/gpu/gpu_context_impl.hpp"
#include "src/gpu/gpu_render_pass.hpp"
#include "src/render/hw/draw/hw_dynamic_path_draw.hpp"
#include "src/render/hw/hw_render_pass_builder.hpp"

namespace skity {

GLRootLayer::GLRootLayer(uint32_t width, uint32_t height, const Rect &bounds,
                         GLuint vao)
    : HWRootLayer(width, height, bounds, GPUTextureFormat::kRGBA8Unorm),
      vao_(vao) {}

void GLRootLayer::Draw(GPURenderPass *render_pass) {
  BindVAO();

  HWRootLayer::Draw(render_pass);
}

void GLRootLayer::OnPostDraw(GPURenderPass *render_pass,
                             GPUCommandBuffer *cmd) {
  UnBindVAO();
}

void GLRootLayer::BindVAO() {
  GL_CALL(BindVertexArray, vao_);

  GL_CALL(Enable, GL_SCISSOR_TEST);
  GL_CALL(Enable, GL_BLEND);
}

void GLRootLayer::UnBindVAO() {
  GL_CALL(BindVertexArray, 0);

  GL_CALL(BindBuffer, GL_ARRAY_BUFFER, 0);
  GL_CALL(BindBuffer, GL_ELEMENT_ARRAY_BUFFER, 0);
  GL_CALL(BindBuffer, GL_UNIFORM_BUFFER, 0);
  GL_CALL(Disable, GL_SCISSOR_TEST);
  GL_CALL(Disable, GL_BLEND);

  // FIXME: after canvas restore the stencil state may changed and need reset
  // for outside user
  GL_CALL(StencilMask, 0xFF);
  GL_CALL(ColorMask, 1, 1, 1, 1);
}

GLDirectRootLayer::GLDirectRootLayer(uint32_t width, uint32_t height,
                                     const Rect &bounds, GLuint vao, GLuint fbo)
    : GLRootLayer(width, height, bounds, vao), fbo_id_(fbo) {}

std::shared_ptr<GPURenderPass> GLDirectRootLayer::OnBeginRenderPass(
    GPUCommandBuffer *cmd) {
  GPUTextureDescriptor texture_desc{};

  texture_desc.width = GetWidth();
  texture_desc.height = GetHeight();

  auto mock_texture = std::make_shared<GPUTexturePlaceholderGL>(texture_desc);

  mock_texture->SetFramebuffer(fbo_id_, false);

  GPURenderPassDescriptor render_pass_desc{};

  render_pass_desc.color_attachment.texture = mock_texture;
  render_pass_desc.stencil_attachment.texture = mock_texture;
  render_pass_desc.depth_attachment.texture = mock_texture;

  render_pass_desc.color_attachment.load_op =
      NeedClearSurface() ? GPULoadOp::kClear : GPULoadOp::kLoad;
  render_pass_desc.stencil_attachment.load_op = GPULoadOp::kClear;
  render_pass_desc.stencil_attachment.store_op = GPUStoreOp::kDiscard;
  render_pass_desc.stencil_attachment.clear_value = 0;
  render_pass_desc.depth_attachment.load_op = GPULoadOp::kClear;
  render_pass_desc.depth_attachment.store_op = GPUStoreOp::kDiscard;
  render_pass_desc.depth_attachment.clear_value = 0.f;

  return std::make_shared<GPURenderPassGL>(render_pass_desc, fbo_id_);
}

GLExternTextureLayer::GLExternTextureLayer(std::shared_ptr<GPUTexture> texture,
                                           const Rect &bounds, GLuint vao,
                                           int32_t src_fbo)
    : GLRootLayer(texture->GetDescriptor().width,
                  texture->GetDescriptor().height, bounds, vao),
      ext_texture_(std::move(texture)),
      src_fbo_(src_fbo) {}

HWDrawState GLExternTextureLayer::OnPrepare(HWDrawContext *context) {
  auto ret = GLRootLayer::OnPrepare(context);

  HWRenderPassBuilder builder(context, ext_texture_);

  builder.SetSampleCount(GetSampleCount())
      .SetDrawState(GetLayerDrawState())
      .Build(render_pass_desc_);

  return ret;
}

std::shared_ptr<GPURenderPass> GLExternTextureLayer::OnBeginRenderPass(
    GPUCommandBuffer *cmd) {
  auto gpu_render_pass = cmd->BeginRenderPass(render_pass_desc_);

  if (src_fbo_ > 0) {
    auto gpu_render_pass_gl =
        static_cast<GPURenderPassGL *>(gpu_render_pass.get());
    gpu_render_pass_gl->SetAfterCleanupAction([this, gpu_render_pass_gl]() {
      auto read_fbo = src_fbo_;
      auto draw_fbo = gpu_render_pass_gl->GetTargetFBO();
      auto target_width = render_pass_desc_.GetTargetWidth();
      auto target_height = render_pass_desc_.GetTargetHeight();
      auto rect = Rect::MakeLTRB(0, 0, target_width, target_height);
      gpu_render_pass_gl->BlitFramebuffer(read_fbo, draw_fbo, rect, rect,
                                          target_width, target_height);
    });
  }

  return gpu_render_pass;
}

GLDrawTextureLayer::GLDrawTextureLayer(std::shared_ptr<GPUTexture> texture,
                                       GLuint resolve_fbo, const Rect &bounds,
                                       GLuint vao,
                                       bool can_blit_from_target_fbo)
    : GLRootLayer(texture->GetDescriptor().width,
                  texture->GetDescriptor().height, bounds, vao),
      color_texture_(std::move(texture)),
      resolve_fbo_(resolve_fbo),
      can_blit_from_target_fbo_(can_blit_from_target_fbo) {
  SetScissorBox(Rect::MakeWH(color_texture_->GetDescriptor().width,
                             color_texture_->GetDescriptor().height));
}

HWDrawState GLDrawTextureLayer::OnPrepare(HWDrawContext *context) {
  auto ret = GLRootLayer::OnPrepare(context);

  HWRenderPassBuilder builder(context, color_texture_);

  builder.SetSampleCount(GetSampleCount())
      .SetDrawState(GetLayerDrawState())
      .Build(render_pass_desc_);

  // prepare layer back draw
  {
    const auto &bounds = GetBounds();
    Path path;
    path.AddRect(bounds);

    Paint paint;
    paint.SetStyle(Paint::kFill_Style);
    paint.SetShader(
        CreateDrawLayerShader(context->gpuContext, color_texture_, bounds));
    // Since 'can_blit_from_target_fbo_' is a experimental feature, we still
    // use 'kSrcOver' as the default blend mode.
    BlendMode blend_mode =
        can_blit_from_target_fbo_ ? BlendMode::kSrc : BlendMode::kSrcOver;
    paint.SetBlendMode(blend_mode);
    layer_back_draw_ = context->arena_allocator->Make<HWDynamicPathDraw>(
        GetTransform(), std::move(path), std::move(paint), false, false);
  }

  // If layer_back_draw_ is null means user want open WGSL pipeline
  // but the library does not open dynamic shader during compile time
  if (layer_back_draw_) {
    layer_back_draw_->SetSampleCount(GetSampleCount());
    layer_back_draw_->SetColorFormat(GetColorFormat());

    layer_back_draw_->SetScissorBox(GetScissorBox());
    layer_back_draw_->SetClipDepth(context->total_clip_depth);

    layer_back_draw_->Prepare(context);
  }

  return ret;
}

void GLDrawTextureLayer::OnGenerateCommand(HWDrawContext *context,
                                           HWDrawState state) {
  GLRootLayer::OnGenerateCommand(context, state);

  if (layer_back_draw_) {
    layer_back_draw_->GenerateCommand(context, state);
  }
}

std::shared_ptr<GPURenderPass> GLDrawTextureLayer::OnBeginRenderPass(
    GPUCommandBuffer *cmd) {
  auto gpu_render_pass = cmd->BeginRenderPass(render_pass_desc_);
  if (can_blit_from_target_fbo_ && resolve_fbo_ > 0) {
    auto gpu_render_pass_gl =
        static_cast<GPURenderPassGL *>(gpu_render_pass.get());
    gpu_render_pass_gl->SetAfterCleanupAction([this, gpu_render_pass_gl]() {
      auto read_fbo = resolve_fbo_;
      auto draw_fbo = gpu_render_pass_gl->GetTargetFBO();
      auto target_width = render_pass_desc_.GetTargetWidth();
      auto target_height = render_pass_desc_.GetTargetHeight();
      auto rect = Rect::MakeLTRB(0, 0, target_width, target_height);
      gpu_render_pass_gl->BlitFramebuffer(read_fbo, draw_fbo, rect, rect,
                                          target_width, target_height);
    });
  }
  return gpu_render_pass;
}

void GLDrawTextureLayer::OnPostDraw(GPURenderPass *, GPUCommandBuffer *) {
  if (!layer_back_draw_) {
    return;
  }

  GPUTextureDescriptor fake_tex_desc = color_texture_->GetDescriptor();

  auto fake_attachment =
      std::make_shared<GPUTexturePlaceholderGL>(fake_tex_desc);

  fake_attachment->SetFramebuffer(resolve_fbo_, false);

  fake_tex_desc.format = GPUTextureFormat::kDepth24Stencil8;

  auto fake_ds_attachment =
      std::make_shared<GPUTexturePlaceholderGL>(fake_tex_desc);

  GPURenderPassDescriptor fake_desc{};
  fake_desc.color_attachment.load_op =
      NeedClearSurface() ? GPULoadOp::kClear : GPULoadOp::kLoad;
  fake_desc.color_attachment.store_op = GPUStoreOp::kStore;
  fake_desc.color_attachment.clear_value = {0.0, 0.0, 0.0, 0.0};
  fake_desc.color_attachment.texture = fake_attachment;
  fake_desc.stencil_attachment.texture = fake_ds_attachment;
  fake_desc.stencil_attachment.load_op = GPULoadOp::kClear;
  fake_desc.stencil_attachment.store_op = GPUStoreOp::kDiscard;
  fake_desc.stencil_attachment.clear_value = 0;
  fake_desc.depth_attachment.texture = fake_ds_attachment;
  fake_desc.depth_attachment.load_op = GPULoadOp::kClear;
  fake_desc.depth_attachment.store_op = GPUStoreOp::kDiscard;
  fake_desc.depth_attachment.clear_value = 0.f;

  GPURenderPassGL fake_render_pass(fake_desc, resolve_fbo_);

  layer_back_draw_->Draw(&fake_render_pass);

  fake_render_pass.EncodeCommands(
      GetViewport(), GPUScissorRect{0, 0, GetWidth(), GetHeight()});
}

GLPartialDrawTextureLayer::GLPartialDrawTextureLayer(
    std::shared_ptr<GPUTexture> texture, GLuint resolve_fbo,
    const skity::Rect &bounds, GLuint vao)
    : GLDrawTextureLayer(std::move(texture), resolve_fbo, bounds, vao, false) {}

HWDrawState GLPartialDrawTextureLayer::OnPrepare(
    skity::HWDrawContext *context) {
  auto ret = GLRootLayer::OnPrepare(context);

  HWRenderPassBuilder builder(context, color_texture_);

  builder.SetSampleCount(GetSampleCount())
      .SetDrawState(GetLayerDrawState())
      .Build(render_pass_desc_);

  // prepare layer back draw
  {
    auto height = bottom_ - top_;
    auto width = right_ - left_;
    auto bounds = Rect::MakeXYWH(left_, top_, width, height);
    bounds.Offset(dx_, dy_);
    Path path;
    path.AddRect(bounds);

    Paint paint;
    paint.SetStyle(Paint::kFill_Style);
    paint.SetShader(
        CreateDrawLayerShader(context->gpuContext, color_texture_, bounds));
    // Since 'can_blit_from_target_fbo_' is a experimental feature, we still
    // use 'kSrcOver' as the default blend mode.
    BlendMode blend_mode =
        can_blit_from_target_fbo_ ? BlendMode::kSrc : BlendMode::kSrcOver;
    paint.SetBlendMode(blend_mode);
    layer_back_draw_ = context->arena_allocator->Make<HWDynamicPathDraw>(
        GetTransform(), std::move(path), std::move(paint), false, false);
  }

  // If layer_back_draw_ is null means user want open WGSL pipeline
  // but the library does not open dynamic shader during compile time
  if (layer_back_draw_) {
    layer_back_draw_->SetSampleCount(GetSampleCount());
    layer_back_draw_->SetColorFormat(GetColorFormat());

    layer_back_draw_->SetScissorBox(GetScissorBox());
    layer_back_draw_->SetClipDepth(context->total_clip_depth);

    context->mvp = FromGLM(glm::ortho(0.f, static_cast<float>(target_width_),
                                      static_cast<float>(target_height_), 0.f));

    layer_back_draw_->Prepare(context);
  }

  return ret;
}

void GLPartialDrawTextureLayer::OnPostDraw(skity::GPURenderPass *render_pass,
                                           skity::GPUCommandBuffer *cmd) {
  if (!layer_back_draw_) {
    return;
  }

  GPUTextureDescriptor fake_tex_desc = color_texture_->GetDescriptor();
  fake_tex_desc.width = target_width_;
  fake_tex_desc.height = target_height_;

  auto fake_attachment =
      std::make_shared<GPUTexturePlaceholderGL>(fake_tex_desc);

  fake_attachment->SetFramebuffer(resolve_fbo_, false);

  fake_tex_desc.format = GPUTextureFormat::kDepth24Stencil8;

  auto fake_ds_attachment =
      std::make_shared<GPUTexturePlaceholderGL>(fake_tex_desc);

  GPURenderPassDescriptor fake_desc{};
  fake_desc.color_attachment.load_op =
      NeedClearSurface() ? GPULoadOp::kClear : GPULoadOp::kLoad;
  fake_desc.color_attachment.store_op = GPUStoreOp::kStore;
  fake_desc.color_attachment.clear_value = {0.0, 0.0, 0.0, 0.0};
  fake_desc.color_attachment.texture = fake_attachment;
  fake_desc.stencil_attachment.texture = fake_ds_attachment;
  fake_desc.stencil_attachment.load_op = GPULoadOp::kClear;
  fake_desc.stencil_attachment.store_op = GPUStoreOp::kStore;
  fake_desc.stencil_attachment.clear_value = 0;
  fake_desc.depth_attachment.texture = fake_ds_attachment;
  fake_desc.depth_attachment.load_op = GPULoadOp::kClear;
  fake_desc.depth_attachment.store_op = GPUStoreOp::kStore;
  fake_desc.depth_attachment.clear_value = 0.f;

  GPURenderPassGL fake_render_pass(fake_desc, resolve_fbo_);

  layer_back_draw_->Draw(&fake_render_pass);

  GPUViewport viewport{0.f,
                       0.f,
                       static_cast<float>(target_width_),
                       static_cast<float>(target_height_),
                       0.f,
                       1.f};

  GPUScissorRect scissor{
      0,
      0,
      target_width_,
      target_height_,
  };

  fake_render_pass.EncodeCommands(viewport, scissor);
}

}  // namespace skity
