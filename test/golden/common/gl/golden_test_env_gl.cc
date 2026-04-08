// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "common/gl/golden_test_env_gl.hpp"

#define EGL_EGLEXT_PROTOTYPES
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <skity/gpu/gpu_context_gl.hpp>

#include "common/gl/golden_texture_gl.hpp"

namespace skity {
namespace testing {

GoldenTestEnvGL::GoldenTestEnvGL() {}

void GoldenTestEnvGL::SetUp() {
  const EGLint displayAttribs[] = {
      EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE,
      EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE,
      EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE, EGL_NONE};

  display_ = eglGetPlatformDisplayEXT(
      EGL_PLATFORM_ANGLE_ANGLE, (void*)EGL_DEFAULT_DISPLAY, displayAttribs);

  EGLint major, minor;
  eglInitialize(display_, &major, &minor);

  const EGLint configAttribs[] = {EGL_SURFACE_TYPE,
                                  EGL_PBUFFER_BIT,
                                  EGL_RENDERABLE_TYPE,
                                  EGL_OPENGL_ES3_BIT,
                                  EGL_RED_SIZE,
                                  8,
                                  EGL_GREEN_SIZE,
                                  8,
                                  EGL_BLUE_SIZE,
                                  8,
                                  EGL_ALPHA_SIZE,
                                  8,
                                  EGL_NONE};
  EGLConfig config;
  EGLint numConfigs;
  eglChooseConfig(display_, configAttribs, &config, 1, &numConfigs);

  const EGLint pbufferAttribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
  surface_ = eglCreatePbufferSurface(display_, config, pbufferAttribs);

  const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  context_ = eglCreateContext(display_, config, EGL_NO_CONTEXT, contextAttribs);
  eglMakeCurrent(display_, surface_, surface_, context_);

  GoldenTestEnv::SetUp();
}

void GoldenTestEnvGL::TearDown() {
  GoldenTestEnv::TearDown();

  eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroySurface(display_, surface_);
  eglDestroyContext(display_, context_);
}

std::shared_ptr<GoldenTexture> GoldenTestEnvGL::DisplayListToTexture(
    DisplayList* dl, uint32_t width, uint32_t height) {
  eglMakeCurrent(display_, surface_, surface_, context_);
  // create off screen fbo and texture
  GLuint fbo = 0;
  GLuint texture = 0;
  glGenFramebuffers(1, &fbo);
  glGenTextures(1, &texture);

  if (fbo == 0 || texture == 0) {
    auto egl_error = eglGetError();
    std::cerr << "eglCreatePbufferSurface failed, error: " << std::hex
              << egl_error << std::endl;

    auto gl_error = glGetError();
    std::cerr << "glGenFramebuffers failed, error: " << std::hex << gl_error
              << std::endl;

    return {};
  }

  glBindTexture(GL_TEXTURE_2D, texture);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);

  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         texture, 0);

  auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

  if (status != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "glCheckFramebufferStatus failed, error: " << std::hex
              << status << std::endl;
    return {};
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);

  GPUSurfaceDescriptorGL surface_desc;
  surface_desc.backend = GPUBackendType::kOpenGL;
  surface_desc.width = width;
  surface_desc.height = height;
  surface_desc.sample_count = 4;
  surface_desc.surface_type = GLSurfaceType::kFramebuffer;
  surface_desc.gl_id = fbo;
  surface_desc.has_stencil_attachment = false;

  auto surface = GetGPUContext()->CreateSurface(&surface_desc);

  if (!surface) {
    return {};
  }

  auto canvas = surface->LockCanvas();

  dl->Draw(canvas);

  canvas->Flush();
  surface->Flush();

  glFinish();

  auto err = glGetError();

  // read pixels from fbo
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  std::vector<uint8_t> pixels(width * height * 4);
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  glDeleteFramebuffers(1, &fbo);
  glDeleteTextures(1, &texture);

  // flip Y in-place
  std::vector<uint8_t> rowTmpVec(width * 4);
  for (int y = 0; y < height / 2; y++) {
    unsigned char* rowA = pixels.data() + y * width * 4;
    unsigned char* rowB = pixels.data() + (height - 1 - y) * width * 4;
    std::memcpy(rowTmpVec.data(), rowA, width * 4);
    std::memcpy(rowA, rowB, width * 4);
    std::memcpy(rowB, rowTmpVec.data(), width * 4);
  }

  auto data = Data::MakeWithCopy(pixels.data(), pixels.size());

  auto pixmap = std::make_shared<Pixmap>(std::move(data), width, height,
                                         AlphaType::kPremul_AlphaType);

  auto image = skity::Image::MakeImage(pixmap, nullptr);

  return std::make_shared<GoldenTextureGL>(std::move(image), std::move(pixmap));
}

std::unique_ptr<skity::GPUContext> GoldenTestEnvGL::CreateGPUContext() {
  return GLContextCreate((void*)eglGetProcAddress);
}

GoldenTestEnv* CreateGoldenTestEnvGL() { return new GoldenTestEnvGL(); }

}  // namespace testing
}  // namespace skity
