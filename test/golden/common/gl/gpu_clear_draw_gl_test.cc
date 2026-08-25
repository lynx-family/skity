// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <EGL/egl.h>
#include <gtest/gtest.h>

#include <array>
#include <memory>

#include "common/golden_test_env.hpp"
#include "src/gpu/gl/gl_interface.hpp"
#include "src/gpu/gl/gpu_device_gl.hpp"
#include "src/gpu/gl/gpu_render_pass_gl.hpp"
#include "src/gpu/gl/gpu_texture_gl.hpp"
#include "src/gpu/gpu_context_impl.hpp"

namespace skity {
namespace testing {
namespace {

PFNGLGETSTRINGPROC g_real_get_string = nullptr;
PFNGLDRAWARRAYSPROC g_real_draw_arrays = nullptr;
uint32_t g_gl_clear_call_count = 0;
uint32_t g_gl_draw_arrays_call_count = 0;

const GLubyte* GLAD_API_PTR ForcedPowerVRGetString(GLenum name) {
  if (name == GL_RENDERER) {
    return reinterpret_cast<const GLubyte*>("PowerVR GE8320");
  }
  return g_real_get_string(name);
}

void GLAD_API_PTR CountGLClear(GLbitfield) { ++g_gl_clear_call_count; }

void GLAD_API_PTR CountGLDrawArrays(GLenum mode, GLint first, GLsizei count) {
  ++g_gl_draw_arrays_call_count;
  g_real_draw_arrays(mode, first, count);
}

class ScopedPowerVRWorkaround final {
 public:
  explicit ScopedPowerVRWorkaround(GLInterface* gl)
      : gl_(gl),
        get_string_(gl->fGetString),
        clear_(gl->fClear),
        draw_arrays_(gl->fDrawArrays) {
    g_real_get_string = get_string_;
    g_real_draw_arrays = draw_arrays_;
    g_gl_clear_call_count = 0;
    g_gl_draw_arrays_call_count = 0;
    gl_->fGetString = ForcedPowerVRGetString;
    gl_->fClear = CountGLClear;
    gl_->fDrawArrays = CountGLDrawArrays;
  }

  ~ScopedPowerVRWorkaround() {
    gl_->fGetString = get_string_;
    gl_->fClear = clear_;
    gl_->fDrawArrays = draw_arrays_;
  }

  uint32_t GetClearCallCount() const { return g_gl_clear_call_count; }

  uint32_t GetDrawArraysCallCount() const {
    return g_gl_draw_arrays_call_count;
  }

  void Clear(GLbitfield mask) const { clear_(mask); }

 private:
  GLInterface* gl_;
  PFNGLGETSTRINGPROC get_string_;
  PFNGLCLEARPROC clear_;
  PFNGLDRAWARRAYSPROC draw_arrays_;
};

float UnpackDepth(uint32_t value) {
  return static_cast<float>(value >> 8) / 0xFFFFFF;
}

TEST(GLClearDrawGLIntegrationTest,
     ForcedWorkaroundClearsColorDepthStencilAndHonorsScissor) {
  auto* env = GoldenTestEnv::GetInstance();
  if (!env || env->GetBackend() != Backend::kGL) {
    GTEST_SKIP() << "Requires the GL golden environment";
  }

  auto* gl = GLInterface::GlobalInterface();
  ASSERT_NE(gl, nullptr);
  ScopedPowerVRWorkaround workaround(gl);

  auto gpu_context =
      GLContextCreate(reinterpret_cast<void*>(eglGetProcAddress));
  auto* context = static_cast<GPUContextImpl*>(gpu_context.get());
  auto* device = static_cast<GPUDeviceGL*>(context->GetGPUDevice());
  ASSERT_TRUE(device->GetDriverWorkarounds().use_draw_for_clear);
  ASSERT_NE(device->GetClearDrawProgram(), 0u);
  ASSERT_TRUE(device->CanUseMSAA());

  constexpr uint32_t kSize = 8;
  GLuint fbo = 0;
  GLuint color_texture = 0;
  GLuint depth_stencil_buffer = 0;
  GLuint vao = 0;
  gl->fGenFramebuffers(1, &fbo);
  gl->fGenTextures(1, &color_texture);
  gl->fGenRenderbuffers(1, &depth_stencil_buffer);
  gl->fGenVertexArrays(1, &vao);
  ASSERT_NE(fbo, 0u);
  ASSERT_NE(color_texture, 0u);
  ASSERT_NE(depth_stencil_buffer, 0u);
  ASSERT_NE(vao, 0u);

  gl->fBindTexture(GL_TEXTURE_2D, color_texture);
  gl->fTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kSize, kSize, 0, GL_RGBA,
                  GL_UNSIGNED_BYTE, nullptr);
  gl->fBindRenderbuffer(GL_RENDERBUFFER, depth_stencil_buffer);
  gl->fRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, kSize, kSize);
  gl->fBindFramebuffer(GL_FRAMEBUFFER, fbo);
  gl->fFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            color_texture, 0);
  gl->fFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                               GL_RENDERBUFFER, depth_stencil_buffer);
  ASSERT_EQ(gl->fCheckFramebufferStatus(GL_FRAMEBUFFER),
            GL_FRAMEBUFFER_COMPLETE);

  gl->fDisable(GL_SCISSOR_TEST);
  gl->fColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  gl->fDepthMask(GL_TRUE);
  gl->fStencilMask(0xFF);
  gl->fClearColor(1.f, 0.f, 1.f, 1.f);
  gl->fClearDepthf(1.f);
  gl->fClearStencil(0x7F);
  workaround.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                   GL_STENCIL_BUFFER_BIT);

  GPUTextureDescriptor color_desc;
  color_desc.width = kSize;
  color_desc.height = kSize;
  color_desc.format = GPUTextureFormat::kRGBA8Unorm;
  auto color = std::make_shared<GPUTexturePlaceholderGL>(color_desc);

  GPUTextureDescriptor depth_stencil_desc = color_desc;
  depth_stencil_desc.format = GPUTextureFormat::kDepth24Stencil8;
  auto depth_stencil = std::make_shared<GPUTextureRenderBufferGL>(
      depth_stencil_desc, depth_stencil_buffer);

  GPURenderPassDescriptor pass_desc;
  pass_desc.color_attachment.texture = color;
  pass_desc.color_attachment.load_op = GPULoadOp::kClear;
  pass_desc.color_attachment.clear_value = {};
  pass_desc.depth_attachment.texture = depth_stencil;
  pass_desc.depth_attachment.load_op = GPULoadOp::kClear;
  pass_desc.depth_attachment.store_op = GPUStoreOp::kStore;
  pass_desc.depth_attachment.clear_value = 0.f;
  pass_desc.stencil_attachment.texture = depth_stencil;
  pass_desc.stencil_attachment.load_op = GPULoadOp::kClear;
  pass_desc.stencil_attachment.store_op = GPUStoreOp::kStore;
  pass_desc.stencil_attachment.clear_value = 0;

  gl->fBindVertexArray(vao);
  gl->fEnable(GL_SCISSOR_TEST);
  {
    GPURenderPassGL pass(pass_desc, fbo, device);
    pass.EncodeCommands(std::nullopt, GPUScissorRect{2, 2, 4, 4});
  }
  EXPECT_EQ(workaround.GetClearCallCount(), 0u);
  EXPECT_EQ(gl->fGetError(), GL_NO_ERROR);

  gl->fBindFramebuffer(GL_FRAMEBUFFER, fbo);
  std::array<uint8_t, 4> inside_color = {};
  std::array<uint8_t, 4> outside_color = {};
  gl->fReadPixels(3, 3, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, inside_color.data());
  gl->fReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, outside_color.data());
  EXPECT_EQ(inside_color, (std::array<uint8_t, 4>{0, 0, 0, 0}));
  EXPECT_EQ(outside_color, (std::array<uint8_t, 4>{255, 0, 255, 255}));

  uint32_t inside_depth_stencil = 0;
  uint32_t outside_depth_stencil = 0;
  gl->fReadPixels(3, 3, 1, 1, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8,
                  &inside_depth_stencil);
  gl->fReadPixels(0, 0, 1, 1, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8,
                  &outside_depth_stencil);
  EXPECT_FLOAT_EQ(UnpackDepth(inside_depth_stencil), 0.f);
  EXPECT_EQ(inside_depth_stencil & 0xFF, 0u);
  EXPECT_FLOAT_EQ(UnpackDepth(outside_depth_stencil), 1.f);
  EXPECT_EQ(outside_depth_stencil & 0xFF, 0x7Fu);
  EXPECT_EQ(gl->fGetError(), GL_NO_ERROR);

  pass_desc.color_attachment.load_op = GPULoadOp::kLoad;
  const auto preserve_color_draw_count = workaround.GetDrawArraysCallCount();
  {
    GPURenderPassGL pass(pass_desc, fbo, device);
    pass.EncodeCommands(std::nullopt, GPUScissorRect{0, 0, kSize, kSize});
  }
  EXPECT_EQ(workaround.GetDrawArraysCallCount(), preserve_color_draw_count + 1);

  gl->fBindFramebuffer(GL_FRAMEBUFFER, fbo);
  gl->fReadPixels(3, 3, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, inside_color.data());
  gl->fReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, outside_color.data());
  EXPECT_EQ(inside_color, (std::array<uint8_t, 4>{0, 0, 0, 0}));
  EXPECT_EQ(outside_color, (std::array<uint8_t, 4>{255, 0, 255, 255}));

  gl->fReadPixels(0, 0, 1, 1, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8,
                  &outside_depth_stencil);
  EXPECT_FLOAT_EQ(UnpackDepth(outside_depth_stencil), 0.f);
  EXPECT_EQ(outside_depth_stencil & 0xFF, 0u);
  EXPECT_EQ(workaround.GetClearCallCount(), 0u);
  EXPECT_EQ(gl->fGetError(), GL_NO_ERROR);
  pass_desc.color_attachment.load_op = GPULoadOp::kClear;

  gl->fDisable(GL_SCISSOR_TEST);
  gl->fColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  gl->fDepthMask(GL_TRUE);
  gl->fStencilMask(0xFF);
  gl->fClearColor(1.f, 0.f, 1.f, 1.f);
  gl->fClearDepthf(1.f);
  gl->fClearStencil(0x7F);
  workaround.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                   GL_STENCIL_BUFFER_BIT);

  gl->fEnable(GL_SCISSOR_TEST);
  {
    GPURenderPassGL pass(pass_desc, fbo, device);
    pass.EncodeCommands(GPUViewport{2.f, 2.f, 2.f, 2.f, 0.f, 1.f},
                        GPUScissorRect{0, 0, kSize, kSize});
  }
  gl->fBindFramebuffer(GL_FRAMEBUFFER, fbo);
  gl->fReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, outside_color.data());
  EXPECT_EQ(outside_color, (std::array<uint8_t, 4>{0, 0, 0, 0}));
  EXPECT_EQ(workaround.GetClearCallCount(), 0u);
  EXPECT_EQ(gl->fGetError(), GL_NO_ERROR);

  gl->fBindFramebuffer(GL_FRAMEBUFFER, fbo);
  gl->fFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                               GL_RENDERBUFFER, 0);
  ASSERT_EQ(gl->fCheckFramebufferStatus(GL_FRAMEBUFFER),
            GL_FRAMEBUFFER_COMPLETE);
  pass_desc.depth_attachment.texture.reset();
  pass_desc.stencil_attachment.texture.reset();

  auto draw_arrays_count = workaround.GetDrawArraysCallCount();
  {
    GPURenderPassGL pass(pass_desc, fbo, device);
    pass.EncodeCommands(std::nullopt, GPUScissorRect{0, 0, kSize, kSize});
  }
  EXPECT_EQ(workaround.GetDrawArraysCallCount(), draw_arrays_count + 1);
  gl->fBindFramebuffer(GL_FRAMEBUFFER, fbo);
  gl->fReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, outside_color.data());
  EXPECT_EQ(outside_color, (std::array<uint8_t, 4>{0, 0, 0, 0}));
  EXPECT_EQ(gl->fGetError(), GL_NO_ERROR);

  gl->fClearColor(1.f, 0.f, 1.f, 1.f);
  workaround.Clear(GL_COLOR_BUFFER_BIT);
  pass_desc.color_attachment.load_op = GPULoadOp::kLoad;
  draw_arrays_count = workaround.GetDrawArraysCallCount();
  {
    GPURenderPassGL pass(pass_desc, fbo, device);
    pass.EncodeCommands(std::nullopt, GPUScissorRect{0, 0, kSize, kSize});
  }
  EXPECT_EQ(workaround.GetDrawArraysCallCount(), draw_arrays_count);
  gl->fBindFramebuffer(GL_FRAMEBUFFER, fbo);
  gl->fReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, outside_color.data());
  EXPECT_EQ(outside_color, (std::array<uint8_t, 4>{255, 0, 255, 255}));
  EXPECT_EQ(gl->fGetError(), GL_NO_ERROR);

  gl->fFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                               GL_RENDERBUFFER, depth_stencil_buffer);
  pass_desc.color_attachment.load_op = GPULoadOp::kClear;
  pass_desc.depth_attachment.texture = depth_stencil;
  pass_desc.stencil_attachment.texture = depth_stencil;

  // Exercise the normal HWLayer pass, DrawTexture's final fake pass, and the
  // desktop MSAA resolve path.
  GPUSurfaceDescriptorGL surface_desc;
  surface_desc.backend = GPUBackendType::kOpenGL;
  surface_desc.width = kSize;
  surface_desc.height = kSize;
  surface_desc.surface_type = GLSurfaceType::kFramebuffer;
  surface_desc.gl_id = fbo;
  surface_desc.has_stencil_attachment = true;
  for (auto mode : {GLSurfaceMode::kDirect, GLSurfaceMode::kDrawTexture,
                    GLSurfaceMode::kBlit}) {
    surface_desc.sample_count = mode == GLSurfaceMode::kBlit ? 4 : 1;
    gl->fBindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl->fDisable(GL_SCISSOR_TEST);
    gl->fColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    gl->fDepthMask(GL_TRUE);
    gl->fStencilMask(0xFF);
    gl->fClearColor(1.f, 0.f, 1.f, 1.f);
    gl->fClearDepthf(1.f);
    gl->fClearStencil(0x7F);
    workaround.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                     GL_STENCIL_BUFFER_BIT);

    surface_desc.surface_mode = mode;
    const auto clear_draw_count = workaround.GetDrawArraysCallCount();
    auto surface = gpu_context->CreateSurface(&surface_desc);
    ASSERT_NE(surface, nullptr);
    auto* canvas = surface->LockCanvas();
    ASSERT_NE(canvas, nullptr);
    Paint paint;
    paint.SetColor(Color_RED);
    canvas->DrawRect(Rect::MakeXYWH(2.f, 2.f, 4.f, 4.f), paint);
    canvas->Flush();
    surface->Flush();

    gl->fBindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl->fReadPixels(3, 3, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, inside_color.data());
    gl->fReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                    outside_color.data());
    EXPECT_EQ(inside_color, (std::array<uint8_t, 4>{255, 0, 0, 255}));
    EXPECT_EQ(outside_color, (std::array<uint8_t, 4>{0, 0, 0, 0}));
    EXPECT_GT(workaround.GetDrawArraysCallCount(), clear_draw_count);
    EXPECT_EQ(workaround.GetClearCallCount(), 0u);
    EXPECT_EQ(gl->fGetError(), GL_NO_ERROR);
  }

  gl->fBindFramebuffer(GL_FRAMEBUFFER, 0);
  gl->fBindVertexArray(0);
  gl->fDisable(GL_SCISSOR_TEST);
  gl->fDeleteFramebuffers(1, &fbo);
  gl->fDeleteTextures(1, &color_texture);
  gl->fDeleteVertexArrays(1, &vao);
}

}  // namespace
}  // namespace testing
}  // namespace skity
