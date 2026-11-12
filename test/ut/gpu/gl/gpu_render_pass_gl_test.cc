// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gl/gpu_render_pass_gl.hpp"

#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <utility>
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

enum class IndexEvent { kDraw, kDelete };

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
  static std::vector<std::pair<IndexEvent, GLuint>> index_events_;
  static GLuint bound_index_;
  static GLuint next_buffer_;
};

std::array<GLuint, 8> GPURenderPassGLTest::samplers_;
std::vector<std::array<GLuint, 8>> GPURenderPassGLTest::draws_;
std::vector<std::pair<IndexEvent, GLuint>> GPURenderPassGLTest::index_events_;
GLuint GPURenderPassGLTest::bound_index_;
GLuint GPURenderPassGLTest::next_buffer_;

TEST_P(GPURenderPassGLTest, TextureOnlyDrawClearsPreviousSampler) {
  GPUDeviceGL device;
  auto buffer = device.CreateBuffer({});
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
  pattern.vertex_buffer.buffer = buffer.get();
  pattern.index_buffer.buffer = buffer.get();
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
  circle.vertex_buffer.buffer = buffer.get();
  circle.index_buffer.buffer = buffer.get();
  circle.instance_buffer.buffer = buffer.get();
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

TEST_F(GPURenderPassGLTest, InvalidatesDiscardedDepthStencilBeforeUnbinding) {
  static GLuint bound_fbo;
  static int invalidations;
  bound_fbo = 0;
  invalidations = 0;
  gl_.fBindFramebuffer = [](GLenum, GLuint fbo) { bound_fbo = fbo; };
  gl_.fInvalidateFramebuffer = [](GLenum target, GLsizei count,
                                  const GLenum* attachments) {
    EXPECT_EQ(bound_fbo, 7u);
    EXPECT_EQ(target, GL_FRAMEBUFFER);
    ASSERT_EQ(count, 2);
    EXPECT_EQ(attachments[0], GL_STENCIL_ATTACHMENT);
    EXPECT_EQ(attachments[1], GL_DEPTH_ATTACHMENT);
    ++invalidations;
  };

  GPUDeviceGL device;
  GPURenderPassDescriptor desc;
  GPUTextureDescriptor texture_desc;
  texture_desc.format = GPUTextureFormat::kDepth24Stencil8;
  desc.stencil_attachment.texture =
      std::make_shared<GPUTextureGL>(texture_desc);
  desc.depth_attachment.texture = desc.stencil_attachment.texture;
  desc.depth_attachment.store_op = GPUStoreOp::kDiscard;
  desc.stencil_attachment.store_op = GPUStoreOp::kDiscard;
  GPURenderPassGL(desc, 7, &device).EncodeCommands(std::nullopt, std::nullopt);
  EXPECT_EQ(invalidations, 1);
  EXPECT_EQ(bound_fbo, 0u);

  GPURenderPassGL(desc, 0, &device).EncodeCommands(std::nullopt, std::nullopt);
  desc.stencil_attachment.store_op = GPUStoreOp::kStore;
  desc.depth_attachment.store_op = GPUStoreOp::kStore;
  GPURenderPassGL(desc, 7, &device).EncodeCommands(std::nullopt, std::nullopt);
  EXPECT_EQ(invalidations, 1);

  desc.stencil_attachment.store_op = GPUStoreOp::kDiscard;
  desc.depth_attachment.store_op = GPUStoreOp::kDiscard;
  gl_.fInvalidateFramebuffer = nullptr;
  GPURenderPassGL(desc, 7, &device).EncodeCommands(std::nullopt, std::nullopt);
  EXPECT_EQ(bound_fbo, 0u);
}

TEST_P(GPURenderPassGLTest,
       ReleasesPreviousIndexOnlyAfterDifferentIndexedDraw) {
  gl_.fGetString = [](GLenum name) {
    const char* value = name == GL_VENDOR     ? "Mesa/X.org"
                        : name == GL_RENDERER ? "virgl"
                                              : "OpenGL ES 3.2 Mesa 20.2.3";
    return reinterpret_cast<const GLubyte*>(value);
  };
  next_buffer_ = 1;
  bound_index_ = 0;
  index_events_.clear();
  gl_.fGenBuffers = [](GLsizei count, GLuint* ids) {
    for (GLsizei i = 0; i < count; ++i) {
      ids[i] = next_buffer_++;
    }
  };
  gl_.fBindBuffer = [](GLenum target, GLuint buffer) {
    if (target == GL_ELEMENT_ARRAY_BUFFER) {
      bound_index_ = buffer;
    }
  };
  gl_.fDeleteBuffers = [](GLsizei count, const GLuint* ids) {
    for (GLsizei i = 0; i < count; ++i) {
      index_events_.emplace_back(IndexEvent::kDelete, ids[i]);
    }
  };
  gl_.fDrawElements = [](GLenum, GLsizei, GLenum, const void*) {
    index_events_.emplace_back(IndexEvent::kDraw, bound_index_);
  };
  gl_.fDrawElementsInstanced = [](GLenum, GLsizei, GLenum, const void*,
                                  GLsizei) {
    index_events_.emplace_back(IndexEvent::kDraw, bound_index_);
  };
  auto device = std::make_unique<GPUDeviceGL>();
  GPUBufferDescriptor vertex_desc;
  vertex_desc.usage = GPUBufferUsage::kVertexBuffer;
  auto vertex = device->CreateBuffer(vertex_desc);
  std::shared_ptr<GPUBuffer> first = device->CreateBuffer({});
  const auto first_id = static_cast<GPUBufferGL*>(first.get())->GetBufferId();
  std::vector<GPUVertexBufferLayout> layouts;
  GPURenderPipelineDescriptor desc;
  desc.vertex_function = std::make_shared<TestShaderFunction>();
  desc.fragment_function = std::make_shared<TestShaderFunction>();
  desc.buffers = &layouts;
  GPURenderPipelineGL pipeline(std::make_shared<GLProgram>(1, true), desc);
  auto draw = [&](GPUBuffer* buffer, uint32_t count, uint32_t instances = 1) {
    Command command;
    command.pipeline = &pipeline;
    command.vertex_buffer.buffer = vertex.get();
    command.index_buffer.buffer = buffer;
    command.index_count = count;
    if (GetParam()) {
      command.instance_buffer.buffer = vertex.get();
      command.instance_count = instances;
    }
    GPURenderPassGL pass({}, 0, device.get());
    pass.AddCommand(&command);
    pass.EncodeCommands(std::nullopt, std::nullopt);
  };

  // A buffer owned only by a unique_ptr can still be drawn and released.
  auto unshared = device->CreateBuffer({});
  const auto unshared_id =
      static_cast<GPUBufferGL*>(unshared.get())->GetBufferId();
  draw(unshared.get(), 6);
  unshared.reset();

  draw(first.get(), 6);
  draw(first.get(), 6);
  first.reset();
  std::vector<std::pair<IndexEvent, GLuint>> expected_events = {
      {IndexEvent::kDraw, unshared_id},
      {IndexEvent::kDelete, unshared_id},
      {IndexEvent::kDraw, first_id},
      {IndexEvent::kDraw, first_id}};
  EXPECT_EQ(index_events_, expected_events);
  std::shared_ptr<GPUBuffer> second = device->CreateBuffer({});
  const auto second_id = static_cast<GPUBufferGL*>(second.get())->GetBufferId();
  // Allocation, binding and empty commands do not replace the driver cache.
  gl_.fBindBuffer(GL_ELEMENT_ARRAY_BUFFER, second_id);
  draw(second.get(), 0);
  if (GetParam()) {
    draw(second.get(), 6, 0);
  }
  EXPECT_EQ(index_events_, expected_events);

  // The GL call is issued, but VirGL skips it before updating the index buffer.
  draw(second.get(), 2);
  expected_events.emplace_back(IndexEvent::kDraw, second_id);
  EXPECT_EQ(index_events_, expected_events);

  draw(second.get(), 6);
  second.reset();
  expected_events.emplace_back(IndexEvent::kDraw, second_id);
  expected_events.emplace_back(IndexEvent::kDelete, first_id);
  EXPECT_EQ(index_events_, expected_events);
  device.reset();
  expected_events.emplace_back(IndexEvent::kDelete, second_id);
  EXPECT_EQ(index_events_, expected_events);
}

INSTANTIATE_TEST_SUITE_P(SamplerBindings, GPURenderPassGLTest, testing::Bool());

}  // namespace
}  // namespace skity
