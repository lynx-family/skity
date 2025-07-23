// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gl/gpu_command_buffer_gl.hpp"

#include "src/gpu/gl/gl_interface.hpp"
#include "src/gpu/gl/gpu_render_pass_gl.hpp"
#include "src/gpu/gl/gpu_texture_gl.hpp"

namespace skity {

std::shared_ptr<GPURenderPass> GPUCommandBufferGL::BeginRenderPass(
    const GPURenderPassDescriptor& desc) {
  // this render pass needs msaa resolve
  if (desc.color_attachment.resolve_texture && context_support_msaa_) {
#ifdef SKITY_ANDROID
    if (GLInterface::GlobalInterface()->ext_multisampled_render_to_texture !=
        0) {
      return BeginTileMSAARenderPass(desc);
    } else {
      return BeginMSAAResolveRenderPass(desc);
    }
#else
    return BeginMSAAResolveRenderPass(desc);
#endif
  } else {
    return BeginDirectRenderPass(desc);
  }
}

bool GPUCommandBufferGL::Submit() { return true; }

std::shared_ptr<GPURenderPass> GPUCommandBufferGL::BeginDirectRenderPass(
    const skity::GPURenderPassDescriptor& desc) {
  GLuint fbo_id = 0;

  auto color_texture =
      static_cast<GPUTextureGL*>(desc.color_attachment.texture.get());

  const auto& fbo = color_texture->GetFramebuffer();
  if (fbo) {
    fbo_id = fbo->fbo_id;

    GL_CALL(BindFramebuffer, GL_FRAMEBUFFER, fbo_id);
  } else {
    GL_CALL(GenFramebuffers, 1, &fbo_id);
    color_texture->SetFramebuffer(fbo_id, true);
    GL_CALL(BindFramebuffer, GL_FRAMEBUFFER, fbo_id);
    GL_CALL(FramebufferTexture2D, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, color_texture->GetGLTextureID(), 0);
  }

  // need stencil attachment
  if (desc.stencil_attachment.texture) {
    auto stencil_texture = static_cast<GPUTextureRenderBufferGL*>(
        desc.stencil_attachment.texture.get());

    GL_CALL(FramebufferRenderbuffer, GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER, stencil_texture->GetBufferId());
  } else {
    GL_CALL(FramebufferRenderbuffer, GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER, 0);
  }

  // need depth attachment
  if (desc.depth_attachment.texture) {
    auto depth_texture = static_cast<GPUTextureRenderBufferGL*>(
        desc.depth_attachment.texture.get());

    GL_CALL(FramebufferRenderbuffer, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_RENDERBUFFER, depth_texture->GetBufferId());
  } else {
    GL_CALL(FramebufferRenderbuffer, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_RENDERBUFFER, 0);
  }

  GL_CALL(BindFramebuffer, GL_FRAMEBUFFER, 0);

  return std::make_shared<GPURenderPassGL>(desc, fbo_id);
}

std::shared_ptr<GPURenderPass> GPUCommandBufferGL::BeginMSAAResolveRenderPass(
    const skity::GPURenderPassDescriptor& desc) {
  GLuint render_fbo = 0;
  GLuint resolve_fbo = 0;
  {
    auto color_texture = static_cast<GPUTextureRenderBufferGL*>(
        desc.color_attachment.texture.get());

    const auto& fbo = color_texture->GetFramebuffer();

    if (fbo) {
      render_fbo = fbo->fbo_id;
      GL_CALL(BindFramebuffer, GL_FRAMEBUFFER, render_fbo);
    } else {
      GL_CALL(GenFramebuffers, 1, &render_fbo);

      color_texture->SetFramebuffer(render_fbo, true);

      GL_CALL(BindFramebuffer, GL_FRAMEBUFFER, render_fbo);

      GL_CALL(FramebufferRenderbuffer, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
              GL_RENDERBUFFER, color_texture->GetBufferId());
    }

    if (desc.stencil_attachment.texture) {
      auto stencil_texture = static_cast<GPUTextureRenderBufferGL*>(
          desc.stencil_attachment.texture.get());

      GL_CALL(FramebufferRenderbuffer, GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
              GL_RENDERBUFFER, stencil_texture->GetBufferId());
    } else {
      GL_CALL(FramebufferRenderbuffer, GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
              GL_RENDERBUFFER, 0);
    }

    if (desc.depth_attachment.texture) {
      auto depth_texture = static_cast<GPUTextureRenderBufferGL*>(
          desc.depth_attachment.texture.get());

      GL_CALL(FramebufferRenderbuffer, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
              GL_RENDERBUFFER, depth_texture->GetBufferId());
    } else {
      GL_CALL(FramebufferRenderbuffer, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
              GL_RENDERBUFFER, 0);
    }

    GL_CALL(BindFramebuffer, GL_FRAMEBUFFER, 0);
  }

  {
    auto color_texture =
        static_cast<GPUTextureGL*>(desc.color_attachment.resolve_texture.get());

    const auto& fbo = color_texture->GetFramebuffer();

    if (fbo) {
      resolve_fbo = fbo->fbo_id;
    } else {
      GL_CALL(GenFramebuffers, 1, &resolve_fbo);
      color_texture->SetFramebuffer(resolve_fbo, true);
      GL_CALL(BindFramebuffer, GL_FRAMEBUFFER, resolve_fbo);
      GL_CALL(FramebufferTexture2D, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
              GL_TEXTURE_2D, color_texture->GetGLTextureID(), 0);

      GL_CALL(BindFramebuffer, GL_FRAMEBUFFER, 0);
    }
  }

  return std::make_shared<GLMSAAResolveRenderPass>(desc, render_fbo,
                                                   resolve_fbo);
}

#ifdef SKITY_ANDROID

std::shared_ptr<GPURenderPass> GPUCommandBufferGL::BeginTileMSAARenderPass(
    const GPURenderPassDescriptor& desc) {
  // If running on Android, and the GL_EXT_multisampled_render_to_texture
  // extension is supported all attachment texture is PlaceholderTexture the
  // only valid texture is the color_attachment.resolve_texture

  GLuint fbo_id = 0;

  auto color_texture =
      static_cast<GPUTextureGL*>(desc.color_attachment.resolve_texture.get());

  uint32_t sample_count =
      desc.color_attachment.texture->GetDescriptor().sample_count;

  auto const& fbo = color_texture->GetFramebuffer();

  if (fbo) {
    fbo_id = fbo->fbo_id;

    GL_CALL(BindFramebuffer, GL_FRAMEBUFFER, fbo_id);
  } else {
    GL_CALL(GenFramebuffers, 1, &fbo_id);

    color_texture->SetFramebuffer(fbo_id, true);

    GL_CALL(BindFramebuffer, GL_FRAMEBUFFER, fbo_id);

    GL_CALL(FramebufferTexture2DMultisampleEXT, GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            color_texture->GetGLTextureID(), 0, sample_count);
  }

  if (desc.stencil_attachment.texture) {
    auto stencil_texture = static_cast<GPUTextureRenderBufferGL*>(
        desc.stencil_attachment.texture.get());

    GL_CALL(FramebufferRenderbuffer, GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER, stencil_texture->GetBufferId());
  } else {
    GL_CALL(FramebufferRenderbuffer, GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER, 0);
  }

  if (desc.depth_attachment.texture) {
    auto depth_texture = static_cast<GPUTextureRenderBufferGL*>(
        desc.depth_attachment.texture.get());

    GL_CALL(FramebufferRenderbuffer, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_RENDERBUFFER, depth_texture->GetBufferId());
  } else {
    GL_CALL(FramebufferRenderbuffer, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_RENDERBUFFER, 0);
  }

  GL_CALL(BindFramebuffer, GL_FRAMEBUFFER, 0);

  return std::make_shared<GPURenderPassGL>(desc, fbo_id);
}

#endif

}  // namespace skity
