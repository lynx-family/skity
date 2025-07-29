// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <functional>
#include <mutex>
#include <skity/gpu/gpu_context_gl.hpp>

#include "common/window.hpp"

PFNGLGENTEXTURESPROC f_glGenTextures = nullptr;
PFNGLBINDTEXTUREPROC f_glBindTexture = nullptr;
PFNGLTEXPARAMETERIPROC f_glTexParameteri = nullptr;
PFNGLTEXIMAGE2DPROC f_glTexImage2D = nullptr;
PFNGLGENFRAMEBUFFERSPROC f_glGenFramebuffers = nullptr;
PFNGLBINDFRAMEBUFFERPROC f_glBindFramebuffer = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC f_glFramebufferTexture2D = nullptr;
PFNGLFINISHPROC f_glFinish = nullptr;

namespace skity::example::lock_canvas {

std::shared_ptr<skity::Image> DrawOffscreenGL(
    skity::GPUContext *context, int width, int height,
    std::function<void(skity::GPUSurface *surface)> &&func) {
  static std::once_flag _init_flag;

  std::call_once(_init_flag, []() {
    f_glGenTextures = (PFNGLGENTEXTURESPROC)glfwGetProcAddress("glGenTextures");
    f_glBindTexture = (PFNGLBINDTEXTUREPROC)glfwGetProcAddress("glBindTexture");
    f_glTexParameteri =
        (PFNGLTEXPARAMETERIPROC)glfwGetProcAddress("glTexParameteri");
    f_glTexImage2D = (PFNGLTEXIMAGE2DPROC)glfwGetProcAddress("glTexImage2D");
    f_glGenFramebuffers =
        (PFNGLGENFRAMEBUFFERSPROC)glfwGetProcAddress("glGenFramebuffers");
    f_glBindFramebuffer =
        (PFNGLBINDFRAMEBUFFERPROC)glfwGetProcAddress("glBindFramebuffer");
    f_glFramebufferTexture2D =
        (PFNGLFRAMEBUFFERTEXTURE2DPROC)glfwGetProcAddress(
            "glFramebufferTexture2D");
    f_glFinish = (PFNGLFINISHPROC)glfwGetProcAddress("glFinish");
  });

  GLuint tex = 0;

  f_glGenTextures(1, &tex);
  f_glBindTexture(GL_TEXTURE_2D, tex);
  f_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);

  f_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  f_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  GLuint fbo = 0;
  f_glGenFramebuffers(1, &fbo);
  f_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  f_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           tex, 0);
  f_glBindFramebuffer(GL_FRAMEBUFFER, 0);

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

  f_glBindFramebuffer(GL_FRAMEBUFFER, 0);

  f_glFinish();

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
