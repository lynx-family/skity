// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/c/skity_presenter.h>
#include <skity/c/skity_presenter_vk.h>

#include <skity/gpu/gpu_context.hpp>
#include <skity/gpu/gpu_context_vk.hpp>
#include <skity/gpu/gpu_presenter.hpp>
#include <skity/gpu/gpu_surface.hpp>
#include <utility>

#include "handle.hpp"

namespace {

skity::GPUNativeWindowVK* NativeWindowOf(skity_presenter handle) {
  auto window =
      skity::capi::get_impl<skity_presenter_s, skity::GPUNativeWindowVK>(
          handle, SKITY_OBJECT_TYPE_PRESENTER);
  return window.get();
}

}  // namespace

extern "C" {

skity_result skity_presenter_create(skity_context context,
                                    const skity_presenter_create_info* info,
                                    skity_presenter* out_presenter) {
  if (info == nullptr || out_presenter == nullptr) {
    return SKITY_ERROR_INVALID_ARGUMENT;
  }
  *out_presenter = nullptr;
  if (info->s_type != SKITY_STRUCTURE_TYPE_PRESENTER_CREATE_INFO) {
    return SKITY_ERROR_INVALID_ARGUMENT;
  }
  auto context_impl = skity::capi::get_impl<skity_context_s, skity::GPUContext>(
      context, SKITY_OBJECT_TYPE_CONTEXT);
  if (context_impl == nullptr) {
    return SKITY_ERROR_INVALID_HANDLE;
  }

  // The p_next extension selects the backend; Vulkan is the only
  // implementation today.
  const auto* ext =
      static_cast<const skity_presenter_create_info_vk*>(info->p_next);
  if (ext == nullptr ||
      ext->s_type != SKITY_STRUCTURE_TYPE_PRESENTER_CREATE_INFO_VK) {
    return SKITY_ERROR_NOT_SUPPORTED;
  }

  skity::GPUNativeWindowInfoVK descriptor{};
  descriptor.native_window.type =
      static_cast<skity::VKNativeWindowType>(ext->native_window.type);
  descriptor.native_window.handle = ext->native_window.handle;
  descriptor.native_window.secondary_handle =
      ext->native_window.secondary_handle;
  descriptor.native_window.window_id = ext->native_window.window_id;
  descriptor.width = info->width;
  descriptor.height = info->height;
  descriptor.present_queue = ext->present_queue;
  descriptor.present_queue_family_index = ext->present_queue_family_index;
  descriptor.min_image_count = ext->min_image_count;
  descriptor.format = ext->format;
  descriptor.color_space = ext->color_space;
  descriptor.present_mode = ext->present_mode;
  descriptor.composite_alpha = ext->composite_alpha;
  descriptor.pre_transform = ext->pre_transform;
  descriptor.clipped = ext->clipped != 0;

  auto window = skity::CreateGPUNativeWindowVK(context_impl.get(), &descriptor);
  if (window == nullptr) {
    return SKITY_ERROR_INITIALIZATION_FAILED;
  }
  std::shared_ptr<skity::GPUNativeWindowVK> impl(window.release());
  *out_presenter = skity::capi::alloc_handle<skity_presenter_s>(
      SKITY_OBJECT_TYPE_PRESENTER, SKITY_HANDLE_OWNING, std::move(impl));
  return SKITY_SUCCESS;
}

void skity_presenter_destroy(skity_presenter window) {
  skity::capi::destroy_handle<skity_presenter_s>(window,
                                                 SKITY_OBJECT_TYPE_PRESENTER);
}

uint32_t skity_presenter_get_width(skity_presenter window) {
  auto* native_window = NativeWindowOf(window);
  return native_window != nullptr ? native_window->GetWidth() : 0u;
}

uint32_t skity_presenter_get_height(skity_presenter window) {
  auto* native_window = NativeWindowOf(window);
  return native_window != nullptr ? native_window->GetHeight() : 0u;
}

skity_result skity_presenter_resize(skity_presenter window, uint32_t width,
                                    uint32_t height) {
  auto* native_window = NativeWindowOf(window);
  if (native_window == nullptr) {
    return SKITY_ERROR_INVALID_HANDLE;
  }
  return native_window->Resize(width, height)
             ? SKITY_SUCCESS
             : SKITY_ERROR_INITIALIZATION_FAILED;
}

skity_result skity_presenter_acquire_next_surface(skity_presenter presenter,
                                                  uint32_t sample_count,
                                                  float content_scale,
                                                  skity_surface* out_surface) {
  if (out_surface == nullptr) {
    return SKITY_ERROR_INVALID_ARGUMENT;
  }
  auto* native_window = NativeWindowOf(presenter);
  if (native_window == nullptr) {
    return SKITY_ERROR_INVALID_HANDLE;
  }
  auto* gpu_presenter = native_window->GetPresenter();
  if (gpu_presenter == nullptr) {
    return SKITY_ERROR_NOT_SUPPORTED;
  }

  skity::GPUSurfaceAcquireDescriptor descriptor{};
  descriptor.sample_count = sample_count ? sample_count : 1;
  descriptor.content_scale = content_scale != 0.f ? content_scale : 1.f;
  auto result = gpu_presenter->AcquireNextSurface(descriptor);
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

skity_result skity_presenter_present(skity_presenter presenter,
                                     skity_surface* surface) {
  if (surface == nullptr) {
    return SKITY_ERROR_INVALID_ARGUMENT;
  }
  auto* native_window = NativeWindowOf(presenter);
  if (native_window == nullptr) {
    return SKITY_ERROR_INVALID_HANDLE;
  }
  auto* gpu_presenter = native_window->GetPresenter();
  auto* wrapper = skity::capi::resolve<skity_surface_s>(
      *surface, SKITY_OBJECT_TYPE_SURFACE);
  if (gpu_presenter == nullptr || wrapper == nullptr) {
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
      gpu_presenter->Present(std::unique_ptr<skity::GPUSurface>(raw_surface));
  if (status == skity::GPUPresenterStatus::kSuccess) {
    return SKITY_SUCCESS;
  }
  return status == skity::GPUPresenterStatus::kNeedRecreate
             ? SKITY_ERROR_NEED_RECREATE
             : SKITY_ERROR_INITIALIZATION_FAILED;
}

}  // extern "C"
