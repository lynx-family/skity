// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_texture.h>

#include <cstring>
#include <skity/gpu/gpu_context.hpp>
#include <skity/gpu/texture.hpp>
#include <skity/io/data.hpp>
#include <skity/io/pixmap.hpp>
#include <vector>

#if defined(SKITY_OPENGL)
#include <skity/gpu/gpu_context_gl.hpp>
#endif

#if defined(SKITY_VULKAN)
#include <skity_c/skity_texture_vk.h>

#include <skity/gpu/gpu_context_vk.hpp>
#endif

#include "handle.hpp"

namespace {

size_t bytes_per_pixel(skity_color_type ct) {
  switch (ct) {
    case SKITY_COLOR_TYPE_A8:
      return 1;
    case SKITY_COLOR_TYPE_RGB565:
      return 2;
    default:
      return 4;
  }
}

skity::Texture* texture_of(skity_texture handle) {
  auto* w =
      skity::capi::resolve<skity_texture_s>(handle, SKITY_OBJECT_TYPE_TEXTURE);
  return w ? static_cast<skity::Texture*>(w->impl.get()) : nullptr;
}

std::shared_ptr<skity::Pixmap> pixmap_from_pixels(
    const void* pixels, size_t row_bytes, uint32_t width, uint32_t height,
    skity_color_type color_type, skity_alpha_type alpha_type) {
  if (color_type == SKITY_COLOR_TYPE_UNKNOWN) {
    return nullptr;
  }
  size_t bpp = bytes_per_pixel(color_type);
  size_t min_row_bytes = static_cast<size_t>(width) * bpp;
  if (row_bytes == 0) {
    row_bytes = min_row_bytes;
  }
  // A row must hold at least one full row of pixels. The GPU upload path
  // (GPUTexture::UploadData) takes a base pointer with no row stride, so a
  // smaller row_bytes would read out of bounds on upload.
  if (row_bytes < min_row_bytes) {
    return nullptr;
  }
  // Repack strided rows into a contiguous buffer: GPU uploads assume
  // tightly-packed rows, so non-tightly-packed sources must be densified to
  // avoid uploading padding bytes as pixel data.
  std::shared_ptr<skity::Data> data;
  if (row_bytes == min_row_bytes) {
    data = skity::Data::MakeWithCopy(pixels, min_row_bytes * height);
  } else {
    std::vector<uint8_t> packed(min_row_bytes * height);
    const auto* src = static_cast<const uint8_t*>(pixels);
    for (uint32_t y = 0; y < height; y++) {
      std::memcpy(packed.data() + y * min_row_bytes, src + y * row_bytes,
                  min_row_bytes);
    }
    data = skity::Data::MakeWithCopy(packed.data(), packed.size());
  }
  if (data == nullptr) {
    return nullptr;
  }
  return std::make_shared<skity::Pixmap>(
      data, min_row_bytes, width, height,
      static_cast<skity::AlphaType>(alpha_type),
      static_cast<skity::ColorType>(color_type));
}

}  // namespace

extern "C" {

skity_texture skity_texture_create(skity_context context,
                                   skity_texture_format format, uint32_t width,
                                   uint32_t height,
                                   skity_alpha_type alpha_type) {
  if (width == 0 || height == 0) {
    return nullptr;
  }
  auto ctx = skity::capi::get_impl<skity_context_s, skity::GPUContext>(
      context, SKITY_OBJECT_TYPE_CONTEXT);
  if (ctx == nullptr) {
    return nullptr;
  }
  auto tex =
      ctx->CreateTexture(static_cast<skity::TextureFormat>(format), width,
                         height, static_cast<skity::AlphaType>(alpha_type));
  if (tex == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_texture_s>(
      SKITY_OBJECT_TYPE_TEXTURE, SKITY_HANDLE_OWNING, std::move(tex));
}

void skity_texture_destroy(skity_texture texture) {
  skity::capi::destroy_handle<skity_texture_s>(texture,
                                               SKITY_OBJECT_TYPE_TEXTURE);
}

void skity_texture_upload(skity_texture texture, const void* pixels,
                          size_t row_bytes, skity_color_type color_type,
                          skity_alpha_type alpha_type) {
  auto* t = texture_of(texture);
  if (t == nullptr || pixels == nullptr) {
    return;
  }
  auto pm = pixmap_from_pixels(pixels, row_bytes, (uint32_t)t->Width(),
                               (uint32_t)t->Height(), color_type, alpha_type);
  if (!pm) {
    return;
  }
  t->UploadImage(pm);
}

void skity_texture_deferred_upload(skity_texture texture, const void* pixels,
                                   size_t row_bytes,
                                   skity_color_type color_type,
                                   skity_alpha_type alpha_type) {
  auto* t = texture_of(texture);
  if (t == nullptr || pixels == nullptr) {
    return;
  }
  auto pm = pixmap_from_pixels(pixels, row_bytes, (uint32_t)t->Width(),
                               (uint32_t)t->Height(), color_type, alpha_type);
  if (!pm) {
    return;
  }
  t->DeferredUploadImage(pm);
}

uint32_t skity_texture_get_width(skity_texture texture) {
  auto* t = texture_of(texture);
  return t ? (uint32_t)t->Width() : 0u;
}

uint32_t skity_texture_get_height(skity_texture texture) {
  auto* t = texture_of(texture);
  return t ? (uint32_t)t->Height() : 0u;
}

skity_texture skity_texture_create_from_backend(
    skity_context context, const skity_backend_texture_info* info,
    skity_texture_release_callback release, void* userdata) {
  if (info == nullptr) {
    return nullptr;
  }
  auto ctx = skity::capi::get_impl<skity_context_s, skity::GPUContext>(
      context, SKITY_OBJECT_TYPE_CONTEXT);
  if (ctx == nullptr) {
    return nullptr;
  }
  auto st = info->p_next != nullptr
                ? *static_cast<const skity_structure_type*>(info->p_next)
                : SKITY_STRUCTURE_TYPE_BACKEND_TEXTURE_INFO;
  std::shared_ptr<skity::Texture> tex;
#if defined(SKITY_OPENGL)
  if (st == SKITY_STRUCTURE_TYPE_BACKEND_TEXTURE_INFO_GL) {
    const auto* gl =
        static_cast<const skity_backend_texture_info_gl*>(info->p_next);
    skity::GPUBackendTextureInfoGL bi{};
    bi.backend = skity::GPUBackendType::kOpenGL;
    bi.width = info->width;
    bi.height = info->height;
    bi.format = static_cast<skity::TextureFormat>(info->format);
    bi.alpha_type = static_cast<skity::AlphaType>(info->alpha_type);
    bi.tex_id = gl->texture_id;
    bi.owned_by_engine = gl->owned_by_engine != 0;
    tex = ctx->WrapTexture(&bi, release, userdata);
  }
#endif

#if defined(SKITY_VULKAN)
  if (st == SKITY_STRUCTURE_TYPE_BACKEND_TEXTURE_INFO_VK) {
    const auto* vk =
        static_cast<const skity_backend_texture_info_vk*>(info->p_next);
    skity::GPUBackendTextureInfoVK bi{};
    bi.backend = skity::GPUBackendType::kVulkan;
    bi.width = info->width;
    bi.height = info->height;
    bi.format = static_cast<skity::TextureFormat>(info->format);
    bi.alpha_type = static_cast<skity::AlphaType>(info->alpha_type);
    bi.image = vk->image;
    bi.image_view = vk->image_view;
    bi.vk_format = vk->vk_format;
    bi.image_usage = vk->image_usage;
    bi.initial_layout = vk->initial_layout;
    bi.final_layout = vk->final_layout;
    bi.owns_image = vk->owns_image != 0;
    bi.owns_image_view = vk->owns_image_view != 0;
    tex = ctx->WrapTexture(&bi, release, userdata);
  }
#endif

  if (tex == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_texture_s>(
      SKITY_OBJECT_TYPE_TEXTURE, SKITY_HANDLE_OWNING, std::move(tex));
}

}  // extern "C"
