// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/mtl/formats_mtl.h"
#include "src/gpu/gpu_render_pass.hpp"
#include "src/gpu/mtl/gpu_texture_mtl.h"

#include <skity/macros.hpp>

#include <cstddef>

namespace skity {

MTLStorageMode ToMTLStorageMode(GPUTextureStorageMode mode, bool supports_memoryless) {
  switch (mode) {
    case GPUTextureStorageMode::kHostVisible:
#ifdef SKITY_IOS
      return MTLStorageModeShared;
#else
      return MTLStorageModeManaged;
#endif
    case GPUTextureStorageMode::kPrivate:
      return MTLStorageModePrivate;
    case skity::GPUTextureStorageMode::kMemoryless:
      if (supports_memoryless) {
        // Device may support but the OS has not been updated.
        if (@available(macOS 11.0, *)) {
          return MTLStorageModeMemoryless;
        } else {
          return MTLStorageModePrivate;
        }
      } else {
        return MTLStorageModePrivate;
      }
  }
}

MTLTextureDescriptor* ToMTLTextureDescriptor(const GPUTextureDescriptor& desc,
                                             bool supports_memoryless) {
  MTLTextureDescriptor* mtl_desc = [[MTLTextureDescriptor alloc] init];
  mtl_desc.textureType = desc.sample_count > 1 ? MTLTextureType2DMultisample : MTLTextureType2D;
  mtl_desc.pixelFormat = ToMTLTextureFormat(desc.format);
  mtl_desc.sampleCount = static_cast<NSUInteger>(desc.sample_count);
  mtl_desc.width = desc.width;
  mtl_desc.height = desc.height;
  mtl_desc.mipmapLevelCount = desc.mip_level_count;
  mtl_desc.usage = MTLTextureUsageUnknown;
  if (desc.usage & static_cast<GPUTextureUsageMask>(GPUTextureUsage::kStorageBinding)) {
    mtl_desc.usage |= MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
  }
  if (desc.usage & static_cast<GPUTextureUsageMask>(GPUTextureUsage::kTextureBinding)) {
    mtl_desc.usage |= MTLTextureUsageShaderRead;
  }
  if (desc.usage & static_cast<GPUTextureUsageMask>(GPUTextureUsage::kRenderAttachment)) {
    mtl_desc.usage |= MTLTextureUsageRenderTarget;
  }

  mtl_desc.storageMode = ToMTLStorageMode(desc.storage_mode, supports_memoryless);

  return mtl_desc;
}

static MTLStencilDescriptor* ToMTLStencilDescriptor(const GPUStencilFaceState& face_state) {
  MTLStencilDescriptor* stencilDesc = [[MTLStencilDescriptor alloc] init];
  stencilDesc.stencilCompareFunction = ToMTLCompareFunction(face_state.compare);
  stencilDesc.stencilFailureOperation = ToMTLStencilOperation(face_state.fail_op);
  stencilDesc.depthFailureOperation = ToMTLStencilOperation(face_state.depth_fail_op);
  stencilDesc.depthStencilPassOperation = ToMTLStencilOperation(face_state.pass_op);
  stencilDesc.readMask = face_state.stencil_read_mask;
  stencilDesc.writeMask = face_state.stencil_write_mask;
  return stencilDesc;
}

MTLDepthStencilDescriptor* ToMTLDepthStencilDescriptor(
    const GPUDepthStencilState& depth_stencil_state) {
  MTLDepthStencilDescriptor* depthStencilDesc = [[MTLDepthStencilDescriptor alloc] init];
  if (depth_stencil_state.enable_stencil) {
    depthStencilDesc.frontFaceStencil =
        ToMTLStencilDescriptor(depth_stencil_state.stencil_state.front);
    depthStencilDesc.backFaceStencil =
        ToMTLStencilDescriptor(depth_stencil_state.stencil_state.back);
  }

  if (depth_stencil_state.enable_depth) {
    depthStencilDesc.depthCompareFunction =
        ToMTLCompareFunction(depth_stencil_state.depth_state.compare);
    depthStencilDesc.depthWriteEnabled = depth_stencil_state.depth_state.enableWrite;
  }

  return depthStencilDesc;
}

MTLVertexDescriptor* ToMTLVertexDescriptor(const std::vector<GPUVertexBufferLayout>& buffers) {
  MTLVertexDescriptor* vertex_desc = [MTLVertexDescriptor vertexDescriptor];
  for (size_t i = 0; i < buffers.size(); i++) {
    auto& layout = buffers[i];
    vertex_desc.layouts[i].stride = layout.array_stride;
    vertex_desc.layouts[i].stepRate = 1;
    vertex_desc.layouts[i].stepFunction = ToMTLVertexStepFunction(layout.step_mode);
    for (size_t j = 0; j < layout.attributes.size(); j++) {
      auto& attribute = layout.attributes[j];
      auto location = attribute.shader_location;
      vertex_desc.attributes[location].offset = attribute.offset;
      vertex_desc.attributes[location].format = ToMTLVertexFormat(attribute.format);
      vertex_desc.attributes[location].bufferIndex = i;
    }
  }

  return vertex_desc;
}

constexpr MTLSamplerMinMagFilter ToMTLSamplerMinMagFilter(GPUFilterMode filter) {
  switch (filter) {
    case GPUFilterMode::kNearest:
      return MTLSamplerMinMagFilterNearest;
    case GPUFilterMode::kLinear:
      return MTLSamplerMinMagFilterLinear;
  }
}

constexpr MTLSamplerMipFilter ToMTLSamplerMipFilter(GPUMipmapMode mipmap) {
  switch (mipmap) {
    case GPUMipmapMode::kNone:
      return MTLSamplerMipFilterNotMipmapped;
    case GPUMipmapMode::kNearest:
      return MTLSamplerMipFilterNearest;
    case GPUMipmapMode::kLinear:
      return MTLSamplerMipFilterLinear;
  }
}

constexpr MTLSamplerAddressMode ToMTLSamplerAddressMode(GPUAddressMode address) {
  switch (address) {
    case GPUAddressMode::kClampToEdge:
      return MTLSamplerAddressModeClampToEdge;
    case GPUAddressMode::kRepeat:
      return MTLSamplerAddressModeRepeat;
    case GPUAddressMode::kMirrorRepeat:
      return MTLSamplerAddressModeMirrorRepeat;
  }
}

MTLSamplerDescriptor* ToMTLSamplerDescriptor(const GPUSamplerDescriptor& desc) {
  MTLSamplerDescriptor* mtl_desc = [[MTLSamplerDescriptor alloc] init];
  mtl_desc.minFilter = ToMTLSamplerMinMagFilter(desc.min_filter);
  mtl_desc.magFilter = ToMTLSamplerMinMagFilter(desc.mag_filter);
  mtl_desc.mipFilter = ToMTLSamplerMipFilter(desc.mipmap_filter);
  mtl_desc.sAddressMode = ToMTLSamplerAddressMode(desc.address_mode_u);
  mtl_desc.tAddressMode = ToMTLSamplerAddressMode(desc.address_mode_v);
  mtl_desc.rAddressMode = ToMTLSamplerAddressMode(desc.address_mode_w);
  return mtl_desc;
}

constexpr MTLLoadAction ToMTLLoadAction(GPULoadOp op, bool need_resolve) {
  switch (op) {
    case GPULoadOp::kDontCare:
      return MTLLoadActionDontCare;
    case GPULoadOp::kLoad:
      return need_resolve ? MTLLoadActionClear : MTLLoadActionLoad;
    case GPULoadOp::kClear:
      return MTLLoadActionClear;
  }

  return MTLLoadActionDontCare;
}

constexpr MTLStoreAction ToMTLStoreAction(GPUStoreOp op, bool need_resolve) {
  switch (op) {
    case GPUStoreOp::kStore:
      return need_resolve ? MTLStoreActionMultisampleResolve : MTLStoreActionStore;
    case GPUStoreOp::kDiscard:
      return need_resolve ? MTLStoreActionMultisampleResolve : MTLStoreActionDontCare;
  }

  return MTLStoreActionDontCare;
}

constexpr MTLClearColor ToMTLClearColor(GPUColor clear_color) {
  return MTLClearColorMake(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
}

MTLRenderPassDescriptor* ToMTLRenderPassDescriptor(const GPURenderPassDescriptor& desc) {
  MTLRenderPassDescriptor* mtl_render_pass_desc = [MTLRenderPassDescriptor renderPassDescriptor];

  mtl_render_pass_desc.colorAttachments[0].texture =
      static_cast<GPUTextureMTL&>(*desc.color_attachment.texture).GetMTLTexture();
  bool need_resolve = desc.color_attachment.resolve_texture != nullptr;
  if (need_resolve) {
    mtl_render_pass_desc.colorAttachments[0].resolveTexture =
        static_cast<GPUTextureMTL&>(*desc.color_attachment.resolve_texture).GetMTLTexture();
  }
  mtl_render_pass_desc.colorAttachments[0].loadAction =
      ToMTLLoadAction(desc.color_attachment.load_op, need_resolve);
  mtl_render_pass_desc.colorAttachments[0].storeAction =
      ToMTLStoreAction(desc.color_attachment.store_op, need_resolve);
  mtl_render_pass_desc.colorAttachments[0].clearColor =
      ToMTLClearColor(desc.color_attachment.clear_value);

  if (desc.stencil_attachment.texture != nullptr) {
    bool need_resolve = desc.stencil_attachment.resolve_texture != nullptr;

    mtl_render_pass_desc.stencilAttachment.texture =
        static_cast<GPUTextureMTL&>(*desc.stencil_attachment.texture).GetMTLTexture();
    if (need_resolve) {
      mtl_render_pass_desc.stencilAttachment.resolveTexture =
          static_cast<GPUTextureMTL&>(*desc.stencil_attachment.resolve_texture).GetMTLTexture();
    }
    mtl_render_pass_desc.stencilAttachment.loadAction =
        ToMTLLoadAction(desc.stencil_attachment.load_op, need_resolve);
    mtl_render_pass_desc.stencilAttachment.storeAction =
        ToMTLStoreAction(desc.stencil_attachment.store_op, need_resolve);
    mtl_render_pass_desc.stencilAttachment.clearStencil = desc.stencil_attachment.clear_value;
  }

  if (desc.depth_attachment.texture) {
    bool need_resolve = desc.depth_attachment.resolve_texture != nullptr;

    mtl_render_pass_desc.depthAttachment.texture =
        static_cast<GPUTextureMTL*>(desc.depth_attachment.texture.get())->GetMTLTexture();
    if (need_resolve) {
      mtl_render_pass_desc.depthAttachment.resolveTexture =
          static_cast<GPUTextureMTL*>(desc.depth_attachment.resolve_texture.get())->GetMTLTexture();
    }

    mtl_render_pass_desc.depthAttachment.loadAction =
        ToMTLLoadAction(desc.depth_attachment.load_op, need_resolve);
    mtl_render_pass_desc.depthAttachment.storeAction =
        ToMTLStoreAction(desc.depth_attachment.store_op, need_resolve);
    mtl_render_pass_desc.depthAttachment.clearDepth = desc.depth_attachment.clear_value;
  }

  return mtl_render_pass_desc;
}

TextureFormat ToTextureFormat(MTLPixelFormat format) {
  switch (format) {
    case MTLPixelFormatRGBA8Unorm:
      return TextureFormat::kRGBA;
    case MTLPixelFormatRGBA8Unorm_sRGB:
      return TextureFormat::kRGB;
    case MTLPixelFormatStencil8:
      return TextureFormat::kS;
    case MTLPixelFormatBGRA8Unorm:
      return TextureFormat::kBGRA;
    case MTLPixelFormatR8Unorm:
      return TextureFormat::kR;
    case MTLPixelFormatB5G6R5Unorm:
      return TextureFormat::kRGB565;
    default:
      return TextureFormat::kRGBA;
  }
}

GPUTextureFormat ToGPUTextureFormat(MTLPixelFormat format) {
  switch (format) {
    case MTLPixelFormatRGBA8Unorm:
      return GPUTextureFormat::kRGBA8Unorm;
    case MTLPixelFormatRGBA8Unorm_sRGB:
      return GPUTextureFormat::kRGB8Unorm;
    case MTLPixelFormatStencil8:
      return GPUTextureFormat::kStencil8;
    case MTLPixelFormatBGRA8Unorm:
      return GPUTextureFormat::kBGRA8Unorm;
    case MTLPixelFormatR8Unorm:
      return GPUTextureFormat::kR8Unorm;
    case MTLPixelFormatB5G6R5Unorm:
      return GPUTextureFormat::kRGB565Unorm;
    default:
      return GPUTextureFormat::kRGBA8Unorm;
  }
}

}  // namespace skity
