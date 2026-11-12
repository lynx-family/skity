// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gl/gpu_buffer_gl.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "src/gpu/gl/gpu_device_gl.hpp"

namespace skity {

#if defined(_WIN32)
__declspec(dllimport)
#endif
    extern GLInterface* g_interface;

namespace {

class GPUBufferGLTest : public testing::Test {
 protected:
  void SetUp() override {
    saved_ = g_interface;
    g_interface = &gl_;
    deleted_.clear();
    next_name_ = 1;
    gl_.fGenBuffers = [](GLsizei count, GLuint* buffers) {
      for (GLsizei i = 0; i < count; ++i) {
        buffers[i] = next_name_++;
      }
    };
    gl_.fDeleteBuffers = [](GLsizei count, const GLuint* buffers) {
      for (GLsizei i = 0; i < count; ++i) {
        deleted_.push_back(buffers[i]);
      }
    };
    gl_.fGetIntegerv = [](GLenum, GLint* value) { *value = 3; };
    gl_.fGetString = [](GLenum name) {
      const char* value = name == GL_VENDOR     ? "Mesa/X.org"
                          : name == GL_RENDERER ? "virgl"
                                                : "OpenGL ES 3.2 Mesa 20.2.3";
      return reinterpret_cast<const GLubyte*>(value);
    };
    gl_.fGetError = []() -> GLenum { return GL_NO_ERROR; };
  }

  void TearDown() override { g_interface = saved_; }

  static GLuint BufferId(const GPUBuffer& buffer) {
    return static_cast<const GPUBufferGL&>(buffer).GetBufferId();
  }

  GLInterface gl_;
  GLInterface* saved_ = nullptr;
  static std::vector<GLuint> deleted_;
  static GLuint next_name_;
};

std::vector<GLuint> GPUBufferGLTest::deleted_;
GLuint GPUBufferGLTest::next_name_;

TEST_F(GPUBufferGLTest, OnlyLastDrawnIndexResourceIsRetained) {
  GPUDeviceGL device;
  ASSERT_TRUE(device.GetDriverWorkarounds().retain_last_index_buffer);
  std::shared_ptr<GPUBuffer> first = device.CreateBuffer({});
  const auto first_name = BufferId(*first);
  device.DidDrawIndexedBuffer(first);
  device.DidDrawIndexedBuffer(first);
  first.reset();
  EXPECT_TRUE(deleted_.empty());

  auto unused = device.CreateBuffer({});
  const auto unused_name = BufferId(*unused);
  unused.reset();
  EXPECT_EQ(deleted_, (std::vector<GLuint>{unused_name}));

  std::shared_ptr<GPUBuffer> next = device.CreateBuffer({});
  device.DidDrawIndexedBuffer(next);
  EXPECT_EQ(deleted_, (std::vector<GLuint>{unused_name, first_name}));
  next.reset();
  EXPECT_EQ(deleted_, (std::vector<GLuint>{unused_name, first_name}));
}

TEST_F(GPUBufferGLTest, ReplacingDeviceReferenceDoesNotDeleteLiveBuffer) {
  auto device = std::make_unique<GPUDeviceGL>();
  std::shared_ptr<GPUBuffer> first = device->CreateBuffer({});
  std::shared_ptr<GPUBuffer> second = device->CreateBuffer({});
  const auto first_name = BufferId(*first);
  const auto second_name = BufferId(*second);
  device->DidDrawIndexedBuffer(first);
  device->DidDrawIndexedBuffer(second);
  EXPECT_TRUE(deleted_.empty());
  first.reset();
  EXPECT_EQ(deleted_, (std::vector<GLuint>{first_name}));
  second.reset();
  EXPECT_EQ(deleted_, (std::vector<GLuint>{first_name}));
  device.reset();
  EXPECT_EQ(deleted_, (std::vector<GLuint>{first_name, second_name}));
}

TEST_F(GPUBufferGLTest, DeviceCanBeDestroyedBeforeLastDrawnBuffer) {
  auto device = std::make_unique<GPUDeviceGL>();
  std::shared_ptr<GPUBuffer> buffer = device->CreateBuffer({});
  const auto name = BufferId(*buffer);
  device->DidDrawIndexedBuffer(buffer);
  device.reset();
  EXPECT_TRUE(deleted_.empty());
  buffer.reset();
  EXPECT_EQ(deleted_, (std::vector<GLuint>{name}));
}

TEST_F(GPUBufferGLTest, DeviceDestructionReleasesLastDrawnBuffer) {
  auto device = std::make_unique<GPUDeviceGL>();
  std::shared_ptr<GPUBuffer> buffer = device->CreateBuffer({});
  const auto name = BufferId(*buffer);
  std::weak_ptr<GPUBuffer> weak_buffer = buffer;
  device->DidDrawIndexedBuffer(buffer);
  buffer.reset();
  EXPECT_TRUE(deleted_.empty());
  EXPECT_FALSE(weak_buffer.expired());
  device.reset();
  EXPECT_EQ(deleted_, (std::vector<GLuint>{name}));
  EXPECT_TRUE(weak_buffer.expired());
}

TEST_F(GPUBufferGLTest, RetentionIsBoundedAcrossFrames) {
  auto device = std::make_unique<GPUDeviceGL>();
  for (GLuint i = 1; i <= 1000; ++i) {
    std::shared_ptr<GPUBuffer> buffer = device->CreateBuffer({});
    device->DidDrawIndexedBuffer(buffer);
    buffer.reset();
    ASSERT_EQ(deleted_.size(), i - 1u);
    if (i > 1) {
      EXPECT_EQ(deleted_.back(), i - 1u);
    }
  }
  device.reset();
  ASSERT_EQ(deleted_.size(), 1000u);
  EXPECT_EQ(deleted_.back(), 1000u);
}

TEST_F(GPUBufferGLTest, DevicesHaveIndependentRetainedResources) {
  auto first_device = std::make_unique<GPUDeviceGL>();
  auto second_device = std::make_unique<GPUDeviceGL>();
  std::shared_ptr<GPUBuffer> first = first_device->CreateBuffer({});
  std::shared_ptr<GPUBuffer> second = second_device->CreateBuffer({});
  const auto first_name = BufferId(*first);
  const auto second_name = BufferId(*second);
  first_device->DidDrawIndexedBuffer(first);
  second_device->DidDrawIndexedBuffer(second);
  first.reset();
  second.reset();
  EXPECT_TRUE(deleted_.empty());
  second_device.reset();
  EXPECT_EQ(deleted_, (std::vector<GLuint>{second_name}));
  first_device.reset();
  EXPECT_EQ(deleted_, (std::vector<GLuint>{second_name, first_name}));
}

TEST_F(GPUBufferGLTest, NonIndexBuffersDeleteImmediately) {
  GPUDeviceGL device;
  GPUBufferDescriptor desc;
  desc.usage = GPUBufferUsage::kVertexBuffer | GPUBufferUsage::kUniformBuffer;
  auto vertex = device.CreateBuffer(desc);
  const auto name = BufferId(*vertex);
  vertex.reset();
  EXPECT_EQ(deleted_, (std::vector<GLuint>{name}));
}

TEST_F(GPUBufferGLTest, UnaffectedDeviceDeletesDrawnIndexImmediately) {
  gl_.fGetString = [](GLenum) {
    return reinterpret_cast<const GLubyte*>("OpenGL ES 3.2 Mali-G72");
  };
  GPUDeviceGL device;
  ASSERT_FALSE(device.GetDriverWorkarounds().retain_last_index_buffer);
  std::shared_ptr<GPUBuffer> buffer = device.CreateBuffer({});
  const auto name = BufferId(*buffer);
  device.DidDrawIndexedBuffer(buffer);
  buffer.reset();
  EXPECT_EQ(deleted_, (std::vector<GLuint>{name}));
}

}  // namespace
}  // namespace skity
