// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gl/gpu_surface_gl.hpp"

#include "src/gpu/gl/gl_interface.hpp"
#include "src/gpu/gl/gpu_buffer_gl.hpp"
#include "src/gpu/gl/gpu_texture_gl.hpp"
#include "src/logging.hpp"
#include "src/render/hw/gl/gl_root_layer.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"

namespace skity {

GPUSurfaceGL::GPUSurfaceGL(const GPUSurfaceDescriptor &desc,
                           GPUContextImpl *ctx)
    : GPUSurfaceImpl(desc, ctx) {}

GPUSurfaceGL::~GPUSurfaceGL() {
  if (vao_) {
    GL_CALL(DeleteVertexArrays, 1, &vao_);
  }

  LOGI("GPUSurfaceGL: [ {:p} ] destroyed", reinterpret_cast<void *>(this));
}

void GPUSurfaceGL::Init() { GL_CALL(GenVertexArrays, 1, &vao_); }

std::shared_ptr<Pixmap> GPUSurfaceGL::ReadPixels(const Rect &rect) {
  return nullptr;
}

DirectSurfaceGL::DirectSurfaceGL(const GPUSurfaceDescriptor &desc,
                                 GPUContextImpl *ctx, uint32_t fbo_id,
                                 bool need_free)
    : GPUSurfaceGL(desc, ctx),
      target_fbo_id_(fbo_id),
      need_free_fbo_(need_free) {}

DirectSurfaceGL::~DirectSurfaceGL() {
  if (need_free_fbo_ && target_fbo_id_) {
    GL_CALL(DeleteFramebuffers, 1, &target_fbo_id_);
  }
}

HWRootLayer *DirectSurfaceGL::OnBeginNextFrame(bool clear) {
  auto root_layer = GetArenaAllocator()->Make<GLDirectRootLayer>(
      static_cast<uint32_t>(std::floor(GetWidth() * ContentScale())),
      static_cast<uint32_t>(std::floor(GetHeight() * ContentScale())),
      Rect::MakeWH(GetWidth(), GetHeight()), GetVertexArray(), target_fbo_id_);

  root_layer->SetClearSurface(clear);
  root_layer->SetArenaAllocator(GetArenaAllocator());

  return root_layer;
}

TextureSurfaceGL::TextureSurfaceGL(const GPUSurfaceDescriptor &desc,
                                   GPUContextImpl *ctx,
                                   std::shared_ptr<GPUTexture> texture)
    : GPUSurfaceGL(desc, ctx), ext_texture_(std::move(texture)) {}

HWRootLayer *TextureSurfaceGL::OnBeginNextFrame(bool clear) {
  auto root_layer = GetArenaAllocator()->Make<GLExternTextureLayer>(
      ext_texture_, Rect::MakeWH(GetWidth(), GetHeight()), GetVertexArray());

  root_layer->SetClearSurface(clear);
  root_layer->SetSampleCount(GetSampleCount());
  root_layer->SetArenaAllocator(GetArenaAllocator());

  return root_layer;
}

HWRootLayer *DrawTextureSurfaceGL::OnBeginNextFrame(bool clear) {
  auto root_layer = GetArenaAllocator()->Make<GLDrawTextureLayer>(
      color_attachment_, resolve_fbo_, Rect::MakeWH(GetWidth(), GetHeight()),
      GetVertexArray(), can_blit_from_target_fbo_);

  root_layer->SetClearSurface(clear);
  root_layer->SetSampleCount(GetSampleCount());
  root_layer->SetArenaAllocator(GetArenaAllocator());

  return root_layer;
}

HWRootLayer *PartialFBOSurfaceGL::OnBeginNextFrame(bool clear) {
  auto root_layer = GetArenaAllocator()->Make<GLPartialDrawTextureLayer>(
      color_attachment_, resolve_fbo_, Rect::MakeWH(GetWidth(), GetHeight()),
      GetVertexArray());

  root_layer->SetClearSurface(clear);
  root_layer->SetSampleCount(GetSampleCount());
  root_layer->SetArenaAllocator(GetArenaAllocator());

  root_layer->SetFrameInfo(frame_info_.width, frame_info_.height,
                           frame_info_.left, frame_info_.top, frame_info_.right,
                           frame_info_.bottom);
  root_layer->UpdateTranslate(translate_x_, translate_y_);

  return root_layer;
}

BlitSurfaceGL::BlitSurfaceGL(const GPUSurfaceDescriptor &desc,
                             GPUContextImpl *ctx, uint32_t resolve_fbo,
                             bool can_blit_from_target_fbo)
    : GPUSurfaceGL(desc, ctx),
      resolve_fbo_(resolve_fbo),
      can_blit_from_target_fbo_(can_blit_from_target_fbo) {}

void BlitSurfaceGL::Init() {
  GPUSurfaceGL::Init();

  GPUTextureDescriptor desc{};
  desc.format = GPUTextureFormat::kRGBA8Unorm;
  desc.width = static_cast<uint32_t>(std::floor(GetWidth() * ContentScale()));
  desc.height = static_cast<uint32_t>(std::floor(GetHeight() * ContentScale()));
  desc.usage =
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kRenderAttachment);
  desc.storage_mode = GPUTextureStorageMode::kPrivate;

  texture_ = GetGPUContext()->GetGPUDevice()->CreateTexture(desc);
}

void BlitSurfaceGL::OnFlush() {
  const auto &render_fbo =
      static_cast<GPUTextureGL *>(texture_.get())->GetFramebuffer();

  // If the texture is been used as a render pass target, the fbo must not be
  // null, otherwise there is a problem inside the GL implementation.
  if (!render_fbo) {
    return;
  }

  GL_CALL(BindFramebuffer, GL_READ_FRAMEBUFFER, render_fbo->fbo_id);
  GL_CALL(BindFramebuffer, GL_DRAW_FRAMEBUFFER, resolve_fbo_);

  auto target_width = texture_->GetDescriptor().width;
  auto target_height = texture_->GetDescriptor().height;

  GL_CALL(BlitFramebuffer, 0, 0, target_width, target_height, 0, 0,
          target_width, target_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

  GL_CALL(BindFramebuffer, GL_FRAMEBUFFER, 0);
}

HWRootLayer *BlitSurfaceGL::OnBeginNextFrame(bool clear) {
  auto root_layer = GetArenaAllocator()->Make<GLExternTextureLayer>(
      texture_, Rect::MakeWH(GetWidth(), GetHeight()), GetVertexArray(),
      can_blit_from_target_fbo_ ? static_cast<int32_t>(resolve_fbo_) : -1);

  root_layer->SetClearSurface(clear);
  root_layer->SetSampleCount(GetSampleCount());
  root_layer->SetArenaAllocator(GetArenaAllocator());

  return root_layer;
}

}  // namespace skity
