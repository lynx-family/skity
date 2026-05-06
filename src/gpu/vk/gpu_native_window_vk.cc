// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/macros.hpp>

#if defined(SKITY_WIN) && !defined(VK_USE_PLATFORM_WIN32_KHR)
#define VK_USE_PLATFORM_WIN32_KHR 1
#endif

#if defined(SKITY_ANDROID) && !defined(VK_USE_PLATFORM_ANDROID_KHR)
#define VK_USE_PLATFORM_ANDROID_KHR 1
#endif

#if (defined(SKITY_MACOS) || defined(SKITY_IOS)) && \
    !defined(VK_USE_PLATFORM_METAL_EXT)
#define VK_USE_PLATFORM_METAL_EXT 1
#endif

#if (defined(SKITY_MACOS) || defined(SKITY_IOS)) && \
    !defined(VK_USE_PLATFORM_MACOS_MVK)
#define VK_USE_PLATFORM_MACOS_MVK 1
#endif

#if defined(SKITY_LINUX) && !defined(VK_USE_PLATFORM_WAYLAND_KHR)
#define VK_USE_PLATFORM_WAYLAND_KHR 1
#endif

#if defined(SKITY_LINUX) && !defined(VK_USE_PLATFORM_XCB_KHR)
#define VK_USE_PLATFORM_XCB_KHR 1
#endif

#if defined(SKITY_LINUX) && !defined(VK_USE_PLATFORM_XLIB_KHR)
#define VK_USE_PLATFORM_XLIB_KHR 1
#endif

#include <skity/gpu/gpu_context_vk.hpp>

#include <memory>

#include "src/gpu/vk/gpu_context_impl_vk.hpp"
#include "src/gpu/vk/vulkan_context_state.hpp"
#include "src/logging.hpp"

namespace skity {

namespace {

template <typename Proc>
Proc LoadInstanceProc(PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                      VkInstance instance, const char* proc_name) {
  if (get_instance_proc_addr == nullptr || instance == VK_NULL_HANDLE ||
      proc_name == nullptr) {
    return nullptr;
  }

  return reinterpret_cast<Proc>(get_instance_proc_addr(instance, proc_name));
}

bool IsValidNativeWindowInfo(const VKNativeWindowInfo& info) {
  switch (info.type) {
    case VKNativeWindowType::kWin32:
      return info.handle != nullptr;
    case VKNativeWindowType::kAndroid:
      return info.handle != nullptr;
    case VKNativeWindowType::kMetalLayer:
      return info.handle != nullptr;
    case VKNativeWindowType::kWayland:
      return info.handle != nullptr && info.secondary_handle != nullptr;
    case VKNativeWindowType::kXcb:
      return info.handle != nullptr && info.window_id != 0;
    case VKNativeWindowType::kXlib:
      return info.handle != nullptr && info.window_id != 0;
    case VKNativeWindowType::kInvalid:
    default:
      return false;
  }
}

bool CreateVkSurfaceForWindow(const VulkanContextState* state,
                              const VKNativeWindowInfo& window,
                              VkSurfaceKHR* surface) {
  if (state == nullptr || surface == nullptr) {
    LOGE("Failed to create Vulkan window surface: invalid arguments");
    return false;
  }

  *surface = VK_NULL_HANDLE;

  const auto get_instance_proc_addr = state->GetInstanceProcAddr();
  const VkInstance instance = state->GetInstance();
  if (get_instance_proc_addr == nullptr || instance == VK_NULL_HANDLE) {
    LOGE("Failed to create Vulkan window surface: context has no instance");
    return false;
  }

  if (!IsValidNativeWindowInfo(window)) {
    LOGE("Failed to create Vulkan window surface: invalid native window info");
    return false;
  }

  switch (window.type) {
#if defined(SKITY_WIN) && defined(VK_USE_PLATFORM_WIN32_KHR)
    case VKNativeWindowType::kWin32: {
      auto create_surface = LoadInstanceProc<PFN_vkCreateWin32SurfaceKHR>(
          get_instance_proc_addr, instance, "vkCreateWin32SurfaceKHR");
      if (create_surface == nullptr) {
        LOGE("Failed to load vkCreateWin32SurfaceKHR");
        return false;
      }

      VkWin32SurfaceCreateInfoKHR create_info = {};
      create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
      create_info.hwnd = static_cast<decltype(create_info.hwnd)>(window.handle);
      create_info.hinstance =
          static_cast<decltype(create_info.hinstance)>(window.secondary_handle);
      return create_surface(instance, &create_info, nullptr, surface) ==
                 VK_SUCCESS &&
             *surface != VK_NULL_HANDLE;
    }
#endif
#if defined(SKITY_ANDROID) && defined(VK_USE_PLATFORM_ANDROID_KHR)
    case VKNativeWindowType::kAndroid: {
      auto create_surface = LoadInstanceProc<PFN_vkCreateAndroidSurfaceKHR>(
          get_instance_proc_addr, instance, "vkCreateAndroidSurfaceKHR");
      if (create_surface == nullptr) {
        LOGE("Failed to load vkCreateAndroidSurfaceKHR");
        return false;
      }

      VkAndroidSurfaceCreateInfoKHR create_info = {};
      create_info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
      create_info.window =
          static_cast<decltype(create_info.window)>(window.handle);
      return create_surface(instance, &create_info, nullptr, surface) ==
                 VK_SUCCESS &&
             *surface != VK_NULL_HANDLE;
    }
#endif
#if (defined(SKITY_MACOS) || defined(SKITY_IOS)) && \
    defined(VK_USE_PLATFORM_METAL_EXT)
    case VKNativeWindowType::kMetalLayer: {
      auto create_surface = LoadInstanceProc<PFN_vkCreateMetalSurfaceEXT>(
          get_instance_proc_addr, instance, "vkCreateMetalSurfaceEXT");
      if (create_surface != nullptr) {
        VkMetalSurfaceCreateInfoEXT create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
        create_info.pLayer = window.handle;
        if (create_surface(instance, &create_info, nullptr, surface) ==
                VK_SUCCESS &&
            *surface != VK_NULL_HANDLE) {
          return true;
        }
      }

#if defined(VK_USE_PLATFORM_MACOS_MVK)
      auto create_macos_surface = LoadInstanceProc<PFN_vkCreateMacOSSurfaceMVK>(
          get_instance_proc_addr, instance, "vkCreateMacOSSurfaceMVK");
      if (create_macos_surface == nullptr) {
        LOGE("Failed to load vkCreateMetalSurfaceEXT or vkCreateMacOSSurfaceMVK");
        return false;
      }

      VkMacOSSurfaceCreateInfoMVK create_info = {};
      create_info.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
      create_info.pView =
          window.secondary_handle != nullptr ? window.secondary_handle
                                             : window.handle;
      return create_macos_surface(instance, &create_info, nullptr, surface) ==
                 VK_SUCCESS &&
             *surface != VK_NULL_HANDLE;
#else
      LOGE("Failed to load vkCreateMetalSurfaceEXT");
      return false;
#endif
    }
#endif
#if defined(SKITY_LINUX) && defined(VK_USE_PLATFORM_WAYLAND_KHR)
    case VKNativeWindowType::kWayland: {
      auto create_surface = LoadInstanceProc<PFN_vkCreateWaylandSurfaceKHR>(
          get_instance_proc_addr, instance, "vkCreateWaylandSurfaceKHR");
      if (create_surface == nullptr) {
        LOGE("Failed to load vkCreateWaylandSurfaceKHR");
        return false;
      }

      VkWaylandSurfaceCreateInfoKHR create_info = {};
      create_info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
      create_info.display =
          static_cast<decltype(create_info.display)>(window.handle);
      create_info.surface =
          static_cast<decltype(create_info.surface)>(window.secondary_handle);
      return create_surface(instance, &create_info, nullptr, surface) ==
                 VK_SUCCESS &&
             *surface != VK_NULL_HANDLE;
    }
#endif
#if defined(SKITY_LINUX) && defined(VK_USE_PLATFORM_XCB_KHR)
    case VKNativeWindowType::kXcb: {
      auto create_surface = LoadInstanceProc<PFN_vkCreateXcbSurfaceKHR>(
          get_instance_proc_addr, instance, "vkCreateXcbSurfaceKHR");
      if (create_surface == nullptr) {
        LOGE("Failed to load vkCreateXcbSurfaceKHR");
        return false;
      }

      VkXcbSurfaceCreateInfoKHR create_info = {};
      create_info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
      create_info.connection =
          static_cast<decltype(create_info.connection)>(window.handle);
      create_info.window =
          static_cast<decltype(create_info.window)>(window.window_id);
      return create_surface(instance, &create_info, nullptr, surface) ==
                 VK_SUCCESS &&
             *surface != VK_NULL_HANDLE;
    }
#endif
#if defined(SKITY_LINUX) && defined(VK_USE_PLATFORM_XLIB_KHR)
    case VKNativeWindowType::kXlib: {
      auto create_surface = LoadInstanceProc<PFN_vkCreateXlibSurfaceKHR>(
          get_instance_proc_addr, instance, "vkCreateXlibSurfaceKHR");
      if (create_surface == nullptr) {
        LOGE("Failed to load vkCreateXlibSurfaceKHR");
        return false;
      }

      VkXlibSurfaceCreateInfoKHR create_info = {};
      create_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
      create_info.dpy = static_cast<decltype(create_info.dpy)>(window.handle);
      create_info.window =
          static_cast<decltype(create_info.window)>(window.window_id);
      return create_surface(instance, &create_info, nullptr, surface) ==
                 VK_SUCCESS &&
             *surface != VK_NULL_HANDLE;
    }
#endif
    case VKNativeWindowType::kInvalid:
      LOGE("Failed to create Vulkan window surface: invalid window type");
      return false;
    default:
      LOGE("Failed to create Vulkan window surface: unsupported platform type");
      return false;
  }
}

class GPUNativeWindowVKImpl final : public GPUNativeWindowVK {
 public:
  GPUNativeWindowVKImpl(GPUContext* context, const GPUNativeWindowInfoVK& info)
      : info_(info), context_(context) {}

  ~GPUNativeWindowVKImpl() override {
    presenter_.reset();
    DestroySurface();
  }

  bool Init() {
    if (info_.width == 0 || info_.height == 0) {
      LOGE("Failed to initialize GPUNativeWindowVK: invalid descriptor");
      return false;
    }

    if (!ResolveContext()) {
      return false;
    }

    auto* vk_context = static_cast<GPUContextVK*>(context_);
    state_ = vk_context->GetState();
    if (state_ == nullptr) {
      LOGE("Failed to initialize GPUNativeWindowVK: missing Vulkan state");
      return false;
    }

    destroy_surface_ = LoadInstanceProc<PFN_vkDestroySurfaceKHR>(
        state_->GetInstanceProcAddr(), state_->GetInstance(),
        "vkDestroySurfaceKHR");
    if (destroy_surface_ == nullptr) {
      LOGE("Failed to load vkDestroySurfaceKHR");
      return false;
    }

    if (!CreateVkSurfaceForWindow(state_, info_.native_window, &surface_)) {
      LOGE("Failed to initialize GPUNativeWindowVK: surface creation failed");
      return false;
    }

    if (!RecreatePresenter(info_.width, info_.height)) {
      LOGE("Failed to initialize GPUNativeWindowVK: presenter creation failed");
      return false;
    }

    return true;
  }

  GPUContext* GetContext() const override { return context_; }

  GPUPresenter* GetPresenter() const override { return presenter_.get(); }

  uint32_t GetWidth() const override { return width_; }

  uint32_t GetHeight() const override { return height_; }

  bool Resize(uint32_t width, uint32_t height) override {
    if (width == 0 || height == 0) {
      LOGE("Failed to resize GPUNativeWindowVK: invalid size");
      return false;
    }

    return RecreatePresenter(width, height);
  }

 private:
  bool RecreatePresenter(uint32_t width, uint32_t height) {
    if (context_ == nullptr || surface_ == VK_NULL_HANDLE) {
      return false;
    }

    GPUPresenterDescriptorVK presenter_desc = {};
    presenter_desc.backend = GPUBackendType::kVulkan;
    presenter_desc.width = width;
    presenter_desc.height = height;
    presenter_desc.surface = surface_;
    presenter_desc.present_queue = info_.present_queue;
    presenter_desc.present_queue_family_index =
        info_.present_queue_family_index;
    presenter_desc.min_image_count = info_.min_image_count;
    presenter_desc.format = info_.format;
    presenter_desc.color_space = info_.color_space;
    presenter_desc.present_mode = info_.present_mode;
    presenter_desc.composite_alpha = info_.composite_alpha;
    presenter_desc.pre_transform = info_.pre_transform;
    presenter_desc.clipped = info_.clipped;

    auto presenter = context_->CreatePresenter(&presenter_desc);
    if (presenter == nullptr) {
      LOGE("Failed to create Vulkan presenter for window context");
      return false;
    }

    presenter_ = std::move(presenter);
    width_ = width;
    height_ = height;
    return true;
  }

  void DestroySurface() {
    if (destroy_surface_ == nullptr || state_ == nullptr ||
        surface_ == VK_NULL_HANDLE) {
      return;
    }

    destroy_surface_(state_->GetInstance(), surface_, nullptr);
    surface_ = VK_NULL_HANDLE;
  }

  bool ResolveContext() {
    if (context_ == nullptr) {
      LOGE("Failed to initialize GPUNativeWindowVK: missing GPUContext");
      return false;
    }

    if (context_->GetBackendType() != GPUBackendType::kVulkan) {
      LOGE("Failed to initialize GPUNativeWindowVK: context is not Vulkan");
      return false;
    }

    return true;
  }

  GPUNativeWindowInfoVK info_ = {};
  GPUContext* context_ = nullptr;
  std::unique_ptr<GPUPresenter> presenter_ = nullptr;
  const VulkanContextState* state_ = nullptr;
  PFN_vkDestroySurfaceKHR destroy_surface_ = nullptr;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
};

}  // namespace

std::unique_ptr<GPUNativeWindowVK> CreateGPUNativeWindowVK(
    GPUContext* context, const GPUNativeWindowInfoVK* info) {
  if (info == nullptr) {
    LOGE("CreateGPUNativeWindowVK called with null info");
    return nullptr;
  }

  auto native_window = std::make_unique<GPUNativeWindowVKImpl>(context, *info);
  if (!native_window->Init()) {
    return nullptr;
  }

  return native_window;
}

}  // namespace skity
