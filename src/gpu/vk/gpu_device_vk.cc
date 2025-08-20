// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_device_vk.hpp"

#include <algorithm>
#include <set>

#include "src/gpu/vk/gpu_buffer_vk.hpp"
#include "src/gpu/vk/gpu_command_buffer_vk.hpp"
#include "src/gpu/vk/gpu_pipeline_cache_vk.hpp"
#include "src/gpu/vk/gpu_render_pipeline_vk.hpp"
#include "src/gpu/vk/gpu_sampler_vk.hpp"
#include "src/gpu/vk/gpu_shader_function_vk.hpp"
#include "src/gpu/vk/gpu_texture_vk.hpp"
#include "src/gpu/vk/vk_interface.hpp"
#include "src/logging.hpp"

namespace skity {

GPUDeviceVk::GPUDeviceVk() = default;

GPUDeviceVk::~GPUDeviceVk() {
  // Destroy pipeline cache first (before device)
  pipeline_cache_.reset();

  if (allocator_ != VK_NULL_HANDLE) {
    vmaDestroyAllocator(allocator_);
  }

  if (command_pool_ != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device_, command_pool_, nullptr);
  }

  if (default_render_pass_ != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device_, default_render_pass_, nullptr);
  }
  if (depth_stencil_render_pass_ != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device_, depth_stencil_render_pass_, nullptr);
  }

  if (device_ != VK_NULL_HANDLE) {
    vkDestroyDevice(device_, nullptr);
  }
}

bool GPUDeviceVk::Init() {
  auto* vk_interface = GetVkInterface();
  if (!vk_interface) {
    LOGE("Failed to get Vulkan interface");
    return false;
  }

  physical_device_ = vk_interface->SelectBestPhysicalDevice();
  if (physical_device_ == VK_NULL_HANDLE) {
    LOGE("Failed to find suitable physical device");
    return false;
  }

  // Get device properties and features
  vkGetPhysicalDeviceProperties(physical_device_, &device_properties_);
  vkGetPhysicalDeviceFeatures(physical_device_, &device_features_);

  LOGI("Using Vulkan device: {}", device_properties_.deviceName);

  if (!CreateLogicalDevice()) {
    return false;
  }

  if (!CreateCommandPool()) {
    return false;
  }

  if (!CreateVmaAllocator()) {
    return false;
  }

  // Initialize pipeline cache
  pipeline_cache_ = std::make_unique<GPUPipelineCacheVk>(this);
  if (!pipeline_cache_->Initialize()) {
    LOGE("Failed to initialize pipeline cache");
    return false;
  }

  // Create default render pass for pipeline compatibility
  if (!CreateDefaultRenderPass()) {
    LOGE("Failed to create default render pass");
    return false;
  }

  return true;
}

bool GPUDeviceVk::CreateLogicalDevice() {
  queue_family_indices_ = FindQueueFamilies(physical_device_);

  if (!queue_family_indices_.IsComplete()) {
    LOGE("Failed to find required queue families");
    return false;
  }

  std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
  std::set<uint32_t> unique_queue_families = {
      queue_family_indices_.graphics_family,
      queue_family_indices_.present_family};

  float queue_priority = 1.0f;
  for (uint32_t queue_family : unique_queue_families) {
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = queue_family;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;
    queue_create_infos.push_back(queue_create_info);
  }

  VkPhysicalDeviceFeatures device_features{};
  device_features.samplerAnisotropy = VK_TRUE;

  VkDeviceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.queueCreateInfoCount =
      static_cast<uint32_t>(queue_create_infos.size());
  create_info.pQueueCreateInfos = queue_create_infos.data();
  create_info.pEnabledFeatures = &device_features;

  auto* vk_interface = GetVkInterface();
  auto device_extensions =
      vk_interface->GetRequiredDeviceExtensions(physical_device_);
  create_info.enabledExtensionCount =
      static_cast<uint32_t>(device_extensions.size());
  create_info.ppEnabledExtensionNames = device_extensions.data();

  // Note: Device-level validation layers are deprecated since Vulkan 1.0.13
  // Validation layers are now only enabled at instance level
  create_info.enabledLayerCount = 0;
  create_info.ppEnabledLayerNames = nullptr;

  VkResult result =
      vkCreateDevice(physical_device_, &create_info, nullptr, &device_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to create logical device: {}", result);
    return false;
  }

  // Load device functions
  volkLoadDevice(device_);

  // Check for extension support
  CheckExtensionSupport();

  vkGetDeviceQueue(device_, queue_family_indices_.graphics_family, 0,
                   &graphics_queue_);
  vkGetDeviceQueue(device_, queue_family_indices_.present_family, 0,
                   &present_queue_);

  return true;
}

bool GPUDeviceVk::CreateCommandPool() {
  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_info.queueFamilyIndex = queue_family_indices_.graphics_family;

  VkResult result =
      vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to create command pool: {}", result);
    return false;
  }

  return true;
}

bool GPUDeviceVk::CreateVmaAllocator() {
  VmaVulkanFunctions vulkan_functions = {};
  vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

  VmaAllocatorCreateInfo allocator_info = {};
  allocator_info.physicalDevice = physical_device_;
  allocator_info.device = device_;
  allocator_info.instance = GetVkInterface()->GetInstance();
  allocator_info.pVulkanFunctions = &vulkan_functions;

  VkResult result = vmaCreateAllocator(&allocator_info, &allocator_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to create VMA allocator: {}", result);
    return false;
  }

  return true;
}

QueueFamilyIndices GPUDeviceVk::FindQueueFamilies(
    VkPhysicalDevice device) const {
  QueueFamilyIndices indices;

  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                           nullptr);

  std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                           queue_families.data());

  int i = 0;
  for (const auto& queue_family : queue_families) {
    if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphics_family = i;
    }

    if (queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT) {
      indices.compute_family = i;
    }

    if (queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT) {
      indices.transfer_family = i;
    }

    // For now, assume graphics queue can also present
    // In a real implementation, we'd check actual surface support
    if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.present_family = i;
    }

    if (indices.IsComplete()) {
      break;
    }

    i++;
  }

  return indices;
}

bool GPUDeviceVk::CheckDeviceExtensionSupport(VkPhysicalDevice device) const {
  uint32_t extension_count;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count,
                                       nullptr);

  std::vector<VkExtensionProperties> available_extensions(extension_count);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count,
                                       available_extensions.data());

  auto* vk_interface = GetVkInterface();
  auto required_extensions = vk_interface->GetRequiredDeviceExtensions();

  std::set<std::string> required_extensions_set(required_extensions.begin(),
                                                required_extensions.end());

  for (const auto& extension : available_extensions) {
    required_extensions_set.erase(extension.extensionName);
  }

  return required_extensions_set.empty();
}

std::unique_ptr<GPUBuffer> GPUDeviceVk::CreateBuffer(GPUBufferUsageMask usage) {
  auto buffer = std::make_unique<GPUBufferVk>(this, usage);
  return std::move(buffer);
}

std::shared_ptr<GPUShaderFunction> GPUDeviceVk::CreateShaderFunction(
    const GPUShaderFunctionDescriptor& desc) {
  return GPUShaderFunctionVk::Create(this, desc);
}

std::unique_ptr<GPURenderPipeline> GPUDeviceVk::CreateRenderPipeline(
    const GPURenderPipelineDescriptor& desc) {
  if (pipeline_cache_) {
    return pipeline_cache_->GetOrCreatePipeline(desc);
  }
  return GPURenderPipelineVk::Create(this, desc);
}

std::unique_ptr<GPURenderPipeline> GPUDeviceVk::ClonePipeline(
    GPURenderPipeline* base, const GPURenderPipelineDescriptor& desc) {
  if (!base->IsValid()) {
    return nullptr;
  }

  auto variant_desc = desc;

  // Clone the pipeline with new descriptor - this will create a new Vulkan
  // pipeline with the same shaders but different render state
  if (pipeline_cache_) {
    return pipeline_cache_->GetOrCreatePipeline(variant_desc);
  }

  return GPURenderPipelineVk::Create(this, variant_desc);
}

std::shared_ptr<GPUCommandBuffer> GPUDeviceVk::CreateCommandBuffer() {
  auto command_buffer = std::make_shared<GPUCommandBufferVk>(this);
  if (!command_buffer->Initialize()) {
    LOGE("Failed to initialize command buffer");
    return nullptr;
  }
  return command_buffer;
}

std::shared_ptr<GPUSampler> GPUDeviceVk::CreateSampler(
    const GPUSamplerDescriptor& desc) {
  return GPUSamplerVk::Create(this, desc);
}

std::shared_ptr<GPUTexture> GPUDeviceVk::CreateTexture(
    const GPUTextureDescriptor& desc) {
  return GPUTextureVk::Create(this, desc);
}

bool GPUDeviceVk::CanUseMSAA() {
  return true;  // Vulkan generally supports MSAA
}

uint32_t GPUDeviceVk::GetBufferAlignment() {
  return static_cast<uint32_t>(
      device_properties_.limits.minUniformBufferOffsetAlignment);
}

uint32_t GPUDeviceVk::GetMaxTextureSize() {
  return device_properties_.limits.maxImageDimension2D;
}

VkCommandBuffer GPUDeviceVk::BeginSingleTimeCommands() {
  VkCommandBufferAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandPool = command_pool_;
  alloc_info.commandBufferCount = 1;

  VkCommandBuffer command_buffer;
  vkAllocateCommandBuffers(device_, &alloc_info, &command_buffer);

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(command_buffer, &begin_info);

  return command_buffer;
}

void GPUDeviceVk::EndSingleTimeCommands(VkCommandBuffer command_buffer) {
  vkEndCommandBuffer(command_buffer);

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;

  vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
  vkQueueWaitIdle(graphics_queue_);

  vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
}

void GPUDeviceVk::CheckExtensionSupport() {
  // Check for VK_KHR_synchronization2 support
  uint32_t extension_count;
  vkEnumerateDeviceExtensionProperties(physical_device_, nullptr,
                                       &extension_count, nullptr);

  std::vector<VkExtensionProperties> available_extensions(extension_count);
  vkEnumerateDeviceExtensionProperties(
      physical_device_, nullptr, &extension_count, available_extensions.data());

  for (const auto& extension : available_extensions) {
    if (strcmp(extension.extensionName,
               VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME) == 0) {
      synchronization2_supported_ = true;
      break;
    }
  }

  if (synchronization2_supported_) {
    LOGI("VK_KHR_synchronization2 extension is supported");
  } else {
    LOGI(
        "VK_KHR_synchronization2 extension not supported, using legacy "
        "barriers");
  }
}

bool GPUDeviceVk::CreateDefaultRenderPass() {
  // Create a basic render pass compatible with most common surface formats
  // This will be used for pipeline creation to satisfy Vulkan's requirement
  // that pipelines must be compatible with a render pass

  VkAttachmentDescription color_attachment{};
  color_attachment.format =
      VK_FORMAT_B8G8R8A8_UNORM;  // Use linear format to match GL
  LOGI("Default render pass format: %d (VK_FORMAT_B8G8R8A8_UNORM)",
       VK_FORMAT_B8G8R8A8_UNORM);
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_attachment_ref{};
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 1;
  render_pass_info.pAttachments = &color_attachment;
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;
  render_pass_info.dependencyCount = 1;
  render_pass_info.pDependencies = &dependency;

  VkResult result = vkCreateRenderPass(device_, &render_pass_info, nullptr,
                                       &default_render_pass_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to create default render pass: %d", result);
    return false;
  }

  LOGI("Created default render pass for pipeline compatibility");
  return true;
}

bool GPUDeviceVk::CreateDepthStencilRenderPass() {
  // Color attachment (match actual render pass format)
  VkAttachmentDescription color_attachment{};
  color_attachment.format =
      VK_FORMAT_B8G8R8A8_UNORM;  // Match actual render pass format
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  // Depth/stencil attachment for pipeline compatibility
  VkAttachmentDescription depth_stencil_attachment{};
  depth_stencil_attachment.format =
      VK_FORMAT_D24_UNORM_S8_UINT;  // Match actual render pass format (even if
                                    // not supported on this device)
  depth_stencil_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depth_stencil_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_stencil_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_stencil_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_stencil_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_stencil_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_stencil_attachment.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  // References
  VkAttachmentReference color_ref{};
  color_ref.attachment = 0;
  color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_stencil_ref{};
  depth_stencil_ref.attachment = 1;
  depth_stencil_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  // Subpass
  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_ref;
  subpass.pDepthStencilAttachment = &depth_stencil_ref;

  // Dependency (match actual render pass exactly)
  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkAttachmentDescription attachments[] = {color_attachment,
                                           depth_stencil_attachment};

  VkRenderPassCreateInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 2;  // Color + depth/stencil
  render_pass_info.pAttachments = attachments;
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;
  render_pass_info.dependencyCount = 1;
  render_pass_info.pDependencies = &dependency;

  VkResult result = vkCreateRenderPass(device_, &render_pass_info, nullptr,
                                       &depth_stencil_render_pass_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to create depth/stencil render pass: %d", result);
    return false;
  }

  return true;
}

VkRenderPass GPUDeviceVk::GetCompatibleRenderPass(VkFormat format,
                                                  bool needsDepthStencil) {
  if (needsDepthStencil) {
    if (depth_stencil_render_pass_ == VK_NULL_HANDLE) {
      if (!CreateDepthStencilRenderPass()) {
        return default_render_pass_;
      }
    }
    return depth_stencil_render_pass_;
  } else {
    return default_render_pass_;
  }
}

}  // namespace skity