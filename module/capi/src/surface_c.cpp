// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_surface.h>

#include <skity/gpu/gpu_context.hpp>
#include <skity/gpu/gpu_surface.hpp>
#include <skity/render/canvas.hpp>
#include <utility>

#include "handle.hpp"

#if defined(SKITY_OPENGL)
#include <skity/gpu/gpu_context_gl.hpp>
#endif

#if defined(SKITY_VULKAN)
#include <skity_c/skity_surface_vk.h>

#include <skity/gpu/gpu_context_vk.hpp>
#endif

namespace {

skity_result CreateSurfaceHandle(std::unique_ptr<skity::GPUSurface> surface,
                                 skity_surface* out_surface) {
  if (surface == nullptr) {
    return SKITY_ERROR_INITIALIZATION_FAILED;
  }
  std::shared_ptr<skity::GPUSurface> impl(surface.release());
  skity_surface_s* handle = skity::capi::alloc_handle<skity_surface_s>(
      SKITY_OBJECT_TYPE_SURFACE, SKITY_HANDLE_OWNING, std::move(impl));
  if (handle == nullptr) {
    return SKITY_ERROR_OUT_OF_HOST_MEMORY;
  }
  *out_surface = handle;
  return SKITY_SUCCESS;
}

#if defined(SKITY_OPENGL)
skity_result CreateSurfaceGL(skity::GPUContext* context,
                             const skity_surface_create_info* info,
                             const skity_surface_create_info_gl* extension,
                             skity_surface* out_surface) {
  skity::GPUSurfaceDescriptorGL descriptor{};
  descriptor.backend = skity::GPUBackendType::kOpenGL;
  descriptor.width = info->width;
  descriptor.height = info->height;
  descriptor.sample_count = info->sample_count ? info->sample_count : 1;
  descriptor.content_scale =
      info->content_scale != 0.f ? info->content_scale : 1.f;
  descriptor.surface_type =
      extension->surface_type == SKITY_GL_SURFACE_TYPE_TEXTURE
          ? skity::GLSurfaceType::kTexture
          : skity::GLSurfaceType::kFramebuffer;
  descriptor.gl_id = extension->gl_id;
  descriptor.has_stencil_attachment = extension->has_stencil != 0;
  descriptor.surface_mode =
      static_cast<skity::GLSurfaceMode>(extension->surface_mode);
  descriptor.can_blit_from_target_fbo =
      extension->can_blit_from_target_fbo != 0;
  return CreateSurfaceHandle(context->CreateSurface(&descriptor), out_surface);
}
#endif

#if defined(SKITY_VULKAN)
skity_result CreateSurfaceVK(skity::GPUContext* context,
                             const skity_surface_create_info* info,
                             const skity_surface_create_info_vk* extension,
                             skity_surface* out_surface) {
  skity::GPUSurfaceDescriptorVK descriptor{};
  descriptor.backend = skity::GPUBackendType::kVulkan;
  descriptor.width = info->width;
  descriptor.height = info->height;
  descriptor.sample_count = info->sample_count ? info->sample_count : 1;
  descriptor.content_scale =
      info->content_scale != 0.f ? info->content_scale : 1.f;
  descriptor.surface_type =
      extension->surface_type == SKITY_VK_SURFACE_TYPE_TEXTURE
          ? skity::VKSurfaceType::kTexture
          : skity::VKSurfaceType::kSwapchainImage;
  descriptor.image = extension->image;
  descriptor.image_view = extension->image_view;
  descriptor.format = extension->format;
  descriptor.image_usage = extension->image_usage;
  descriptor.pre_transform = extension->pre_transform;
  descriptor.initial_layout = extension->initial_layout;
  descriptor.final_layout = extension->final_layout;
  descriptor.owns_image = extension->owns_image != 0;
  descriptor.owns_image_view = extension->owns_image_view != 0;

  skity::GPUSurfaceSyncInfoVK sync_info{};
  if (extension->sync_info != nullptr) {
    sync_info.wait_semaphore = extension->sync_info->wait_semaphore;
    sync_info.wait_dst_stage_mask = extension->sync_info->wait_dst_stage_mask;
    sync_info.signal_semaphore = extension->sync_info->signal_semaphore;
    sync_info.signal_fence = extension->sync_info->signal_fence;
    descriptor.sync_info = &sync_info;
  }
  return CreateSurfaceHandle(context->CreateSurface(&descriptor), out_surface);
}
#endif

}  // namespace

extern "C" {

skity_result skity_surface_create(skity_context context,
                                  const skity_surface_create_info* info,
                                  skity_surface* out_surface) {
  if (info == nullptr || out_surface == nullptr) {
    return SKITY_ERROR_INVALID_ARGUMENT;
  }
  auto context_impl = skity::capi::get_impl<skity_context_s, skity::GPUContext>(
      context, SKITY_OBJECT_TYPE_CONTEXT);
  if (context_impl == nullptr) {
    return SKITY_ERROR_INVALID_HANDLE;
  }

  skity_structure_type structure_type =
      SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO;
  if (info->p_next != nullptr) {
    structure_type = *static_cast<const skity_structure_type*>(info->p_next);
  }

#if defined(SKITY_OPENGL)
  if (structure_type == SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO_GL) {
    return CreateSurfaceGL(
        context_impl.get(), info,
        static_cast<const skity_surface_create_info_gl*>(info->p_next),
        out_surface);
  }
#endif
#if defined(SKITY_VULKAN)
  if (structure_type == SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO_VK) {
    return CreateSurfaceVK(
        context_impl.get(), info,
        static_cast<const skity_surface_create_info_vk*>(info->p_next),
        out_surface);
  }
#endif
  return SKITY_ERROR_INVALID_ARGUMENT;
}

void skity_surface_destroy(skity_surface surface) {
  skity::capi::destroy_handle<skity_surface_s>(surface,
                                               SKITY_OBJECT_TYPE_SURFACE);
}

skity_canvas skity_surface_lock_canvas(skity_surface surface, uint32_t clear) {
  auto surface_impl = skity::capi::get_impl<skity_surface_s, skity::GPUSurface>(
      surface, SKITY_OBJECT_TYPE_SURFACE);
  if (surface_impl == nullptr) {
    return nullptr;
  }
  skity::Canvas* raw = surface_impl->LockCanvas(clear != 0);
  if (raw == nullptr) {
    return nullptr;
  }
  std::shared_ptr<skity::Canvas> impl(raw, [](skity::Canvas*) {});
  return skity::capi::alloc_handle<skity_canvas_s>(SKITY_OBJECT_TYPE_CANVAS, 0,
                                                   std::move(impl));
}

void skity_surface_flush(skity_surface surface) {
  auto surface_impl = skity::capi::get_impl<skity_surface_s, skity::GPUSurface>(
      surface, SKITY_OBJECT_TYPE_SURFACE);
  if (surface_impl != nullptr) {
    surface_impl->Flush();
  }
}

skity_pixmap skity_surface_read_pixels(skity_surface surface,
                                       const skity_rect* rect) {
  auto surface_impl = skity::capi::get_impl<skity_surface_s, skity::GPUSurface>(
      surface, SKITY_OBJECT_TYPE_SURFACE);
  if (surface_impl == nullptr) {
    return nullptr;
  }
  skity::Rect region;
  if (rect != nullptr) {
    region = *reinterpret_cast<const skity::Rect*>(rect);
  } else {
    region = skity::Rect::MakeXYWH(
        0.f, 0.f, static_cast<float>(surface_impl->GetWidth()),
        static_cast<float>(surface_impl->GetHeight()));
  }
  auto pixmap = surface_impl->ReadPixels(region);
  if (pixmap == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_pixmap_s>(
      SKITY_OBJECT_TYPE_PIXMAP, SKITY_HANDLE_OWNING, std::move(pixmap));
}

uint32_t skity_surface_get_width(skity_surface surface) {
  auto surface_impl = skity::capi::get_impl<skity_surface_s, skity::GPUSurface>(
      surface, SKITY_OBJECT_TYPE_SURFACE);
  return surface_impl ? surface_impl->GetWidth() : 0u;
}

uint32_t skity_surface_get_height(skity_surface surface) {
  auto surface_impl = skity::capi::get_impl<skity_surface_s, skity::GPUSurface>(
      surface, SKITY_OBJECT_TYPE_SURFACE);
  return surface_impl ? surface_impl->GetHeight() : 0u;
}

float skity_surface_get_content_scale(skity_surface surface) {
  auto surface_impl = skity::capi::get_impl<skity_surface_s, skity::GPUSurface>(
      surface, SKITY_OBJECT_TYPE_SURFACE);
  return surface_impl ? surface_impl->ContentScale() : 1.f;
}

#if defined(SKITY_VULKAN)
void skity_surface_add_external_wait_semaphore_vk(skity_surface surface,
                                                  skity_semaphore semaphore) {
  auto surface_impl = skity::capi::get_impl<skity_surface_s, skity::GPUSurface>(
      surface, SKITY_OBJECT_TYPE_SURFACE);
  auto semaphore_impl =
      skity::capi::get_impl<skity_semaphore_s, skity::GPUSemaphore>(
          semaphore, SKITY_OBJECT_TYPE_SEMAPHORE);
  if (surface_impl != nullptr && semaphore_impl != nullptr) {
    surface_impl->AddExternalWaitSemaphore(semaphore_impl);
  }
}
#endif

}  // extern "C"
