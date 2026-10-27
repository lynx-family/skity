// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <EGL/egl.h>
#include <gtest/gtest.h>

#include <array>
#include <skity/geometry/matrix.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>
#include <skity/render/canvas.hpp>

#include "common/golden_test_env.hpp"
#include "src/gpu/gl/gl_interface.hpp"

namespace skity::testing {
namespace {

PFNGLDRAWELEMENTSPROC g_draw_elements = nullptr;
PFNGLDRAWELEMENTSINSTANCEDPROC g_draw_instanced = nullptr;
uint32_t g_draw_count = 0;

class ScopedDrawCounter {
 public:
  explicit ScopedDrawCounter(GLInterface* gl) : gl_(gl) {
    g_draw_elements = gl_->fDrawElements;
    g_draw_instanced = gl_->fDrawElementsInstanced;
    g_draw_count = 0;
    gl_->fDrawElements = [](GLenum mode, GLsizei count, GLenum type,
                            const void* indices) {
      ++g_draw_count;
      g_draw_elements(mode, count, type, indices);
    };
    gl_->fDrawElementsInstanced = [](GLenum mode, GLsizei count, GLenum type,
                                     const void* indices, GLsizei instances) {
      ++g_draw_count;
      g_draw_instanced(mode, count, type, indices, instances);
    };
  }

  ~ScopedDrawCounter() {
    gl_->fDrawElements = g_draw_elements;
    gl_->fDrawElementsInstanced = g_draw_instanced;
  }

 private:
  GLInterface* gl_;
};

TEST(StrokeDepthGLIntegrationTest, DirectFramebufferStrokeDrawsOnce) {
  auto* env = GoldenTestEnv::GetInstance();
  if (!env || env->GetBackend() != Backend::kGL) {
    GTEST_SKIP() << "Requires the GL golden environment";
  }
  auto context = GLContextCreate(reinterpret_cast<void*>(eglGetProcAddress));
  auto* gl = GLInterface::GlobalInterface();
  for (bool gpu_tessellation : {false, true}) {
    for (bool perspective : {false, true}) {
      SCOPED_TRACE(::testing::Message() << "gpu=" << gpu_tessellation
                                        << " perspective=" << perspective);
      context->SetEnableGPUTessellation(gpu_tessellation);
      GLuint fbo = 0;
      GLuint color = 0;
      GLuint depth_stencil = 0;
      gl->fGenFramebuffers(1, &fbo);
      gl->fGenTextures(1, &color);
      gl->fGenRenderbuffers(1, &depth_stencil);
      gl->fBindTexture(GL_TEXTURE_2D, color);
      gl->fTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 32, 32, 0, GL_RGBA,
                      GL_UNSIGNED_BYTE, nullptr);
      gl->fBindRenderbuffer(GL_RENDERBUFFER, depth_stencil);
      gl->fRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 32, 32);
      gl->fBindFramebuffer(GL_FRAMEBUFFER, fbo);
      gl->fFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                GL_TEXTURE_2D, color, 0);
      gl->fFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                   GL_RENDERBUFFER, depth_stencil);
      ASSERT_EQ(gl->fCheckFramebufferStatus(GL_FRAMEBUFFER),
                GL_FRAMEBUFFER_COMPLETE);
      GPUSurfaceDescriptorGL desc;
      desc.backend = GPUBackendType::kOpenGL;
      desc.width = desc.height = 32;
      desc.sample_count = 1;
      desc.surface_type = GLSurfaceType::kFramebuffer;
      desc.surface_mode = GLSurfaceMode::kDirect;
      desc.has_stencil_attachment = true;
      desc.gl_id = fbo;
      auto surface = context->CreateSurface(&desc);
      ASSERT_NE(surface, nullptr);

      auto* canvas = surface->LockCanvas();
      if (perspective) {
        Matrix transform;
        transform.SetPersp0(0.001f);
        canvas->Concat(transform);
      }
      Path path;
      path.MoveTo(4.f, 16.f);
      path.LineTo(28.f, 16.f);
      path.MoveTo(16.f, 16.f);
      path.LineTo(28.f, 16.f);
      Paint paint;
      paint.SetStyle(Paint::kStroke_Style);
      paint.SetStrokeWidth(8.f);
      paint.SetColor(ColorSetARGB(128, 255, 0, 0));
      canvas->DrawPath(path, paint);
      {
        ScopedDrawCounter counter(gl);
        canvas->Flush();
        surface->Flush();
        EXPECT_EQ(g_draw_count, 1u);
      }

      gl->fBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
      std::array<uint8_t, 4> single{}, overlap{};
      gl->fReadPixels(8, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, single.data());
      gl->fReadPixels(24, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, overlap.data());
      EXPECT_GT(single[3], 0);
      EXPECT_LT(single[3], 255);
      EXPECT_EQ(single, overlap);
      EXPECT_EQ(gl->fGetError(), GL_NO_ERROR);
      surface.reset();
      gl->fBindFramebuffer(GL_FRAMEBUFFER, 0);
      gl->fBindTexture(GL_TEXTURE_2D, 0);
      gl->fBindRenderbuffer(GL_RENDERBUFFER, 0);
      gl->fDeleteFramebuffers(1, &fbo);
      gl->fDeleteTextures(1, &color);
      gl->fDeleteRenderbuffers(1, &depth_stencil);
    }
  }
}

}  // namespace
}  // namespace skity::testing
