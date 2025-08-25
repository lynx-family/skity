// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "test/bench/common/bench_target_gl.hpp"

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>

#include <cassert>
#include <skity/gpu/gpu_context_gl.hpp>

namespace skity {

std::shared_ptr<BenchTarget> BenchTargetGL::Create(skity::GPUContext *context,
                                                   Options options) {
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, options.width, options.height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

  skity::GPUSurfaceDescriptorGL desc;
  desc.backend = skity::GPUBackendType::kOpenGL;
  desc.width = options.width;
  desc.height = options.height;
  desc.gl_id = texture;
  desc.surface_type = skity::GLSurfaceType::kTexture;
  desc.has_stencil_attachment = false;
  desc.sample_count = options.aa == AAType::kMSAA ? 4 : 1;
  auto surface = context->CreateSurface(&desc);
  assert(surface.get() != nullptr);
  return std::make_shared<BenchTargetGL>(context, std::move(surface), options,
                                         texture);
}

BenchTargetGL::BenchTargetGL(skity::GPUContext *context,
                             std::unique_ptr<skity::GPUSurface> surface,
                             Options options, uint32_t texture)
    : BenchTarget(context, std::move(surface), options), texture_(texture) {}

BenchTargetGL::~BenchTargetGL() {
  if (texture_) {
    glDeleteTextures(1, &texture_);
  }
  texture_ = 0;
}

}  // namespace skity
