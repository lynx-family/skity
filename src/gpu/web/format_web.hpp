// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_WEB_FORMAT_WEB_HPP
#define SRC_GPU_WEB_FORMAT_WEB_HPP

#include <webgpu/webgpu.h>

#include "src/gpu/gpu_render_pass.hpp"
#include "src/gpu/gpu_render_pipeline.hpp"
#include "src/gpu/gpu_sampler.hpp"
#include "src/gpu/gpu_texture.hpp"

namespace skity {

constexpr WGPUAddressMode ToWGPUAddressMode(GPUAddressMode mode) {
  switch (mode) {
    case GPUAddressMode::kClampToEdge:
      return WGPUAddressMode_ClampToEdge;
    case GPUAddressMode::kMirrorRepeat:
      return WGPUAddressMode_MirrorRepeat;
    case GPUAddressMode::kRepeat:
      return WGPUAddressMode_Repeat;
    default:
      return WGPUAddressMode_ClampToEdge;
  }
}

constexpr WGPUFilterMode ToWGPUFilterMode(GPUFilterMode mode) {
  switch (mode) {
    case GPUFilterMode::kLinear:
      return WGPUFilterMode_Linear;
    case GPUFilterMode::kNearest:
      return WGPUFilterMode_Nearest;
    default:
      return WGPUFilterMode_Linear;
  }
}

constexpr WGPUMipmapFilterMode ToWGPUMipmapFilterMode(GPUMipmapMode mode) {
  switch (mode) {
    case GPUMipmapMode::kNone:
      return WGPUMipmapFilterMode_Undefined;
    case GPUMipmapMode::kLinear:
      return WGPUMipmapFilterMode_Linear;
    case GPUMipmapMode::kNearest:
      return WGPUMipmapFilterMode_Nearest;
    default:
      return WGPUMipmapFilterMode_Linear;
  }
}

constexpr WGPUTextureFormat ToWGPUTextureFormat(GPUTextureFormat format) {
  switch (format) {
    case GPUTextureFormat::kR8Unorm:
      return WGPUTextureFormat_R8Unorm;
    case GPUTextureFormat::kRGB8Unorm:
    case GPUTextureFormat::kRGBA8Unorm:
      return WGPUTextureFormat_RGBA8Unorm;
    case GPUTextureFormat::kBGRA8Unorm:
      return WGPUTextureFormat_BGRA8Unorm;
    case GPUTextureFormat::kStencil8:
      return WGPUTextureFormat_Stencil8;
    case GPUTextureFormat::kDepth24Stencil8:
      return WGPUTextureFormat_Depth24PlusStencil8;
    case GPUTextureFormat::kInvalid:
      return WGPUTextureFormat_Undefined;
    default:
      return WGPUTextureFormat_RGBA8Unorm;
  }
}

constexpr WGPUVertexFormat ToWGPUVertexFormat(GPUVertexFormat format) {
  switch (format) {
    case GPUVertexFormat::kFloat32:
      return WGPUVertexFormat_Float32;
    case GPUVertexFormat::kFloat32x2:
      return WGPUVertexFormat_Float32x2;
    case GPUVertexFormat::kFloat32x3:
      return WGPUVertexFormat_Float32x3;
    case GPUVertexFormat::kFloat32x4:
      return WGPUVertexFormat_Float32x4;
    default:
      return WGPUVertexFormat_Float32;
  }
}

constexpr WGPUCompareFunction ToWGPUCompareFunction(GPUCompareFunction func) {
  switch (func) {
    case GPUCompareFunction::kNever:
      return WGPUCompareFunction_Never;
    case GPUCompareFunction::kLess:
      return WGPUCompareFunction_Less;
    case GPUCompareFunction::kEqual:
      return WGPUCompareFunction_Equal;
    case GPUCompareFunction::kLessEqual:
      return WGPUCompareFunction_LessEqual;
    case GPUCompareFunction::kGreater:
      return WGPUCompareFunction_Greater;
    case GPUCompareFunction::kNotEqual:
      return WGPUCompareFunction_NotEqual;
    case GPUCompareFunction::kGreaterEqual:
      return WGPUCompareFunction_GreaterEqual;
    case GPUCompareFunction::kAlways:
      return WGPUCompareFunction_Always;
    default:
      return WGPUCompareFunction_Never;
  }
}

constexpr WGPUStencilOperation ToWGPUStencilOperation(GPUStencilOperation op) {
  switch (op) {
    case GPUStencilOperation::kKeep:
      return WGPUStencilOperation_Keep;
    case GPUStencilOperation::kZero:
      return WGPUStencilOperation_Zero;
    case GPUStencilOperation::kReplace:
      return WGPUStencilOperation_Replace;
    case GPUStencilOperation::kInvert:
      return WGPUStencilOperation_Invert;
    case GPUStencilOperation::kIncrementClamp:
      return WGPUStencilOperation_IncrementClamp;
    case GPUStencilOperation::kDecrementClamp:
      return WGPUStencilOperation_DecrementClamp;
    case GPUStencilOperation::kIncrementWrap:
      return WGPUStencilOperation_IncrementWrap;
    case GPUStencilOperation::kDecrementWrap:
      return WGPUStencilOperation_DecrementWrap;
    default:
      return WGPUStencilOperation_Keep;
  }
}

constexpr WGPUBlendFactor ToWGPUBlendFactor(GPUBlendFactor factor) {
  switch (factor) {
    case GPUBlendFactor::kZero:
      return WGPUBlendFactor_Zero;
    case GPUBlendFactor::kOne:
      return WGPUBlendFactor_One;
    case GPUBlendFactor::kSrc:
      return WGPUBlendFactor_Src;
    case GPUBlendFactor::kOneMinusSrc:
      return WGPUBlendFactor_OneMinusSrc;
    case GPUBlendFactor::kSrcAlpha:
      return WGPUBlendFactor_SrcAlpha;
    case GPUBlendFactor::kOneMinusSrcAlpha:
      return WGPUBlendFactor_OneMinusSrcAlpha;
    case GPUBlendFactor::kDst:
      return WGPUBlendFactor_Dst;
    case GPUBlendFactor::kOneMinusDst:
      return WGPUBlendFactor_OneMinusDst;
    case GPUBlendFactor::kDstAlpha:
      return WGPUBlendFactor_DstAlpha;
    case GPUBlendFactor::kOneMinusDstAlpha:
      return WGPUBlendFactor_OneMinusDstAlpha;
    case GPUBlendFactor::kSrcAlphaSaturated:
      return WGPUBlendFactor_SrcAlphaSaturated;
    default:
      return WGPUBlendFactor_Undefined;
  }
}

constexpr WGPULoadOp ToWGPULoadOp(GPULoadOp op) {
  switch (op) {
    case GPULoadOp::kLoad:
      return WGPULoadOp_Load;
    case GPULoadOp::kClear:
      return WGPULoadOp_Clear;
    default:
      return WGPULoadOp_Load;
  }
}

constexpr WGPUStoreOp ToWGPUStoreOp(GPUStoreOp op) {
  switch (op) {
    case GPUStoreOp::kStore:
      return WGPUStoreOp_Store;
    case GPUStoreOp::kDiscard:
      return WGPUStoreOp_Discard;
    default:
      return WGPUStoreOp_Store;
  }
}

constexpr WGPUColor ToWGPUColor(GPUColor color) {
  return {color.r, color.g, color.b, color.a};
}

}  // namespace skity

#endif  // SRC_GPU_WEB_FORMAT_WEB_HPP
