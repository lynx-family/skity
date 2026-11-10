// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#import <Metal/Metal.h>

#include <gtest/gtest.h>

#include <skity/gpu/gpu_context_mtl.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <skity/io/data.hpp>
#include <vector>

#include "src/gpu/gpu_context_impl.hpp"
#include "src/gpu/gpu_device.hpp"

namespace skity {
namespace {

constexpr char kShader[] = R"(
struct VertexInput {
  @location(0) position: vec2<f32>,
  @location(1) transform_index: u32,
};
@group(0) @binding(0) var<uniform> projection: mat4x4<f32>;
@group(0) @binding(1) var<storage, read> transforms: array<mat4x4<f32>>;
@vertex fn vs_main(input: VertexInput) -> @builtin(position) vec4<f32> {
  return projection * transforms[input.transform_index] * vec4<f32>(input.position, 0.0, 1.0);
}
@fragment fn fs_main() -> @location(0) vec4<f32> {
  return vec4<f32>(1.0, 0.0, 0.0, 1.0);
}
)";

std::unique_ptr<GPUBuffer> UploadBuffer(GPUDevice& device, GPUBufferUsage usage, const void* data,
                                        size_t size) {
  auto buffer = device.CreateBuffer({usage, GPUBufferStorageMode::kHostVisible});
  if (buffer == nullptr) {
    return nullptr;
  }
  void* mapped = buffer->Map(size);
  if (mapped == nullptr) {
    return nullptr;
  }
  std::memcpy(mapped, data, size);
  buffer->Unmap();
  return buffer;
}

TEST(GPUStorageMetalTest, ReadsTwoTransformsAfterUploadOwnerIsReleased) {
  id<MTLDevice> native_device = MTLCreateSystemDefaultDevice();
  if (native_device == nil) {
    GTEST_SKIP() << "Metal device unavailable";
  }
  auto context = MTLContextCreate(native_device, nil);
  ASSERT_NE(context, nullptr);
  auto* context_impl = static_cast<GPUContextImpl*>(context.get());
  ASSERT_NE(context_impl->GetGPUDevice(), nullptr);
  auto& device = *context_impl->GetGPUDevice();
  const auto limit = device.GetCaps().max_vertex_storage_buffer_range;
  if (@available(macOS 10.14, *)) {
    ASSERT_GT(limit, 0u);
  } else {
    GTEST_SKIP() << "Metal buffer length unavailable";
  }

  constexpr std::array<float, 16> kIdentity = {
      1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f,
  };
  auto transforms = std::array{kIdentity, kIdentity};
  transforms[0][12] = -0.5f;
  transforms[1][12] = 0.5f;
  EXPECT_EQ(device.CreateReadOnlyStorageBuffer(nullptr, sizeof(transforms)), nullptr);
  EXPECT_EQ(device.CreateReadOnlyStorageBuffer(transforms.data(), 0), nullptr);
  EXPECT_EQ(device.CreateReadOnlyStorageBuffer(transforms.data(), static_cast<size_t>(limit) + 1),
            nullptr);
  auto storage = device.CreateReadOnlyStorageBuffer(transforms.data(), sizeof(transforms));
  ASSERT_NE(storage, nullptr);
  const auto identities = std::array{kIdentity, kIdentity};
  auto other = device.CreateReadOnlyStorageBuffer(identities.data(), sizeof(identities));
  ASSERT_NE(other, nullptr);
  EXPECT_NE(storage, other);

  auto module = device.CreateShaderModule({GPULabel("StorageMetalTest"), kShader});
  ASSERT_NE(module, nullptr);
  GPUShaderSourceWGX source{module, "vs_main", {}};
  GPUShaderFunctionDescriptor shader_desc{};
  shader_desc.source_type = GPUShaderSourceType::kWGX;
  shader_desc.shader_source = &source;
  shader_desc.error_callback = [](const char* error) { ADD_FAILURE() << error; };
  auto vertex_function = device.CreateShaderFunction(shader_desc);
  source.entry_point = "fs_main";
  shader_desc.stage = GPUShaderStage::kFragment;
  auto fragment_function = device.CreateShaderFunction(shader_desc);
  ASSERT_NE(vertex_function, nullptr);
  ASSERT_NE(fragment_function, nullptr);

  struct Vertex {
    float x;
    float y;
    uint32_t transform_index;
  };
  const std::vector<GPUVertexBufferLayout> layout = {
      {sizeof(Vertex),
       GPUVertexStepMode::kVertex,
       {{GPUVertexFormat::kFloat32x2, offsetof(Vertex, x), 0},
        {GPUVertexFormat::kUint32, offsetof(Vertex, transform_index), 1}}},
  };
  GPURenderPipelineDescriptor pipeline_desc{};
  pipeline_desc.vertex_function = vertex_function;
  pipeline_desc.fragment_function = fragment_function;
  pipeline_desc.buffers = &layout;
  pipeline_desc.target.format = GPUTextureFormat::kRGBA8Unorm;
  pipeline_desc.depth_stencil.format = GPUTextureFormat::kInvalid;
  pipeline_desc.error_callback = shader_desc.error_callback;
  auto pipeline = device.CreateRenderPipeline(pipeline_desc);
  ASSERT_NE(pipeline, nullptr);
  const auto* group = pipeline->GetBindingGroup(0);
  ASSERT_NE(group, nullptr);
  const auto* uniform_entry = group->GetEntry(0);
  const auto* storage_entry = group->GetEntry(1);
  ASSERT_NE(uniform_entry, nullptr);
  ASSERT_NE(storage_entry, nullptr);
  EXPECT_NE(uniform_entry->index, storage_entry->index);

  constexpr Vertex kVertices[] = {{-0.25f, -0.25f, 0}, {0.25f, -0.25f, 0}, {0.f, 0.25f, 0},
                                  {-0.25f, -0.25f, 1}, {0.25f, -0.25f, 1}, {0.f, 0.25f, 1}};
  constexpr std::array<uint32_t, 6> kIndices = {0, 1, 2, 3, 4, 5};
  auto vertices = UploadBuffer(device, GPUBufferUsage::kVertexBuffer, kVertices, sizeof(kVertices));
  auto indices =
      UploadBuffer(device, GPUBufferUsage::kIndexBuffer, kIndices.data(), sizeof(kIndices));
  auto uniform =
      UploadBuffer(device, GPUBufferUsage::kUniformBuffer, kIdentity.data(), sizeof(kIdentity));
  ASSERT_NE(vertices, nullptr);
  ASSERT_NE(indices, nullptr);
  ASSERT_NE(uniform, nullptr);

  constexpr uint32_t kSize = 64;
  Command command{};
  command.pipeline = pipeline.get();
  command.vertex_buffer = {vertices.get(), 0, sizeof(kVertices)};
  command.index_buffer = {indices.get(), 0, sizeof(kIndices)};
  command.index_count = kIndices.size();
  command.scissor_rect = {0, 0, kSize, kSize};
  command.uniform_bindings.push_back({static_cast<GPUShaderStageMask>(GPUShaderStage::kVertex),
                                      uniform_entry->index,
                                      group->group,
                                      uniform_entry->binding,
                                      uniform_entry->name,
                                      {uniform.get(), 0, sizeof(kIdentity)}});
  EXPECT_FALSE(command.IsValid());
  command.storage_bindings.push_back({storage_entry->index + 1, group->group,
                                      storage_entry->binding, storage, sizeof(transforms)});
  EXPECT_FALSE(command.IsValid());
  command.storage_bindings.back().index = storage_entry->index;
  ASSERT_TRUE(command.IsValid());
  std::weak_ptr<GPUBuffer> owner = storage;
  storage.reset();
  EXPECT_FALSE(owner.expired());

  GPUTextureDescriptor texture_desc{};
  texture_desc.width = kSize;
  texture_desc.height = kSize;
  texture_desc.format = GPUTextureFormat::kRGBA8Unorm;
  texture_desc.usage = static_cast<GPUTextureUsageMask>(GPUTextureUsage::kRenderAttachment) |
                       static_cast<GPUTextureUsageMask>(GPUTextureUsage::kCopySrc);
  texture_desc.storage_mode = GPUTextureStorageMode::kPrivate;
  auto texture = device.CreateTexture(texture_desc);
  ASSERT_NE(texture, nullptr);

  GPURenderPassDescriptor pass_desc{};
  pass_desc.color_attachment.texture = texture;
  pass_desc.color_attachment.load_op = GPULoadOp::kClear;
  pass_desc.color_attachment.store_op = GPUStoreOp::kStore;
  pass_desc.color_attachment.clear_value = {0.0, 0.0, 0.0, 1.0};
  auto command_buffer = device.CreateCommandBuffer();
  ASSERT_NE(command_buffer, nullptr);
  auto pass = command_buffer->BeginRenderPass(pass_desc);
  ASSERT_NE(pass, nullptr);
  pass->AddCommand(&command);
  pass->EncodeCommands();
  command.storage_bindings.clear();
  EXPECT_TRUE(owner.expired());
  ASSERT_TRUE(command_buffer->Submit());

  auto data = context_impl->ReadPixels(texture);
  ASSERT_NE(data, nullptr);
  ASSERT_GE(data->Size(), kSize * kSize * 4u);
  const auto* pixels = static_cast<const uint8_t*>(data->RawData());
  auto red_at = [pixels](uint32_t x) { return pixels[(kSize * (kSize / 2) + x) * 4]; };
  EXPECT_EQ(red_at(kSize / 4), 255u);
  EXPECT_EQ(red_at(3 * kSize / 4), 255u);
  EXPECT_EQ(red_at(kSize / 2), 0u);
}

}  // namespace
}  // namespace skity
