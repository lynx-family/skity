// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/web/gpu_surface_web.hpp"

#include "src/gpu/web/gpu_context_impl_web.hpp"
#include "src/render/hw/web/web_root_layer.hpp"

namespace skity {

GPUSurfaceImplWEB::GPUSurfaceImplWEB(const GPUSurfaceDescriptor& desc,
                                     GPUContextImpl* ctx, WGPUTexture texture)
    : GPUSurfaceImpl(desc, ctx), texture_(texture) {}

GPUSurfaceImplWEB::~GPUSurfaceImplWEB() { wgpuTextureRelease(texture_); }

GPUTextureFormat GPUSurfaceImplWEB::GetGPUFormat() const {
  auto format = wgpuTextureGetFormat(texture_);

  switch (format) {
    case WGPUTextureFormat_BGRA8Unorm:
      return GPUTextureFormat::kBGRA8Unorm;
    case WGPUTextureFormat_RGBA8Unorm:
      return GPUTextureFormat::kRGBA8Unorm;
    default:
      return GPUTextureFormat::kBGRA8Unorm;
  }
}

HWRootLayer* GPUSurfaceImplWEB::OnBeginNextFrame(bool) {
  auto root_layer = GetArenaAllocator()->Make<WebRootLayer>(
      static_cast<uint32_t>(GetWidth() * ContentScale()),
      static_cast<uint32_t>(GetHeight() * ContentScale()),
      Rect::MakeWH(GetWidth(), GetHeight()), GetGPUFormat(), texture_);

  root_layer->SetClearSurface(true);  // do not support retain mode surface
  root_layer->SetSampleCount(GetSampleCount());
  root_layer->SetArenaAllocator(GetArenaAllocator());

  return root_layer;
}

}  // namespace skity
