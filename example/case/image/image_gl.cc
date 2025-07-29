// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <functional>
#include <mutex>
#include <skity/gpu/gpu_context_gl.hpp>

#include "common/window.hpp"

namespace skity::example::image {

PFNGLGENTEXTURESPROC f_glGenTextures = nullptr;
PFNGLBINDTEXTUREPROC f_glBindTexture = nullptr;
PFNGLTEXPARAMETERIPROC f_glTexParameteri = nullptr;
PFNGLTEXIMAGE2DPROC f_glTexImage2D = nullptr;

std::shared_ptr<skity::Image> MakeImageGL(
    const std::shared_ptr<skity::Pixmap> &pixmap,
    skity::GPUContext *gpu_context) {
  if (!pixmap) {
    return {};
  }

  static std::once_flag _init_flag;

  std::call_once(_init_flag, []() {
    f_glGenTextures = (PFNGLGENTEXTURESPROC)glfwGetProcAddress("glGenTextures");
    f_glBindTexture = (PFNGLBINDTEXTUREPROC)glfwGetProcAddress("glBindTexture");
    f_glTexParameteri =
        (PFNGLTEXPARAMETERIPROC)glfwGetProcAddress("glTexParameteri");
    f_glTexImage2D = (PFNGLTEXIMAGE2DPROC)glfwGetProcAddress("glTexImage2D");
  });

  GLuint tex;
  f_glGenTextures(1, &tex);
  f_glBindTexture(GL_TEXTURE_2D, tex);
  f_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  f_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  f_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  f_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  f_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pixmap->Width(), pixmap->Height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, pixmap->Addr());

  skity::GPUBackendTextureInfoGL desc{};
  desc.backend = skity::GPUBackendType::kOpenGL;
  desc.format = skity::TextureFormat::kRGBA;
  desc.width = pixmap->Width();
  desc.height = pixmap->Height();
  desc.alpha_type = pixmap->GetAlphaType();
  desc.tex_id = tex;
  desc.owned_by_engine = true;

  auto texture = gpu_context->WrapTexture(&desc);

  return skity::Image::MakeHWImage(texture);
}

}  // namespace skity::example::image
