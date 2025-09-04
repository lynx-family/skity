// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_context_impl_vk.hpp"

#include <skity/gpu/gpu_context_vk.hpp>
#include <skity/gpu/gpu_render_target.hpp>

#include "src/gpu/vk/gpu_device_vk.hpp"
#include "src/gpu/vk/gpu_surface_vk.hpp"
#include "src/gpu/vk/gpu_texture_vk.hpp"
#include "src/gpu/vk/gpu_window_surface_vk.hpp"
#include "src/gpu/vk/vk_interface.hpp"
#include "src/logging.hpp"

namespace skity {

std::unique_ptr<GPUContext> VkContextCreate() {
  // Use default preferences for backward compatibility
  VkDevicePreferences preferences{};
  return VkContextCreate(preferences);
}

std::unique_ptr<GPUContext> VkContextCreate(
    const VkDevicePreferences& preferences) {
  auto* vk_interface = GetVkInterface();
  if (!vk_interface) {
    LOGE("Failed to initialize Vulkan interface");
    return nullptr;
  }

  auto ctx = std::make_unique<GPUContextImplVk>();
  if (!ctx->InitWithPreferences(preferences)) {
    LOGE("Failed to initialize Vulkan context with preferences");
    return nullptr;
  }

  return ctx;
}

std::unique_ptr<GPUContext> VkContextCreateWithExisting(
    uint64_t instance, uint64_t device, uint64_t queue,
    uint32_t queue_family_index) {
  auto* vk_interface = GetVkInterface();
  if (!vk_interface) {
    LOGE("Failed to initialize Vulkan interface");
    return nullptr;
  }

  auto ctx = std::make_unique<GPUContextImplVk>();
  if (!ctx->InitWithExistingObjects(instance, device, queue,
                                    queue_family_index)) {
    LOGE("Failed to initialize Vulkan context with existing objects");
    return nullptr;
  }

  return ctx;
}

bool IsVulkanAvailable() {
  auto* vk_interface = GetVkInterface();
  return vk_interface != nullptr;
}

const char** VkGetAvailableDevices(uint32_t* device_count) {
  if (!device_count) {
    return nullptr;
  }

  auto* vk_interface = GetVkInterface();
  if (!vk_interface) {
    *device_count = 0;
    return nullptr;
  }

  // TODO: Implement device enumeration
  // For now, return a placeholder indicating implementation needed
  static const char* placeholder_device =
      "Vulkan Device (enumeration not implemented)";
  static const char* device_list[] = {placeholder_device};

  *device_count = 1;
  return device_list;
}

uint64_t VkGetInstance(GPUContext* context) {
  if (!context || context->GetBackendType() != GPUBackendType::kVulkan) {
    return 0;
  }

  auto* vk_interface = GetVkInterface();
  if (!vk_interface) {
    return 0;
  }

  return reinterpret_cast<uint64_t>(vk_interface->GetInstance());
}

GPUContextImplVk::GPUContextImplVk()
    : GPUContextImpl(GPUBackendType::kVulkan) {}

bool GPUContextImplVk::InitWithPreferences(
    const VkDevicePreferences& preferences) {
  // For now, delegate to the base Init() method
  // In a full implementation, this would use the preferences to select
  // specific devices, enable validation layers, etc.
  LOGI(
      "Initializing Vulkan context with preferences (validation: %s, "
      "device_type: %u)",
      preferences.enable_validation ? "enabled" : "disabled",
      preferences.preferred_device_type);

  // TODO: Use preferences to configure device selection
  // TODO: Enable validation layers if requested
  // TODO: Enable custom extensions

  return Init();
}

bool GPUContextImplVk::InitWithExistingObjects(uint64_t instance,
                                               uint64_t device, uint64_t queue,
                                               uint32_t queue_family_index) {
  // For now, log the external objects and delegate to base Init()
  // In a full implementation, this would wrap the existing Vulkan objects
  LOGI(
      "Initializing Vulkan context with existing objects (instance: 0x%llx, "
      "device: 0x%llx, queue: 0x%llx, family: %u)",
      instance, device, queue, queue_family_index);

  // TODO: Wrap existing VkInstance, VkDevice, and VkQueue
  // TODO: Skip device creation and use provided objects
  // TODO: Set up context to use external objects

  return Init();
}

std::unique_ptr<GPUDevice> GPUContextImplVk::CreateGPUDevice() {
  auto device = std::make_unique<GPUDeviceVk>();
  if (!device->Init()) {
    LOGE("Failed to initialize Vulkan device");
    return nullptr;
  }
  return std::move(device);
}

std::unique_ptr<GPUSurface> GPUContextImplVk::CreateSurface(
    GPUSurfaceDescriptor* desc) {
  if (!desc) {
    LOGE("Invalid surface descriptor");
    return nullptr;
  }

  // Cast to Vulkan-specific descriptor (caller guarantees this is correct)
  auto* vk_desc = static_cast<GPUSurfaceDescriptorVk*>(desc);

  LOGI("Creating Vulkan surface: type=%d, size=%dx%d",
       static_cast<int>(vk_desc->surface_type), vk_desc->width,
       vk_desc->height);

  switch (vk_desc->surface_type) {
    case VkSurfaceType::kSwapchain: {
      // Create window surface with swapchain
      if (!vk_desc->native_surface) {
        LOGE("No native surface provided for swapchain creation");
        return nullptr;
      }

      auto window_surface = std::make_unique<GPUWindowSurfaceVk>(
          this, vk_desc->width, vk_desc->height, vk_desc->sample_count,
          vk_desc->content_scale);

      VkSurfaceKHR vk_surface =
          reinterpret_cast<VkSurfaceKHR>(vk_desc->native_surface);
      auto* vk_interface = GetVkInterface();

      if (!window_surface->InitWithSurface(vk_surface, vk_interface)) {
        LOGE("Failed to initialize window surface with swapchain");
        return nullptr;
      }

      LOGI("Successfully created Vulkan window surface with swapchain");
      return window_surface;
    }

    case VkSurfaceType::kImage: {
      // Create surface targeting a specific Vulkan image
      LOGE("VkSurfaceType::kImage not yet implemented");
      return nullptr;
    }

    default:
      LOGE("Unknown Vulkan surface type: %d",
           static_cast<int>(vk_desc->surface_type));
      return nullptr;
  }
}

std::unique_ptr<GPUSurface> GPUContextImplVk::CreateFxaaSurface(
    GPUSurfaceDescriptor* desc) {
  // TODO: Implement Vulkan FXAA surface creation
  LOGE("Vulkan FXAA surface creation not yet implemented");
  return nullptr;
}

std::shared_ptr<GPUTexture> GPUContextImplVk::OnWrapTexture(
    GPUBackendTextureInfo* info, ReleaseCallback callback,
    ReleaseUserData user_data) {
  // TODO: Implement Vulkan texture wrapping
  LOGE("Vulkan texture wrapping not yet implemented");
  return nullptr;
}

std::shared_ptr<Data> GPUContextImplVk::OnReadPixels(
    const std::shared_ptr<GPUTexture>& texture) const {
  // TODO: Implement Vulkan pixel reading
  LOGE("Vulkan pixel reading not yet implemented");
  return nullptr;
}

std::unique_ptr<GPURenderTarget> GPUContextImplVk::OnCreateRenderTarget(
    const GPURenderTargetDescriptor& desc, std::shared_ptr<Texture> texture) {
  if (!texture || !texture->GetGPUTexture()) {
    LOGE("Invalid texture provided for Vulkan render target creation");
    return nullptr;
  }

  auto* gpu_texture_vk =
      static_cast<GPUTextureVk*>(texture->GetGPUTexture().get());
  if (!gpu_texture_vk) {
    LOGE("Failed to cast texture to Vulkan texture");
    return nullptr;
  }

  // Create surface descriptor for Vulkan
  GPUSurfaceDescriptorVk surface_desc{};
  surface_desc.backend = GetBackendType();
  surface_desc.width = desc.width;
  surface_desc.height = desc.height;
  surface_desc.content_scale = 1.0f;
  surface_desc.sample_count = desc.sample_count;
  surface_desc.surface_type = VkSurfaceType::kImage;
  surface_desc.vk_format = VK_FORMAT_R8G8B8A8_UNORM;  // Default format

  // Create the Vulkan surface
  auto surface = GPUSurfaceVk::Create(this, surface_desc);
  if (!surface) {
    LOGE("Failed to create Vulkan surface for render target");
    return nullptr;
  }

  // Set the target texture for the surface
  surface->SetTargetTexture(texture->GetGPUTexture());

  return std::make_unique<GPURenderTarget>(std::move(surface), texture);
}

}  // namespace skity