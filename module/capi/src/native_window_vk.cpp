// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_native_window_vk.h>

#include <skity/gpu/gpu_context.hpp>
#include <skity/gpu/gpu_context_vk.hpp>
#include <skity/gpu/gpu_presenter.hpp>
#include <skity/gpu/gpu_surface.hpp>
#include <utility>

#include "handle.hpp"

namespace {

skity::GPUNativeWindowVK* NativeWindowOf(skity_native_window_vk handle) {
  auto window =
      skity::capi::get_impl<skity_native_window_vk_s, skity::GPUNativeWindowVK>(
          handle, SKITY_OBJECT_TYPE_NATIVE_WINDOW_VK);
  return window.get();
}

}  // namespace

extern "C" {

skity_native_window_vk skity_native_window_create_vk(
    skity_context context, const skity_native_window_create_info_vk* info) {
  if (info == nullptr) {
    return nullptr;
  }
  auto context_impl = skity::capi::get_impl<skity_context_s, skity::GPUContext>(
      context, SKITY_OBJECT_TYPE_CONTEXT);
  if (context_impl == nullptr) {
    return nullptr;
  }

  skity::GPUNativeWindowInfoVK descriptor{};
  descriptor.native_window.type =
      static_cast<skity::VKNativeWindowType>(info->native_window.type);
  descriptor.native_window.handle = info->native_window.handle;
  descriptor.native_window.secondary_handle =
      info->native_window.secondary_handle;
  descriptor.native_window.window_id = info->native_window.window_id;
  descriptor.width = info->width;
  descriptor.height = info->height;
  descriptor.present_queue = info->present_queue;
  descriptor.present_queue_family_index = info->present_queue_family_index;
  descriptor.min_image_count = info->min_image_count;
  descriptor.format = info->format;
  descriptor.color_space = info->color_space;
  descriptor.present_mode = info->present_mode;
  descriptor.composite_alpha = info->composite_alpha;
  descriptor.pre_transform = info->pre_transform;
  descriptor.clipped = info->clipped != 0;

  auto window = skity::CreateGPUNativeWindowVK(context_impl.get(), &descriptor);
  if (window == nullptr) {
    return nullptr;
  }
  std::shared_ptr<skity::GPUNativeWindowVK> impl(window.release());
  return skity::capi::alloc_handle<skity_native_window_vk_s>(
      SKITY_OBJECT_TYPE_NATIVE_WINDOW_VK, SKITY_HANDLE_OWNING, std::move(impl));
}

void skity_native_window_destroy_vk(skity_native_window_vk window) {
  skity::capi::destroy_handle<skity_native_window_vk_s>(
      window, SKITY_OBJECT_TYPE_NATIVE_WINDOW_VK);
}

uint32_t skity_native_window_get_width_vk(skity_native_window_vk window) {
  auto* native_window = NativeWindowOf(window);
  return native_window != nullptr ? native_window->GetWidth() : 0u;
}

uint32_t skity_native_window_get_height_vk(skity_native_window_vk window) {
  auto* native_window = NativeWindowOf(window);
  return native_window != nullptr ? native_window->GetHeight() : 0u;
}

skity_result skity_native_window_resize_vk(skity_native_window_vk window,
                                           uint32_t width, uint32_t height) {
  auto* native_window = NativeWindowOf(window);
  if (native_window == nullptr) {
    return SKITY_ERROR_INVALID_HANDLE;
  }
  return native_window->Resize(width, height)
             ? SKITY_SUCCESS
             : SKITY_ERROR_INITIALIZATION_FAILED;
}

skity_result skity_native_window_acquire_next_surface_vk(
    skity_native_window_vk window, uint32_t sample_count, float content_scale,
    skity_surface* out_surface) {
  if (out_surface == nullptr) {
    return SKITY_ERROR_INVALID_ARGUMENT;
  }
  auto* native_window = NativeWindowOf(window);
  if (native_window == nullptr) {
    return SKITY_ERROR_INVALID_HANDLE;
  }
  auto* presenter = native_window->GetPresenter();
  if (presenter == nullptr) {
    return SKITY_ERROR_NOT_SUPPORTED;
  }

  skity::GPUSurfaceAcquireDescriptor descriptor{};
  descriptor.sample_count = sample_count ? sample_count : 1;
  descriptor.content_scale = content_scale != 0.f ? content_scale : 1.f;
  auto result = presenter->AcquireNextSurface(descriptor);
  if (result.status == skity::GPUPresenterStatus::kNeedRecreate) {
    return SKITY_ERROR_NEED_RECREATE;
  }
  if (result.status != skity::GPUPresenterStatus::kSuccess ||
      result.surface == nullptr) {
    return SKITY_ERROR_INITIALIZATION_FAILED;
  }

  auto* handle = new (std::nothrow) skity_surface_s{};
  if (handle == nullptr) {
    return SKITY_ERROR_OUT_OF_HOST_MEMORY;
  }
  handle->header.type = SKITY_OBJECT_TYPE_SURFACE;
  handle->header.flags = SKITY_HANDLE_OWNING;
  handle->owned_surface = std::move(result.surface);
  handle->impl = std::shared_ptr<skity::GPUSurface>(handle->owned_surface.get(),
                                                    [](skity::GPUSurface*) {});
  *out_surface = handle;
  return SKITY_SUCCESS;
}

skity_result skity_native_window_present_vk(skity_native_window_vk window,
                                            skity_surface* surface) {
  if (surface == nullptr) {
    return SKITY_ERROR_INVALID_ARGUMENT;
  }
  auto* native_window = NativeWindowOf(window);
  if (native_window == nullptr) {
    return SKITY_ERROR_INVALID_HANDLE;
  }
  auto* presenter = native_window->GetPresenter();
  auto* wrapper = skity::capi::resolve<skity_surface_s>(
      *surface, SKITY_OBJECT_TYPE_SURFACE);
  if (presenter == nullptr || wrapper == nullptr) {
    return SKITY_ERROR_INVALID_HANDLE;
  }

  auto* raw_surface = wrapper->owned_surface.release();
  if (raw_surface == nullptr) {
    skity::capi::destroy_handle<skity_surface_s>(*surface,
                                                 SKITY_OBJECT_TYPE_SURFACE);
    *surface = nullptr;
    return SKITY_ERROR_INITIALIZATION_FAILED;
  }

  skity::capi::destroy_handle<skity_surface_s>(*surface,
                                               SKITY_OBJECT_TYPE_SURFACE);
  *surface = nullptr;

  auto status =
      presenter->Present(std::unique_ptr<skity::GPUSurface>(raw_surface));
  if (status == skity::GPUPresenterStatus::kSuccess) {
    return SKITY_SUCCESS;
  }
  return status == skity::GPUPresenterStatus::kNeedRecreate
             ? SKITY_ERROR_NEED_RECREATE
             : SKITY_ERROR_INITIALIZATION_FAILED;
}

}  // extern "C"
