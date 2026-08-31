// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_image.h>

#include <cstring>
#include <skity/gpu/gpu_context.hpp>
#include <skity/gpu/texture.hpp>
#include <skity/graphic/image.hpp>
#include <skity/io/data.hpp>
#include <skity/io/pixmap.hpp>
#include <vector>

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

// Build a tightly-packed Pixmap from a (possibly strided) pixel source. The
// GPU upload path (GPUTexture::UploadData) takes a base pointer with no row
// stride, so strided rows are repacked to avoid misaligned / out-of-bounds
// uploads. Returns nullptr on invalid arguments (unknown color type, or a
// row_bytes too small to hold a full row of pixels).
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
  if (row_bytes < min_row_bytes) {
    return nullptr;
  }
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

// Adapts the C callback pair (get_texture / release + userdata) to the C++
// GetPromiseTexture / ReleaseCallback signatures skity expects. The C callback
// returns a skity_texture handle; the thunk turns it into a shared_ptr<Texture>
// (sharing the refcount with the caller's handle).

struct PromiseCtx1 {
  skity_promise_texture_callback c_get;
  skity_promise_release_callback c_release;
  void* c_userdata;
};

struct PromiseCtx2 {
  skity_promise_texture_callback2 c_get;
  skity_promise_release_callback c_release;
  void* c_userdata;
};

std::shared_ptr<skity::Texture> texture_from_handle(skity_texture handle) {
  auto* w =
      skity::capi::resolve<skity_texture_s>(handle, SKITY_OBJECT_TYPE_TEXTURE);
  return w ? std::static_pointer_cast<skity::Texture>(w->impl) : nullptr;
}

std::shared_ptr<skity::Texture> promise_get_thunk1(
    skity::PromiseTextureContext pctx) {
  auto* pc = static_cast<PromiseCtx1*>(pctx);
  if (pc->c_get == nullptr) {
    return nullptr;
  }
  return texture_from_handle(pc->c_get(pc->c_userdata));
}

void promise_release_thunk1(skity::PromiseTextureContext pctx) {
  auto* pc = static_cast<PromiseCtx1*>(pctx);
  if (pc->c_release != nullptr) {
    pc->c_release(pc->c_userdata);
  }
  delete pc;
}

std::shared_ptr<skity::Texture> promise_get_thunk2(
    skity::PromiseTextureContext pctx, skity::GPUContext* gpu_ctx) {
  auto* pc = static_cast<PromiseCtx2*>(pctx);
  if (pc->c_get == nullptr) {
    return nullptr;
  }
  // Wrap the GPUContext as a non-owning handle for the duration of the call.
  skity_context ch = nullptr;
  if (gpu_ctx != nullptr) {
    std::shared_ptr<skity::GPUContext> non_owning(gpu_ctx,
                                                  [](skity::GPUContext*) {});
    ch = skity::capi::alloc_handle<skity_context_s>(SKITY_OBJECT_TYPE_CONTEXT,
                                                    0, std::move(non_owning));
  }
  skity_texture th = pc->c_get(pc->c_userdata, ch);
  if (ch != nullptr) {
    skity::capi::destroy_handle<skity_context_s>(ch, SKITY_OBJECT_TYPE_CONTEXT);
  }
  return texture_from_handle(th);
}

void promise_release_thunk2(skity::PromiseTextureContext pctx) {
  auto* pc = static_cast<PromiseCtx2*>(pctx);
  if (pc->c_release != nullptr) {
    pc->c_release(pc->c_userdata);
  }
  delete pc;
}

}  // namespace

extern "C" {

skity_image skity_image_create_from_pixels(uint32_t width, uint32_t height,
                                           const void* pixels, size_t row_bytes,
                                           skity_alpha_type alpha_type,
                                           skity_color_type color_type) {
  if (width == 0 || height == 0 || pixels == nullptr) {
    return nullptr;
  }
  auto pixmap = pixmap_from_pixels(pixels, row_bytes, width, height, color_type,
                                   alpha_type);
  if (pixmap == nullptr) {
    return nullptr;
  }
  auto image = skity::Image::MakeImage(pixmap, nullptr);
  if (image == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_image_s>(
      SKITY_OBJECT_TYPE_IMAGE, SKITY_HANDLE_OWNING, std::move(image));
}

skity_image skity_image_create_from_pixels_with_context(
    uint32_t width, uint32_t height, const void* pixels, size_t row_bytes,
    skity_alpha_type alpha_type, skity_color_type color_type,
    skity_context context) {
  if (width == 0 || height == 0 || pixels == nullptr) {
    return nullptr;
  }
  auto pixmap = pixmap_from_pixels(pixels, row_bytes, width, height, color_type,
                                   alpha_type);
  if (pixmap == nullptr) {
    return nullptr;
  }
  // NULL context handle resolves to a null GPUContext*, matching the CPU-only
  // skity_image_create_from_pixels path.
  auto ctx = skity::capi::get_impl<skity_context_s, skity::GPUContext>(
      context, SKITY_OBJECT_TYPE_CONTEXT);
  auto image = skity::Image::MakeImage(pixmap, ctx.get());
  if (image == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_image_s>(
      SKITY_OBJECT_TYPE_IMAGE, SKITY_HANDLE_OWNING, std::move(image));
}

void skity_image_destroy(skity_image image) {
  skity::capi::destroy_handle<skity_image_s>(image, SKITY_OBJECT_TYPE_IMAGE);
}

uint32_t skity_image_get_width(skity_image image) {
  auto* w = skity::capi::resolve<skity_image_s>(image, SKITY_OBJECT_TYPE_IMAGE);
  return w ? (uint32_t) static_cast<skity::Image*>(w->impl.get())->Width() : 0u;
}

uint32_t skity_image_get_height(skity_image image) {
  auto* w = skity::capi::resolve<skity_image_s>(image, SKITY_OBJECT_TYPE_IMAGE);
  return w ? (uint32_t) static_cast<skity::Image*>(w->impl.get())->Height()
           : 0u;
}

skity_image skity_image_create_from_texture(skity_texture texture) {
  auto tex = skity::capi::get_impl<skity_texture_s, skity::Texture>(
      texture, SKITY_OBJECT_TYPE_TEXTURE);
  if (tex == nullptr) {
    return nullptr;
  }
  auto image = skity::Image::MakeHWImage(tex);
  if (image == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_image_s>(
      SKITY_OBJECT_TYPE_IMAGE, SKITY_HANDLE_OWNING, std::move(image));
}

skity_image skity_image_create_deferred(skity_texture_format format,
                                        uint32_t width, uint32_t height,
                                        skity_alpha_type alpha_type) {
  if (width == 0 || height == 0) {
    return nullptr;
  }
  auto di = skity::Image::MakeDeferredTextureImage(
      static_cast<skity::TextureFormat>(format), width, height,
      static_cast<skity::AlphaType>(alpha_type));
  if (di == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_image_s>(
      SKITY_OBJECT_TYPE_IMAGE, SKITY_HANDLE_OWNING, std::move(di));
}

void skity_image_deferred_set_texture(skity_image image,
                                      skity_texture texture) {
  auto* w = skity::capi::resolve<skity_image_s>(image, SKITY_OBJECT_TYPE_IMAGE);
  auto tex = skity::capi::get_impl<skity_texture_s, skity::Texture>(
      texture, SKITY_OBJECT_TYPE_TEXTURE);
  if (w == nullptr || tex == nullptr) {
    return;
  }
  // Only valid on a deferred image; see skity_image_create_deferred.
  auto di = std::static_pointer_cast<skity::DeferredTextureImage>(w->impl);
  di->SetTexture(tex);
}

skity_image skity_image_create_promise(
    skity_texture_format format, uint32_t width, uint32_t height,
    skity_alpha_type alpha_type, skity_promise_texture_callback get_texture,
    skity_promise_release_callback release, void* userdata) {
  if (width == 0 || height == 0 || get_texture == nullptr) {
    return nullptr;
  }
  auto* pc = new (std::nothrow) PromiseCtx1{get_texture, release, userdata};
  if (pc == nullptr) {
    return nullptr;
  }
  auto img = skity::Image::MakePromiseTextureImage(
      static_cast<skity::TextureFormat>(format), width, height,
      static_cast<skity::AlphaType>(alpha_type), &promise_get_thunk1,
      &promise_release_thunk1, pc);
  if (img == nullptr) {
    delete pc;
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_image_s>(
      SKITY_OBJECT_TYPE_IMAGE, SKITY_HANDLE_OWNING, std::move(img));
}

skity_image skity_image_create_promise2(
    skity_texture_format format, uint32_t width, uint32_t height,
    skity_alpha_type alpha_type, skity_promise_texture_callback2 get_texture,
    skity_promise_release_callback release, void* userdata) {
  if (width == 0 || height == 0 || get_texture == nullptr) {
    return nullptr;
  }
  auto* pc = new (std::nothrow) PromiseCtx2{get_texture, release, userdata};
  if (pc == nullptr) {
    return nullptr;
  }
  auto img = skity::Image::MakePromiseTextureImage2(
      static_cast<skity::TextureFormat>(format), width, height,
      static_cast<skity::AlphaType>(alpha_type), &promise_get_thunk2,
      &promise_release_thunk2, pc);
  if (img == nullptr) {
    delete pc;
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_image_s>(
      SKITY_OBJECT_TYPE_IMAGE, SKITY_HANDLE_OWNING, std::move(img));
}

skity_pixmap skity_image_read_pixels(skity_image image, skity_context context) {
  auto img = skity::capi::get_impl<skity_image_s, skity::Image>(
      image, SKITY_OBJECT_TYPE_IMAGE);
  if (img == nullptr) {
    return nullptr;
  }
  std::shared_ptr<skity::Pixmap> pm;
  // CPU-backed images (PixmapImage) own their pixels directly and can be read
  // back without a GPUContext; only GPU-backed images require one. This matches
  // the header note that a context is only needed for GPU-backed images.
  auto* pixmap_slot = img->GetPixmap();
  if (pixmap_slot != nullptr && *pixmap_slot) {
    pm = *pixmap_slot;
  } else {
    auto ctx = skity::capi::get_impl<skity_context_s, skity::GPUContext>(
        context, SKITY_OBJECT_TYPE_CONTEXT);
    if (ctx == nullptr) {
      return nullptr;
    }
    pm = img->ReadPixels(ctx.get());
  }
  if (pm == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_pixmap_s>(
      SKITY_OBJECT_TYPE_PIXMAP, SKITY_HANDLE_OWNING, std::move(pm));
}

uint32_t skity_image_scale_pixels(skity_image image, skity_pixmap dst,
                                  skity_context context,
                                  const skity_sampling_options* sampling) {
  auto img = skity::capi::get_impl<skity_image_s, skity::Image>(
      image, SKITY_OBJECT_TYPE_IMAGE);
  auto pm = skity::capi::get_impl<skity_pixmap_s, skity::Pixmap>(
      dst, SKITY_OBJECT_TYPE_PIXMAP);
  if (img == nullptr || pm == nullptr) {
    return 0;
  }
  // NULL context handle yields a null GPUContext*; GPU-backed images will then
  // fail to scale (CPU-backed images scale on the CPU).
  auto ctx = skity::capi::get_impl<skity_context_s, skity::GPUContext>(
      context, SKITY_OBJECT_TYPE_CONTEXT);
  skity::SamplingOptions so{};
  if (sampling != nullptr) {
    so.filter = static_cast<skity::FilterMode>(sampling->filter);
    so.mipmap = static_cast<skity::MipmapMode>(sampling->mipmap);
    so.cubic.B = sampling->cubic_b;
    so.cubic.C = sampling->cubic_c;
  }
  return img->ScalePixels(pm, ctx.get(), so) ? 1u : 0u;
}

}  // extern "C"
