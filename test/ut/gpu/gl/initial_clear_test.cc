// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <OpenGL/gl3.h>
#include <gtest/gtest.h>

#include <array>
#include <skity/gpu/gpu_context_gl.hpp>
#include <skity/render/canvas.hpp>

#include "test/bench/common/bench_gl_context_mac.h"

namespace skity {
namespace {

TEST(InitialColorClearDesktopGL, FullRectClearsWithoutRasterizingSamples) {
  auto native = CreateBenchGLContextMac();
  if (!native->MakeCurrent()) {
    GTEST_SKIP() << "Requires a native desktop GL context";
  }
  auto context = GLContextCreate(GetGLProcLoader());
  ASSERT_NE(context, nullptr);

  GLuint fbo;
  GLuint buffers[2];
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glGenRenderbuffers(2, buffers);
  glBindRenderbuffer(GL_RENDERBUFFER, buffers[0]);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 8, 8);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_RENDERBUFFER, buffers[0]);
  glBindRenderbuffer(GL_RENDERBUFFER, buffers[1]);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 8, 8);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                            GL_RENDERBUFFER, buffers[1]);
  ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

  GPUSurfaceDescriptorGL desc;
  desc.backend = GPUBackendType::kOpenGL;
  desc.width = desc.height = 8;
  desc.gl_id = fbo;
  desc.surface_type = GLSurfaceType::kFramebuffer;
  desc.surface_mode = GLSurfaceMode::kDirect;
  desc.has_stencil_attachment = true;
  auto surface = context->CreateSurface(&desc);
  ASSERT_NE(surface, nullptr);
  auto* canvas = surface->LockCanvas(false);
  Paint paint;
  paint.SetColor(Color_RED);
  GLuint query;
  glGenQueries(1, &query);
  glBeginQuery(GL_SAMPLES_PASSED, query);
  canvas->DrawRect(Rect::MakeWH(8, 8), paint);
  canvas->Flush();
  surface->Flush();
  glEndQuery(GL_SAMPLES_PASSED);

  GLuint samples;
  glGetQueryObjectuiv(query, GL_QUERY_RESULT, &samples);
  EXPECT_EQ(samples, 0u);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  std::array<GLubyte, 4> pixel;
  glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
  EXPECT_EQ(pixel, (std::array<GLubyte, 4>{255, 0, 0, 255}));
  surface.reset();
  context.reset();
  glDeleteQueries(1, &query);
  glDeleteRenderbuffers(2, buffers);
  glDeleteFramebuffers(1, &fbo);
  native->ClearCurrent();
}

}  // namespace
}  // namespace skity
