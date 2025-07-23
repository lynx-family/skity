// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <functional>
#include <skity/gpu/gpu_context_gl.hpp>

#include "common/window.hpp"

namespace skity::example::lock_canvas {

std::shared_ptr<skity::Image> DrawOffscreenGL(
    skity::GPUContext *context, int width, int height,
    std::function<void(skity::GPUSurface *surface)> &&func) {
  GLuint tex = 0;

  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  GLuint fbo = 0;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         tex, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  skity::GPUSurfaceDescriptorGL desc{};
  desc.backend = skity::GPUBackendType::kOpenGL;
  desc.sample_count = 1;
  desc.width = width;
  desc.height = height;
  desc.content_scale = 1;
  desc.surface_type = skity::GLSurfaceType::kFramebuffer;
  desc.gl_id = fbo;
  desc.has_stencil_attachment = false;
  desc.can_blit_from_target_fbo = true;  // must set this to true

  auto surface = context->CreateSurface(&desc);

  if (surface == nullptr) {
    return {};
  }

  func(surface.get());

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  glFinish();

  skity::GPUBackendTextureInfoGL tex_info{};
  tex_info.backend = skity::GPUBackendType::kOpenGL;
  tex_info.format = skity::TextureFormat::kRGBA;
  tex_info.width = width;
  tex_info.height = height;
  tex_info.alpha_type = skity::AlphaType::kPremul_AlphaType;
  tex_info.tex_id = tex;
  tex_info.owned_by_engine = true;

  auto texture = context->WrapTexture(&tex_info);

  if (texture == nullptr) {
    return {};
  }

  return skity::Image::MakeHWImage(texture);
}

}  // namespace skity::example::lock_canvas
