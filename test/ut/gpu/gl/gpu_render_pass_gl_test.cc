// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gl/gpu_render_pass_gl.hpp"

#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "src/gpu/gl/gl_interface.hpp"
#include "src/gpu/gl/gpu_buffer_gl.hpp"
#include "src/gpu/gl/gpu_device_gl.hpp"
#include "src/gpu/gl/gpu_render_pipeline_gl.hpp"
#include "src/gpu/gl/gpu_sampler_gl.hpp"
#include "src/gpu/gl/gpu_texture_gl.hpp"

namespace skity {

// Restore the process-wide interface after each test, including assertion
// exits.
// CMake auto-exports symbols for Windows tests, but DLL data still needs
// an explicit import declaration in the consumer.
#if defined(_WIN32)
__declspec(dllimport)
#endif
    extern GLInterface* g_interface;

namespace {

class TestShaderFunction : public GPUShaderFunction {
 public:
  TestShaderFunction() : GPUShaderFunction({}) {}
  bool IsValid() const override { return true; }
};

class GPURenderPassGLTest : public testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    saved_interface_ = g_interface;
    g_interface = &gl_;
    samplers_ = {};
    draws_.clear();
    gl_.fGetIntegerv = [](GLenum, GLint* value) { *value = 3; };
    gl_.fGetString = [](GLenum) {
      return reinterpret_cast<const GLubyte*>("Mock OpenGL 3.3");
    };
    gl_.fGetError = []() -> GLenum { return GL_NO_ERROR; };
    gl_.fGenBuffers = gl_.fGenSamplers = [](GLsizei, GLuint* id) { *id = 7; };
    gl_.fDeleteBuffers = gl_.fDeleteSamplers = [](GLsizei, const GLuint*) {};
    gl_.fDeleteProgram = [](GLuint) {};
    gl_.fSamplerParameteri = [](GLuint, GLenum, GLint) {};
    gl_.fBindFramebuffer = gl_.fBindBuffer =
        gl_.fBindTexture = [](GLenum, GLuint) {};
    gl_.fActiveTexture = [](GLenum) {};
    gl_.fBindSampler = [](GLuint unit, GLuint sampler) {
      samplers_.at(unit) = sampler;
    };
    gl_.fEnable = gl_.fDisable = gl_.fDepthFunc =
        gl_.fBlendEquation = [](GLenum) {};
    gl_.fStencilFunc = [](GLenum, GLint, GLuint) {};
    gl_.fStencilOp = [](GLenum, GLenum, GLenum) {};
    gl_.fStencilMask = gl_.fUseProgram = [](GLuint) {};
    gl_.fColorMask = [](GLboolean, GLboolean, GLboolean, GLboolean) {};
    gl_.fDepthMask = [](GLboolean) {};
    gl_.fBlendFunc = [](GLenum, GLenum) {};
    gl_.fScissor = gl_.fViewport = [](GLint, GLint, GLsizei, GLsizei) {};
    gl_.fDrawElements = [](GLenum, GLsizei, GLenum, const void*) {
      draws_.push_back(samplers_);
    };
    gl_.fDrawElementsInstanced = [](GLenum, GLsizei, GLenum, const void*,
                                    GLsizei) { draws_.push_back(samplers_); };
  }

  void TearDown() override { g_interface = saved_interface_; }

  GLInterface gl_;
  GLInterface* saved_interface_ = nullptr;
  static std::array<GLuint, 8> samplers_;
  static std::vector<std::array<GLuint, 8>> draws_;
};

std::array<GLuint, 8> GPURenderPassGLTest::samplers_;
std::vector<std::array<GLuint, 8>> GPURenderPassGLTest::draws_;

TEST_P(GPURenderPassGLTest, TextureOnlyDrawClearsPreviousSampler) {
  GPUDeviceGL device;
  GPUBufferGL buffer({});
  std::vector<GPUVertexBufferLayout> layouts;
  GPURenderPipelineDescriptor pipeline_desc;
  pipeline_desc.vertex_function = std::make_shared<TestShaderFunction>();
  pipeline_desc.fragment_function = std::make_shared<TestShaderFunction>();
  pipeline_desc.buffers = &layouts;
  GPURenderPipelineGL pipeline(std::make_shared<GLProgram>(1, true),
                               pipeline_desc);
  GPUSamplerDescriptor sampler_desc;
  sampler_desc.min_filter = GPUFilterMode::kLinear;
  sampler_desc.mag_filter = GPUFilterMode::kLinear;
  auto sampler = GPUSamplerGL::Create(sampler_desc);
  auto texture = std::make_shared<GPUTextureGL>(GPUTextureDescriptor{});
  GPUTextureDescriptor coverage_desc;
  coverage_desc.format = GPUTextureFormat::kRGBA16Uint;
  auto coverage = std::make_shared<GPUTextureGL>(coverage_desc);

  Command pattern;
  pattern.pipeline = &pipeline;
  pattern.vertex_buffer.buffer = &buffer;
  pattern.index_buffer.buffer = &buffer;
  pattern.index_count = 6;
  // Cover both the direct index and WGSL's shared-sampler unit list.
  for (uint32_t unit : {0u, 3u}) {
    TextureBinding binding{};
    binding.index = unit;
    binding.texture = texture;
    pattern.texture_bindings.emplace_back(binding);
  }
  SamplerBinding binding{};
  binding.index = 0;
  binding.sampler = sampler;
  if (GetParam()) {
    binding.uints = std::vector<uint32_t>{0, 3};
  }
  pattern.sampler_bindings.emplace_back(binding);

  Command circle;
  circle.pipeline = &pipeline;
  circle.vertex_buffer.buffer = &buffer;
  circle.index_buffer.buffer = &buffer;
  circle.instance_buffer.buffer = &buffer;
  circle.index_count = 6;
  circle.instance_count = 1;
  for (uint32_t unit : {0u, 3u}) {
    TextureBinding binding{};
    binding.index = unit;
    binding.texture = coverage;
    circle.texture_bindings.emplace_back(binding);
  }

  GPURenderPassGL pass({}, 0, &device);
  pass.AddCommand(&pattern);
  pass.AddCommand(&circle);
  pass.AddCommand(&pattern);
  pass.EncodeCommands(std::nullopt, std::nullopt);

  // Inspect state at each draw, before end-of-pass cleanup can hide leakage.
  ASSERT_EQ(draws_.size(), 3u);
  EXPECT_EQ(draws_[0][0], sampler->GetSamplerID());
  EXPECT_EQ(draws_[0][3], GetParam() ? sampler->GetSamplerID() : 0u);
  EXPECT_EQ(draws_[1][0], 0u);
  EXPECT_EQ(draws_[1][3], 0u);
  EXPECT_EQ(draws_[2], draws_[0]);
}

INSTANTIATE_TEST_SUITE_P(SamplerBindings, GPURenderPassGLTest, testing::Bool());

}  // namespace
}  // namespace skity
