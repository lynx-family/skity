// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/c/skity_render_target.h>

#include <memory>
#include <skity/gpu/gpu_context.hpp>
#include <skity/gpu/gpu_render_target.hpp>
#include <skity/render/canvas.hpp>

#include "handle.hpp"

namespace {

skity_render_target_s* target_of(skity_render_target handle) {
  return skity::capi::resolve<skity_render_target_s>(
      handle, SKITY_OBJECT_TYPE_RENDER_TARGET);
}

}  // namespace

extern "C" {

skity_render_target skity_context_create_render_target(
    skity_context context, const skity_render_target_descriptor* desc) {
  if (desc == nullptr || desc->width == 0 || desc->height == 0) {
    return nullptr;
  }
  auto context_impl = skity::capi::get_impl<skity_context_s, skity::GPUContext>(
      context, SKITY_OBJECT_TYPE_CONTEXT);
  if (context_impl == nullptr) {
    return nullptr;
  }
  skity::GPURenderTargetDescriptor descriptor{};
  descriptor.width = desc->width;
  descriptor.height = desc->height;
  descriptor.sample_count = desc->sample_count ? desc->sample_count : 1;
  descriptor.render_options.coverage_aa =
      static_cast<skity::CoverageAAMode>(desc->coverage_aa);
  auto impl = context_impl->CreateRenderTarget(descriptor);
  if (impl == nullptr) {
    return nullptr;
  }
  skity_render_target_s* w = skity::capi::alloc_handle<skity_render_target_s>(
      SKITY_OBJECT_TYPE_RENDER_TARGET, SKITY_HANDLE_OWNING, {});
  if (w == nullptr) {
    return nullptr;
  }
  // The unique owner is the dedicated member; impl stays a non-owning view
  // so get_impl-style access keeps working.
  w->impl = std::shared_ptr<skity::GPURenderTarget>(
      impl.get(), [](skity::GPURenderTarget*) {});
  w->owned_target = std::move(impl);
  return w;
}

skity_canvas skity_render_target_get_canvas(skity_render_target target) {
  auto impl =
      skity::capi::get_impl<skity_render_target_s, skity::GPURenderTarget>(
          target, SKITY_OBJECT_TYPE_RENDER_TARGET);
  if (impl == nullptr) {
    return nullptr;
  }
  skity::Canvas* raw = impl->GetCanvas();
  if (raw == nullptr) {
    return nullptr;
  }
  // Non-owning view: the render target's recorder owns the canvas.
  std::shared_ptr<skity::Canvas> view(raw, [](skity::Canvas*) {});
  return skity::capi::alloc_handle<skity_canvas_s>(SKITY_OBJECT_TYPE_CANVAS, 0,
                                                   std::move(view));
}

uint32_t skity_render_target_get_width(skity_render_target target) {
  auto impl =
      skity::capi::get_impl<skity_render_target_s, skity::GPURenderTarget>(
          target, SKITY_OBJECT_TYPE_RENDER_TARGET);
  return impl ? impl->GetWidth() : 0;
}

uint32_t skity_render_target_get_height(skity_render_target target) {
  auto impl =
      skity::capi::get_impl<skity_render_target_s, skity::GPURenderTarget>(
          target, SKITY_OBJECT_TYPE_RENDER_TARGET);
  return impl ? impl->GetHeight() : 0;
}

skity_image skity_context_make_snapshot(skity_context context,
                                        skity_render_target target) {
  auto context_impl = skity::capi::get_impl<skity_context_s, skity::GPUContext>(
      context, SKITY_OBJECT_TYPE_CONTEXT);
  skity_render_target_s* w = target_of(target);
  if (context_impl == nullptr || w == nullptr || w->owned_target == nullptr) {
    return nullptr;
  }
  // MakeSnapshot consumes the render target (legacy unique_ptr semantics):
  // hand the object over through the dedicated owner member, then reclaim
  // the wrapper struct. The C handle is invalid afterwards.
  std::unique_ptr<skity::GPURenderTarget> consumed = std::move(w->owned_target);
  w->impl = nullptr;
  auto image = context_impl->MakeSnapshot(std::move(consumed));
  delete w;
  if (image == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_image_s>(
      SKITY_OBJECT_TYPE_IMAGE, SKITY_HANDLE_OWNING, std::move(image));
}

void skity_render_target_destroy(skity_render_target target) {
  skity::capi::destroy_handle<skity_render_target_s>(
      target, SKITY_OBJECT_TYPE_RENDER_TARGET);
}

}  // extern "C"
