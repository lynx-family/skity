// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/gpu/gpu_context_gl.hpp>

#include "common/window.hpp"

namespace skity::example::image {

std::shared_ptr<skity::Image> MakeImageGL(
    const std::shared_ptr<skity::Pixmap> &pixmap,
    skity::GPUContext *gpu_context) {
  if (!pixmap) {
    return {};
  }

  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pixmap->Width(), pixmap->Height(), 0,
               GL_RGBA, GL_UNSIGNED_BYTE, pixmap->Addr());

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
