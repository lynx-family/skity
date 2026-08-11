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

extern "C" {

skity_result skity_surface_create(skity_context context,
                                  const skity_surface_create_info* info,
                                  skity_surface* out_surface) {
  if (info == nullptr || out_surface == nullptr) {
    return SKITY_ERROR_INVALID_ARGUMENT;
  }
  auto ctx = skity::capi::get_impl<skity_context_s, skity::GPUContext>(
      context, SKITY_OBJECT_TYPE_CONTEXT);
  if (ctx == nullptr) {
    return SKITY_ERROR_INVALID_HANDLE;
  }

#if defined(SKITY_OPENGL)
  // Locate the GL backend extension in the p_next chain.
  const skity_surface_create_info_gl* gl_ext = nullptr;
  if (info->p_next != nullptr) {
    auto s_type = *static_cast<const skity_structure_type*>(info->p_next);
    if (s_type == SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO_GL) {
      gl_ext = static_cast<const skity_surface_create_info_gl*>(info->p_next);
    }
  }
  if (gl_ext == nullptr) {
    return SKITY_ERROR_INVALID_ARGUMENT;
  }

  skity::GPUSurfaceDescriptorGL desc{};
  desc.backend = skity::GPUBackendType::kOpenGL;
  desc.width = info->width;
  desc.height = info->height;
  desc.sample_count = info->sample_count ? info->sample_count : 1;
  desc.content_scale = info->content_scale != 0.f ? info->content_scale : 1.f;
  desc.surface_type = (gl_ext->surface_type == SKITY_GL_SURFACE_TYPE_TEXTURE)
                          ? skity::GLSurfaceType::kTexture
                          : skity::GLSurfaceType::kFramebuffer;
  desc.gl_id = gl_ext->gl_id;
  desc.has_stencil_attachment = gl_ext->has_stencil != 0;
  desc.surface_mode = static_cast<skity::GLSurfaceMode>(gl_ext->surface_mode);
  desc.can_blit_from_target_fbo = gl_ext->can_blit_from_target_fbo != 0;

  std::unique_ptr<skity::GPUSurface> surf = ctx->CreateSurface(&desc);
  if (surf == nullptr) {
    return SKITY_ERROR_INITIALIZATION_FAILED;
  }
  std::shared_ptr<void> impl(surf.release());
  skity_surface_s* w = skity::capi::alloc_handle<skity_surface_s>(
      SKITY_OBJECT_TYPE_SURFACE, SKITY_HANDLE_OWNING, std::move(impl));
  if (w == nullptr) {
    return SKITY_ERROR_OUT_OF_HOST_MEMORY;
  }
  *out_surface = w;
  return SKITY_SUCCESS;
#else
  return SKITY_ERROR_NOT_SUPPORTED;
#endif
}

void skity_surface_destroy(skity_surface surface) {
  skity::capi::destroy_handle<skity_surface_s>(surface,
                                               SKITY_OBJECT_TYPE_SURFACE);
}

skity_canvas skity_surface_lock_canvas(skity_surface surface, uint32_t clear) {
  auto surf = skity::capi::get_impl<skity_surface_s, skity::GPUSurface>(
      surface, SKITY_OBJECT_TYPE_SURFACE);
  if (surf == nullptr) {
    return nullptr;
  }
  // The Canvas is owned by the surface; wrap it with a no-op deleter.
  skity::Canvas* raw = surf->LockCanvas(clear != 0);
  if (raw == nullptr) {
    return nullptr;
  }
  std::shared_ptr<void> impl(raw, [](void*) {});
  return skity::capi::alloc_handle<skity_canvas_s>(SKITY_OBJECT_TYPE_CANVAS, 0,
                                                   std::move(impl));
}

void skity_surface_flush(skity_surface surface) {
  auto surf = skity::capi::get_impl<skity_surface_s, skity::GPUSurface>(
      surface, SKITY_OBJECT_TYPE_SURFACE);
  if (surf != nullptr) {
    surf->Flush();
  }
}

skity_pixmap skity_surface_read_pixels(skity_surface surface,
                                       const skity_rect* rect) {
  auto surf = skity::capi::get_impl<skity_surface_s, skity::GPUSurface>(
      surface, SKITY_OBJECT_TYPE_SURFACE);
  if (surf == nullptr) {
    return nullptr;
  }
  skity::Rect region;
  if (rect != nullptr) {
    region = *reinterpret_cast<const skity::Rect*>(rect);
  } else {
    region = skity::Rect::MakeXYWH(0.f, 0.f, (float)surf->GetWidth(),
                                   (float)surf->GetHeight());
  }
  auto pm = surf->ReadPixels(region);
  if (pm == nullptr) {
    return nullptr;
  }
  std::shared_ptr<void> impl(std::move(pm));
  return skity::capi::alloc_handle<skity_pixmap_s>(
      SKITY_OBJECT_TYPE_PIXMAP, SKITY_HANDLE_OWNING, std::move(impl));
}

uint32_t skity_surface_get_width(skity_surface surface) {
  auto surf = skity::capi::get_impl<skity_surface_s, skity::GPUSurface>(
      surface, SKITY_OBJECT_TYPE_SURFACE);
  return surf ? surf->GetWidth() : 0u;
}

uint32_t skity_surface_get_height(skity_surface surface) {
  auto surf = skity::capi::get_impl<skity_surface_s, skity::GPUSurface>(
      surface, SKITY_OBJECT_TYPE_SURFACE);
  return surf ? surf->GetHeight() : 0u;
}

float skity_surface_get_content_scale(skity_surface surface) {
  auto surf = skity::capi::get_impl<skity_surface_s, skity::GPUSurface>(
      surface, SKITY_OBJECT_TYPE_SURFACE);
  return surf ? surf->ContentScale() : 1.f;
}

}  // extern "C"
