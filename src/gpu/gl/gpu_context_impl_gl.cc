// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gl/gpu_context_impl_gl.hpp"

#include <skity/gpu/gpu_context_gl.hpp>
#include <skity/io/data.hpp>

#include "src/gpu/gl/formats_gl.h"
#include "src/gpu/gl/gl_interface.hpp"
#include "src/gpu/gl/gpu_device_gl.hpp"
#include "src/gpu/gl/gpu_surface_gl.hpp"
#include "src/gpu/gl/gpu_texture_gl.hpp"

namespace skity {

std::unique_ptr<GPUContext> GLContextCreate(void* proc_loader) {
  GLInterface::InitGlobalInterface(proc_loader);

  auto ctx = std::make_unique<GPUContextImplGL>();

  ctx->Init();

  return ctx;
}

GPUContextImplGL::GPUContextImplGL()
    : GPUContextImpl(GPUBackendType::kOpenGL) {}

std::unique_ptr<GPUSurface> GPUContextImplGL::CreateSurface(
    GPUSurfaceDescriptor* desc) {
  if (desc->backend != GPUBackendType::kOpenGL) {
    return std::unique_ptr<GPUSurface>();
  }

  auto gl_desc = static_cast<GPUSurfaceDescriptorGL*>(desc);

  if (gl_desc->surface_type == GLSurfaceType::kTexture) {
    return CreateTextureSurface(*desc, gl_desc->gl_id);
  }

  if (!gl_desc->has_stencil_attachment || gl_desc->sample_count > 1) {
    bool can_blit_from_target_fbo =
        gl_desc->sample_count == 1 ? gl_desc->can_blit_from_target_fbo : false;
#ifdef SKITY_ANDROID
    // Blit to the default framebuffer has issues on many devices
    // So All android device use draw to flush the final image
    // If GL_EXT_multisampled_render_to_texture is present will use
    // special GPUSurface implementation
    return CreateDrawTextureSurface(*desc, gl_desc->gl_id,
                                    can_blit_from_target_fbo);
#else
    return CreateBlitSurface(*desc, gl_desc->gl_id, can_blit_from_target_fbo);
#endif
  }

  return CreateDirectSurface(*desc, gl_desc->gl_id, false);
}

std::unique_ptr<GPUSurface> GPUContextImplGL::CreateFxaaSurface(
    GPUSurfaceDescriptor* desc) {
  return std::unique_ptr<GPUSurface>();
}

std::unique_ptr<GPURenderTarget> GPUContextImplGL::OnCreateRenderTarget(
    const GPURenderTargetDescriptor& desc, std::shared_ptr<Texture> texture) {
  auto gl_texture = static_cast<GPUTextureGL*>(texture->GetGPUTexture().get());

  GPUSurfaceDescriptorGL surface_desc{};
  surface_desc.backend = GetBackendType();
  surface_desc.width = desc.width;
  surface_desc.height = desc.height;
  surface_desc.content_scale = 1.0;
  surface_desc.sample_count = desc.sample_count;
  surface_desc.surface_type = GLSurfaceType::kTexture;
  surface_desc.gl_id = gl_texture->GetGLTextureID();

  auto surface = CreateTextureSurface(surface_desc, surface_desc.gl_id);

  return std::make_unique<GPURenderTarget>(std::move(surface), texture);
}

std::shared_ptr<Data> GPUContextImplGL::OnReadPixels(
    const std::shared_ptr<GPUTexture>& texture) const {
  auto gl_texture = static_cast<GPUTextureGL*>(texture.get());

  const auto& fbo = gl_texture->GetFramebuffer();
  if (fbo) {
    GL_CALL(BindFramebuffer, GL_FRAMEBUFFER, fbo->fbo_id);
  } else {
    GLuint fbo_id = 0;
    GL_CALL(GenFramebuffers, 1, &fbo_id);
    if (fbo_id == 0) {
      return nullptr;
    }

    gl_texture->SetFramebuffer(fbo_id, true);

    GL_CALL(BindFramebuffer, GL_FRAMEBUFFER, fbo_id);
    GL_CALL(FramebufferTexture2D, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, gl_texture->GetGLTextureID(), 0);
  }

  auto& desc = texture->GetDescriptor();

  void* pixels = malloc(texture->GetBytes());
  auto data = Data::MakeFromMalloc(pixels, texture->GetBytes());
  GL_CALL(PixelStorei, GL_PACK_ROW_LENGTH, desc.width);
  GL_CALL(PixelStorei, GL_PACK_ALIGNMENT, 1);
  GL_CALL(ReadPixels, 0, 0, desc.width, desc.height,
          ExternalFormatFrom(desc.format), ExternalTypeFrom(desc.format),
          pixels);
  GL_CALL(BindFramebuffer, GL_FRAMEBUFFER, 0);

  return data;
}

std::unique_ptr<GPUDevice> GPUContextImplGL::CreateGPUDevice() {
  return std::make_unique<GPUDeviceGL>();
}

std::shared_ptr<GPUTexture> GPUContextImplGL::OnWrapTexture(
    GPUBackendTextureInfo* info, ReleaseCallback callback,
    ReleaseUserData user_data) {
  if (info->backend != GPUBackendType::kOpenGL) {
    return nullptr;
  }

  auto gl_info = static_cast<GPUBackendTextureInfoGL*>(info);

  GPUTextureDescriptor desc;
  desc.width = gl_info->width;
  desc.height = gl_info->height;
  desc.format = static_cast<GPUTextureFormat>(gl_info->format);
  desc.usage =
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kTextureBinding);
  desc.storage_mode = GPUTextureStorageMode::kHostVisible;

  return GPUExternalTextureGL::Make(
      desc, gl_info->tex_id, gl_info->owned_by_engine, callback, user_data);
}

std::unique_ptr<GPUSurface> GPUContextImplGL::CreateDirectSurface(
    const GPUSurfaceDescriptor& desc, uint32_t fbo_id, bool need_free) {
  auto surface =
      std::make_unique<DirectSurfaceGL>(desc, this, fbo_id, need_free);

  surface->Init();

  return surface;
}

std::unique_ptr<GPUSurface> GPUContextImplGL::CreateBlitSurface(
    const GPUSurfaceDescriptor& desc, uint32_t fbo_id,
    bool can_blit_from_target_fbo) {
  auto surface = std::make_unique<BlitSurfaceGL>(desc, this, fbo_id,
                                                 can_blit_from_target_fbo);

  surface->Init();

  return surface;
}

std::unique_ptr<GPUSurface> GPUContextImplGL::CreateTextureSurface(
    const GPUSurfaceDescriptor& desc, uint32_t tex_id) {
  GPUTextureDescriptor tex_desc{};
  tex_desc.width = desc.width * desc.content_scale;
  tex_desc.height = desc.height * desc.content_scale;
  tex_desc.format = GPUTextureFormat::kRGBA8Unorm;
  tex_desc.storage_mode = GPUTextureStorageMode::kPrivate;
  tex_desc.usage =
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kRenderAttachment);

  auto texture =
      GPUExternalTextureGL::Make(tex_desc, tex_id, /*owned_by_engine=*/false);

  auto surface =
      std::make_unique<TextureSurfaceGL>(desc, this, std::move(texture));

  surface->Init();

  return surface;
}

#ifdef SKITY_ANDROID

std::unique_ptr<GPUSurface> GPUContextImplGL::CreateDrawTextureSurface(
    const GPUSurfaceDescriptor& desc, uint32_t fbo_id,
    bool can_blit_from_target_fbo) {
  GPUTextureDescriptor tex_desc{};
  tex_desc.width = desc.width * desc.content_scale;
  tex_desc.height = desc.height * desc.content_scale;
  tex_desc.format = GPUTextureFormat::kRGBA8Unorm;
  tex_desc.storage_mode = GPUTextureStorageMode::kPrivate;
  tex_desc.usage =
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kRenderAttachment);

  auto color_attachment = GetGPUDevice()->CreateTexture(tex_desc);

  auto surface = std::make_unique<DrawTextureSurfaceGL>(
      desc, this, std::move(color_attachment), fbo_id,
      can_blit_from_target_fbo);

  surface->Init();

  return surface;
}

#endif

std::unique_ptr<GPUSurface> GLCreatePartialSurface(
    GPUContext* context, const GPUSurfaceDescriptorGL& desc,
    const PartialFrameInfo& frame_info) {
  if (context->GetBackendType() != GPUBackendType::kOpenGL) {
    return {};
  }

  if (desc.surface_type != GLSurfaceType::kFramebuffer) {
    return {};
  }

  auto context_gl = static_cast<GPUContextImplGL*>(context);

  GPUTextureDescriptor tex_desc{};
  tex_desc.width = desc.width * desc.content_scale;
  tex_desc.height = desc.height * desc.content_scale;
  tex_desc.format = GPUTextureFormat::kRGBA8Unorm;
  tex_desc.storage_mode = GPUTextureStorageMode::kPrivate;
  tex_desc.usage =
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kRenderAttachment);

  auto color_attachment = context_gl->GetGPUDevice()->CreateTexture(tex_desc);

  auto surface = std::make_unique<PartialFBOSurfaceGL>(
      desc, context_gl, std::move(color_attachment), desc.gl_id);

  surface->SetFrameInfo(frame_info);

  surface->Init();

  return surface;
}

void GLUpdateSurfaceTranslate(GPUSurface* surface, float dx, float dy) {
  auto p_surface = static_cast<PartialFBOSurfaceGL*>(surface);

  if (p_surface == nullptr) {
    return;
  }

  p_surface->UpdateTranslate(dx, dy);
}

}  // namespace skity
