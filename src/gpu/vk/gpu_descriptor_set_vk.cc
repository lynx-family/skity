// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_descriptor_set_vk.hpp"

#include "src/gpu/vk/gpu_buffer_vk.hpp"
#include "src/gpu/vk/gpu_device_vk.hpp"
#include "src/gpu/vk/gpu_sampler_vk.hpp"
#include "src/gpu/vk/gpu_texture_vk.hpp"
#include "src/gpu/vk/spirv_compiler_vk.hpp"
#include "src/logging.hpp"

namespace skity {

GPUDescriptorSetVk::GPUDescriptorSetVk(GPUDeviceVk* device) : device_(device) {}

GPUDescriptorSetVk::~GPUDescriptorSetVk() { Destroy(); }

bool GPUDescriptorSetVk::Initialize(
    const std::vector<DescriptorBinding>& bindings) {
  if (bindings.empty()) {
    LOGE("No descriptor bindings provided");
    return false;
  }

  bindings_ = bindings;

  if (!CreateDescriptorSetLayout(bindings)) {
    LOGE("Failed to create descriptor set layout");
    return false;
  }

  if (!CreateDescriptorPool()) {
    LOGE("Failed to create descriptor pool");
    Destroy();
    return false;
  }

  if (!AllocateDescriptorSet()) {
    LOGE("Failed to allocate descriptor set");
    Destroy();
    return false;
  }

  initialized_ = true;
  LOGI("Successfully initialized descriptor set with %zu bindings",
       bindings.size());
  return true;
}

bool GPUDescriptorSetVk::CreateDescriptorSetLayout(
    const std::vector<DescriptorBinding>& bindings) {
  std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
  layout_bindings.reserve(bindings.size());

  for (const auto& binding : bindings) {
    VkDescriptorSetLayoutBinding layout_binding = {};
    layout_binding.binding = binding.binding;
    layout_binding.descriptorType = binding.type;
    layout_binding.descriptorCount = binding.count;
    layout_binding.stageFlags = binding.stage_flags;
    layout_binding.pImmutableSamplers = nullptr;

    layout_bindings.push_back(layout_binding);
  }

  VkDescriptorSetLayoutCreateInfo layout_info = {};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = static_cast<uint32_t>(layout_bindings.size());
  layout_info.pBindings = layout_bindings.data();

  VkResult result = vkCreateDescriptorSetLayout(
      device_->GetDevice(), &layout_info, nullptr, &descriptor_set_layout_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to create descriptor set layout: %d", result);
    return false;
  }

  return true;
}

bool GPUDescriptorSetVk::CreateDescriptorPool() {
  // Count descriptor types needed
  std::unordered_map<VkDescriptorType, uint32_t> type_counts;
  for (const auto& binding : bindings_) {
    type_counts[binding.type] += binding.count;
  }

  // Create pool sizes
  std::vector<VkDescriptorPoolSize> pool_sizes;
  pool_sizes.reserve(type_counts.size());

  for (const auto& [type, count] : type_counts) {
    VkDescriptorPoolSize pool_size = {};
    pool_size.type = type;
    pool_size.descriptorCount = count;
    pool_sizes.push_back(pool_size);
  }

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1;
  pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
  pool_info.pPoolSizes = pool_sizes.data();

  VkResult result = vkCreateDescriptorPool(device_->GetDevice(), &pool_info,
                                           nullptr, &descriptor_pool_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to create descriptor pool: %d", result);
    return false;
  }

  return true;
}

bool GPUDescriptorSetVk::AllocateDescriptorSet() {
  VkDescriptorSetAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = descriptor_pool_;
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = &descriptor_set_layout_;

  VkResult result = vkAllocateDescriptorSets(device_->GetDevice(), &alloc_info,
                                             &descriptor_set_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to allocate descriptor set: %d", result);
    return false;
  }

  return true;
}

void GPUDescriptorSetVk::BindBuffer(uint32_t binding, GPUBuffer* buffer,
                                    size_t offset, size_t range) {
  if (!buffer) {
    LOGE("Invalid buffer for binding %u", binding);
    return;
  }

  auto* buffer_vk = static_cast<GPUBufferVk*>(buffer);

  // IMPORTANT: Get the buffer handle at binding time, not creation time
  // This ensures we get the current handle after any uploads that might have
  // recreated the buffer
  VkBuffer vk_buffer = buffer_vk->GetBuffer();

  if (vk_buffer == VK_NULL_HANDLE) {
    LOGE(
        "Buffer VkBuffer handle is null for binding %u - this indicates a "
        "buffer connection issue",
        binding);
    return;
  }

  // Validate the buffer handle is reasonable (not obviously corrupted)
  if ((uintptr_t)vk_buffer < 0x1000) {
    LOGE("Buffer VkBuffer handle %p looks invalid for binding %u",
         (void*)vk_buffer, binding);
    return;
  }

  // Align offset to device requirements (16 bytes for uniform buffers)
  size_t aligned_offset =
      (offset + 15) & ~15;  // Round up to nearest 16-byte boundary

  VkDescriptorBufferInfo buffer_info = {};
  buffer_info.buffer = vk_buffer;
  buffer_info.offset = aligned_offset;
  buffer_info.range = range;

  // Store buffer info and defer write set creation until UpdateDescriptorSet
  buffer_infos_.push_back(buffer_info);

  // Store binding info for later write set creation
  VkWriteDescriptorSet write_set = {};
  write_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write_set.dstSet = descriptor_set_;
  write_set.dstBinding = binding;
  write_set.dstArrayElement = 0;
  write_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  write_set.descriptorCount = 1;
  write_set.pBufferInfo = nullptr;  // Will be set in UpdateDescriptorSet

  write_descriptor_sets_.push_back(write_set);
  LOGI(
      "Bound buffer to binding %u (VkBuffer: %p, offset: %zu->%zu, range: %zu)",
      binding, (void*)vk_buffer, offset, aligned_offset, range);
}

void GPUDescriptorSetVk::BindTexture(uint32_t binding, GPUTexture* texture,
                                     GPUSampler* sampler) {
  if (!texture) {
    LOGE("Invalid texture for binding %u", binding);
    return;
  }

  auto* texture_vk = static_cast<GPUTextureVk*>(texture);

  VkDescriptorImageInfo image_info = {};
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image_info.imageView = texture_vk->GetVkImageView();

  if (sampler) {
    auto* sampler_vk = static_cast<GPUSamplerVk*>(sampler);
    image_info.sampler = sampler_vk->GetVkSampler();
  } else {
    image_info.sampler = VK_NULL_HANDLE;
  }

  image_infos_.push_back(image_info);

  VkWriteDescriptorSet write_set = {};
  write_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write_set.dstSet = descriptor_set_;
  write_set.dstBinding = binding;
  write_set.dstArrayElement = 0;
  write_set.descriptorType = sampler ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                                     : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  write_set.descriptorCount = 1;
  write_set.pImageInfo = &image_infos_.back();

  write_descriptor_sets_.push_back(write_set);
  LOGI("Bound texture to binding %u", binding);
}

void GPUDescriptorSetVk::BindSampler(uint32_t binding, GPUSampler* sampler) {
  if (!sampler) {
    LOGE("Invalid sampler for binding %u", binding);
    return;
  }

  auto* sampler_vk = static_cast<GPUSamplerVk*>(sampler);

  VkDescriptorImageInfo image_info = {};
  image_info.sampler = sampler_vk->GetVkSampler();
  image_info.imageView = VK_NULL_HANDLE;
  image_info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  image_infos_.push_back(image_info);

  VkWriteDescriptorSet write_set = {};
  write_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write_set.dstSet = descriptor_set_;
  write_set.dstBinding = binding;
  write_set.dstArrayElement = 0;
  write_set.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  write_set.descriptorCount = 1;
  write_set.pImageInfo = &image_infos_.back();

  write_descriptor_sets_.push_back(write_set);
  LOGI("Bound sampler to binding %u", binding);
}

bool GPUDescriptorSetVk::UpdateDescriptorSet() {
  if (write_descriptor_sets_.empty()) {
    LOGI("No descriptor set updates needed");
    return true;
  }

  // Fix pointer assignments after all buffers are bound
  size_t buffer_index = 0;
  for (size_t i = 0; i < write_descriptor_sets_.size(); ++i) {
    auto& write_set = write_descriptor_sets_[i];
    if (write_set.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER &&
        buffer_index < buffer_infos_.size()) {
      write_set.pBufferInfo = &buffer_infos_[buffer_index];
      buffer_index++;
    }
  }

  vkUpdateDescriptorSets(device_->GetDevice(),
                         static_cast<uint32_t>(write_descriptor_sets_.size()),
                         write_descriptor_sets_.data(), 0, nullptr);

  LOGI("Updated descriptor set with %zu writes", write_descriptor_sets_.size());
  return true;
}

void GPUDescriptorSetVk::Destroy() {
  VkDevice vk_device = device_->GetDevice();

  if (descriptor_set_ != VK_NULL_HANDLE && descriptor_pool_ != VK_NULL_HANDLE) {
    vkFreeDescriptorSets(vk_device, descriptor_pool_, 1, &descriptor_set_);
    descriptor_set_ = VK_NULL_HANDLE;
  }

  if (descriptor_pool_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(vk_device, descriptor_pool_, nullptr);
    descriptor_pool_ = VK_NULL_HANDLE;
  }

  if (descriptor_set_layout_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(vk_device, descriptor_set_layout_, nullptr);
    descriptor_set_layout_ = VK_NULL_HANDLE;
  }

  write_descriptor_sets_.clear();
  buffer_infos_.clear();
  image_infos_.clear();
  initialized_ = false;
}

// GPUDescriptorManagerVk implementation
GPUDescriptorManagerVk::GPUDescriptorManagerVk(GPUDeviceVk* device)
    : device_(device) {}

GPUDescriptorManagerVk::~GPUDescriptorManagerVk() { descriptor_sets_.clear(); }

std::shared_ptr<GPUDescriptorSetVk> GPUDescriptorManagerVk::CreateDescriptorSet(
    const std::vector<DescriptorBinding>& bindings) {
  auto descriptor_set = std::make_shared<GPUDescriptorSetVk>(device_);

  if (!descriptor_set->Initialize(bindings)) {
    LOGE("Failed to initialize descriptor set");
    return nullptr;
  }

  descriptor_sets_.push_back(descriptor_set);
  LOGI("Created descriptor set, total: %zu", descriptor_sets_.size());
  return descriptor_set;
}

VkDescriptorSetLayout GPUDescriptorManagerVk::CreateDescriptorSetLayout(
    const std::vector<DescriptorBinding>& bindings) {
  // Create descriptor set layout directly without creating a descriptor set
  std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
  layout_bindings.reserve(bindings.size());

  for (const auto& binding : bindings) {
    VkDescriptorSetLayoutBinding layout_binding = {};
    layout_binding.binding = binding.binding;
    layout_binding.descriptorType = binding.type;
    layout_binding.descriptorCount = binding.count;
    layout_binding.stageFlags = binding.stage_flags;
    layout_binding.pImmutableSamplers = nullptr;

    layout_bindings.push_back(layout_binding);
  }

  VkDescriptorSetLayoutCreateInfo layout_info = {};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = static_cast<uint32_t>(layout_bindings.size());
  layout_info.pBindings = layout_bindings.data();

  VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
  VkResult result = vkCreateDescriptorSetLayout(
      device_->GetDevice(), &layout_info, nullptr, &descriptor_layout);
  if (result != VK_SUCCESS) {
    LOGE("Failed to create descriptor set layout: %d", result);
    return VK_NULL_HANDLE;
  }

  LOGI("Created standalone descriptor set layout with %zu bindings",
       bindings.size());
  return descriptor_layout;
}

std::shared_ptr<GPUDescriptorSetVk>
GPUDescriptorManagerVk::CreateDescriptorSetFromReflection(
    const SPIRVReflectionInfo& reflection) {
  auto bindings = ExtractBindingsFromReflection(reflection);
  if (bindings.empty()) {
    LOGW("No descriptor bindings found in shader reflection");
    return nullptr;
  }

  LOGI("Creating descriptor set from reflection with %zu bindings",
       bindings.size());
  return CreateDescriptorSet(bindings);
}

std::vector<DescriptorBinding>
GPUDescriptorManagerVk::ExtractBindingsFromReflection(
    const SPIRVReflectionInfo& reflection) {
  std::vector<DescriptorBinding> bindings;

  // Convert GPU shader stage to Vulkan stage flags
  VkShaderStageFlags stage_flags = 0;
  switch (reflection.stage) {
    case GPUShaderStage::kVertex:
      stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
      break;
    case GPUShaderStage::kFragment:
      stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
      break;
    default:
      stage_flags = VK_SHADER_STAGE_ALL;
      break;
  }

  // Extract uniform buffer bindings
  for (const auto& uniform_binding : reflection.uniform_bindings) {
    DescriptorBinding binding;
    binding.binding = uniform_binding.binding;
    binding.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.count = 1;
    binding.stage_flags = stage_flags;

    bindings.push_back(binding);
    LOGI("Added uniform buffer binding: set=%d, binding=%d, name=%s",
         uniform_binding.set, uniform_binding.binding,
         uniform_binding.name.c_str());
  }

  // Extract texture bindings
  for (const auto& texture_binding : reflection.texture_bindings) {
    DescriptorBinding binding;
    binding.binding = texture_binding.binding;
    binding.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.count = 1;
    binding.stage_flags = stage_flags;

    bindings.push_back(binding);
    LOGI("Added texture binding: set=%d, binding=%d, name=%s",
         texture_binding.set, texture_binding.binding,
         texture_binding.name.c_str());
  }

  // Extract sampler bindings
  for (const auto& sampler_binding : reflection.sampler_bindings) {
    DescriptorBinding binding;
    binding.binding = sampler_binding.binding;
    binding.type = VK_DESCRIPTOR_TYPE_SAMPLER;
    binding.count = 1;
    binding.stage_flags = stage_flags;

    bindings.push_back(binding);
    LOGI("Added sampler binding: set=%d, binding=%d, name=%s",
         sampler_binding.set, sampler_binding.binding,
         sampler_binding.name.c_str());
  }

  return bindings;
}

}  // namespace skity