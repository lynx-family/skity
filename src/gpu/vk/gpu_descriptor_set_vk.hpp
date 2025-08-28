// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_DESCRIPTOR_SET_VK_HPP
#define SRC_GPU_VK_GPU_DESCRIPTOR_SET_VK_HPP

#include <volk.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "src/gpu/gpu_buffer.hpp"
#include "src/gpu/gpu_sampler.hpp"
#include "src/gpu/gpu_texture.hpp"

namespace skity {

class GPUDeviceVk;
struct SPIRVReflectionInfo;

struct DescriptorBinding {
  uint32_t binding;
  VkDescriptorType type;
  uint32_t count;
  VkShaderStageFlags stage_flags;
};

class GPUDescriptorSetVk {
 public:
  GPUDescriptorSetVk(GPUDeviceVk* device);
  ~GPUDescriptorSetVk();

  bool Initialize(const std::vector<DescriptorBinding>& bindings);
  void Destroy();

  // Bind resources to descriptor set
  void BindBuffer(uint32_t binding, GPUBuffer* buffer, size_t offset = 0,
                  size_t range = VK_WHOLE_SIZE);
  void BindTexture(uint32_t binding, GPUTexture* texture,
                   GPUSampler* sampler = nullptr);
  void BindSampler(uint32_t binding, GPUSampler* sampler);

  // Update all bindings and prepare for use
  bool UpdateDescriptorSet();

  VkDescriptorSetLayout GetLayout() const { return descriptor_set_layout_; }
  VkDescriptorSet GetDescriptorSet() const { return descriptor_set_; }

 private:
  bool CreateDescriptorSetLayout(
      const std::vector<DescriptorBinding>& bindings);
  bool CreateDescriptorPool();
  bool AllocateDescriptorSet();

  GPUDeviceVk* device_ = nullptr;
  VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
  VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
  VkDescriptorSet descriptor_set_ = VK_NULL_HANDLE;

  std::vector<DescriptorBinding> bindings_;
  std::vector<VkWriteDescriptorSet> write_descriptor_sets_;
  std::vector<VkDescriptorBufferInfo> buffer_infos_;
  std::vector<VkDescriptorImageInfo> image_infos_;

  bool initialized_ = false;
};

// Utility class for managing multiple descriptor sets
class GPUDescriptorManagerVk {
 public:
  GPUDescriptorManagerVk(GPUDeviceVk* device);
  ~GPUDescriptorManagerVk();

  // Create a descriptor set with specified bindings
  std::shared_ptr<GPUDescriptorSetVk> CreateDescriptorSet(
      const std::vector<DescriptorBinding>& bindings);

  // Create descriptor set layout for pipeline creation
  VkDescriptorSetLayout CreateDescriptorSetLayout(
      const std::vector<DescriptorBinding>& bindings);

  // Create descriptor set from SPIRV reflection information
  std::shared_ptr<GPUDescriptorSetVk> CreateDescriptorSetFromReflection(
      const SPIRVReflectionInfo& reflection);

  // Convert SPIRV reflection to descriptor bindings
  static std::vector<DescriptorBinding> ExtractBindingsFromReflection(
      const SPIRVReflectionInfo& reflection);

 private:
  GPUDeviceVk* device_ = nullptr;
  std::vector<std::shared_ptr<GPUDescriptorSetVk>> descriptor_sets_;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_DESCRIPTOR_SET_VK_HPP