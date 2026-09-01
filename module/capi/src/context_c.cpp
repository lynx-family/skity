// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_context.h>
#include <skity_c/skity_precompile.h>

#include <skity/gpu/gpu_context.hpp>
#include <skity/render/precompile_context.hpp>
#include <utility>

#include "handle.hpp"

#if defined(SKITY_OPENGL)
#include <skity/gpu/gpu_context_gl.hpp>
#endif

#if defined(SKITY_VULKAN)
#include <skity_c/skity_context_vk.h>
#include <vulkan/vulkan.h>

#include <skity/gpu/gpu_context_vk.hpp>
#endif

namespace {

skity::GPUContext* context_of(skity_context handle) {
  auto* w =
      skity::capi::resolve<skity_context_s>(handle, SKITY_OBJECT_TYPE_CONTEXT);
  return w ? static_cast<skity::GPUContext*>(w->impl.get()) : nullptr;
}

}  // namespace

extern "C" {

skity_result skity_context_create_gl(skity_gl_get_proc get_proc,
                                     skity_context* out_context) {
  if (get_proc == nullptr || out_context == nullptr) {
    return SKITY_ERROR_INVALID_ARGUMENT;
  }
#if defined(SKITY_OPENGL)
  std::unique_ptr<skity::GPUContext> ctx =
      skity::GLContextCreate(reinterpret_cast<void*>(get_proc));
  if (ctx == nullptr) {
    return SKITY_ERROR_INITIALIZATION_FAILED;
  }
  std::shared_ptr<skity::GPUContext> impl(ctx.release());
  skity_context_s* w = skity::capi::alloc_handle<skity_context_s>(
      SKITY_OBJECT_TYPE_CONTEXT, SKITY_HANDLE_OWNING, std::move(impl));
  if (w == nullptr) {
    return SKITY_ERROR_OUT_OF_HOST_MEMORY;
  }
  *out_context = w;
  return SKITY_SUCCESS;
#else
  return SKITY_ERROR_NOT_SUPPORTED;
#endif
}

void skity_context_destroy(skity_context context) {
  skity::capi::destroy_handle<skity_context_s>(context,
                                               SKITY_OBJECT_TYPE_CONTEXT);
}

void skity_context_set_error_callback(skity_context context,
                                      skity_gpu_error_callback callback,
                                      void* userdata) {
  auto* c = context_of(context);
  if (c == nullptr) {
    return;
  }
  c->SetErrorCallback(reinterpret_cast<skity::GPUErrorCallback>(callback),
                      userdata);
}

void skity_context_set_enable_merging_draw_call(skity_context context,
                                                uint32_t enable) {
  if (auto* c = context_of(context)) c->SetEnableMergingDrawCall(enable != 0);
}

void skity_context_set_enable_contour_aa(skity_context context,
                                         uint32_t enable) {
  if (auto* c = context_of(context)) c->SetEnableContourAA(enable != 0);
}

void skity_context_set_enable_coverage_aa(skity_context context,
                                          uint32_t enable) {
  if (auto* c = context_of(context)) c->SetEnableCoverageAA(enable != 0);
}

void skity_context_set_conflation_correction(skity_context context,
                                             uint32_t enable) {
  if (auto* c = context_of(context)) c->SetConflationCorrection(enable != 0);
}

void skity_context_set_larger_atlas_mask(skity_context context, uint8_t mask) {
  if (auto* c = context_of(context)) c->SetLargerAtlasMask(mask);
}

void skity_context_set_enable_text_linear_filter(skity_context context,
                                                 uint32_t enable) {
  if (auto* c = context_of(context)) c->SetEnableTextLinearFilter(enable != 0);
}

void skity_context_set_enable_gpu_tessellation(skity_context context,
                                               uint32_t enable) {
  if (auto* c = context_of(context)) c->SetEnableGPUTessellation(enable != 0);
}

void skity_context_set_enable_simple_shape_pipeline(skity_context context,
                                                    uint32_t enable) {
  if (auto* c = context_of(context))
    c->SetEnableSimpleShapePipeline(enable != 0);
}

void skity_context_set_resource_cache_limit(skity_context context,
                                            size_t max_bytes) {
  if (auto* c = context_of(context)) c->SetResourceCacheLimit(max_bytes);
}

skity_precompile_context skity_context_create_precompile_context(
    skity_context context, skity_precompile_color_type color_type,
    uint32_t enable_msaa) {
  auto* c = context_of(context);
  if (c == nullptr) {
    return nullptr;
  }
  auto pc = c->CreatePrecompileContext(
      static_cast<skity::PrecompileColorType>(color_type), enable_msaa != 0);
  if (pc == nullptr) {
    return nullptr;
  }
  std::shared_ptr<skity::PrecompileContext> impl(pc.release());
  return skity::capi::alloc_handle<skity_precompile_context_s>(
      SKITY_OBJECT_TYPE_PRECOMPILE_CONTEXT, SKITY_HANDLE_OWNING,
      std::move(impl));
}

#if defined(SKITY_VULKAN)
skity_result skity_context_create_vk(
    PFN_vkGetInstanceProcAddr get_instance_proc_addr,
    skity_context* out_context) {
  if (get_instance_proc_addr == nullptr || out_context == nullptr) {
    return SKITY_ERROR_INVALID_ARGUMENT;
  }
  std::unique_ptr<skity::GPUContext> ctx =
      skity::CreateGPUContextVK(get_instance_proc_addr);
  if (ctx == nullptr) {
    return SKITY_ERROR_INITIALIZATION_FAILED;
  }
  std::shared_ptr<skity::GPUContext> impl(ctx.release());
  skity_context_s* w = skity::capi::alloc_handle<skity_context_s>(
      SKITY_OBJECT_TYPE_CONTEXT, SKITY_HANDLE_OWNING, std::move(impl));
  if (w == nullptr) {
    return SKITY_ERROR_OUT_OF_HOST_MEMORY;
  }
  *out_context = w;
  return SKITY_SUCCESS;
}

skity_result skity_context_create_vk_ex(
    const skity_context_create_info_vk* info, skity_context* out_context) {
  if (info == nullptr || out_context == nullptr ||
      info->get_instance_proc_addr == nullptr) {
    return SKITY_ERROR_INVALID_ARGUMENT;
  }
  if (info->logical_device != VK_NULL_HANDLE &&
      info->get_device_proc_addr == nullptr) {
    return SKITY_ERROR_INVALID_ARGUMENT;
  }

  skity::GPUContextInfoVK vk_info{};
  vk_info.instance = info->instance;
  vk_info.get_instance_proc_addr = info->get_instance_proc_addr;
  vk_info.enabled_instance_extensions = info->enabled_instance_extensions;
  vk_info.enabled_instance_extension_count =
      info->enabled_instance_extension_count;
  vk_info.enabled_instance_extensions_known =
      info->enabled_instance_extensions_known != 0;
  vk_info.physical_device = info->physical_device;
  vk_info.logical_device = info->logical_device;
  vk_info.get_device_proc_addr = info->get_device_proc_addr;
  vk_info.enabled_device_extensions = info->enabled_device_extensions;
  vk_info.enabled_device_extension_count = info->enabled_device_extension_count;
  vk_info.enabled_device_extensions_known =
      info->enabled_device_extensions_known != 0;
  vk_info.dual_source_blending_enabled =
      info->dual_source_blending_enabled != 0;
  vk_info.graphics_queue = info->graphics_queue;
  vk_info.graphics_queue_family_index = info->graphics_queue_family_index;
  vk_info.compute_queue = info->compute_queue;
  vk_info.compute_queue_family_index = info->compute_queue_family_index;
  vk_info.transfer_queue = info->transfer_queue;
  vk_info.transfer_queue_family_index = info->transfer_queue_family_index;
  vk_info.enable_debug_runtime = info->enable_debug_runtime != 0;

  std::unique_ptr<skity::GPUContext> ctx = skity::CreateGPUContextVK(&vk_info);
  if (ctx == nullptr) {
    return SKITY_ERROR_INITIALIZATION_FAILED;
  }
  std::shared_ptr<skity::GPUContext> impl(ctx.release());
  skity_context_s* w = skity::capi::alloc_handle<skity_context_s>(
      SKITY_OBJECT_TYPE_CONTEXT, SKITY_HANDLE_OWNING, std::move(impl));
  if (w == nullptr) {
    return SKITY_ERROR_OUT_OF_HOST_MEMORY;
  }
  *out_context = w;
  return SKITY_SUCCESS;
}
#endif  // defined(SKITY_VULKAN)

}  // extern "C"
