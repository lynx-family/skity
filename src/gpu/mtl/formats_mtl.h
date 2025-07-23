// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_MTL_FORMATS_MTL_HPP
#define SRC_GPU_MTL_FORMATS_MTL_HPP

#import <Metal/Metal.h>

#include <skity/gpu/texture.hpp>

#include "src/gpu/gpu_render_pass.hpp"
#include "src/gpu/gpu_render_pipeline.hpp"
#include "src/gpu/gpu_sampler.hpp"
#include "src/gpu/gpu_texture.hpp"

namespace skity {

NS_ASSUME_NONNULL_BEGIN

MTLTextureDescriptor* ToMTLTextureDescriptor(const GPUTextureDescriptor& desc,
                                             bool supports_memoryless);

constexpr MTLPixelFormat ToMTLTextureFormat(GPUTextureFormat format) {
  switch (format) {
    case GPUTextureFormat::kR8Unorm:
      return MTLPixelFormatR8Unorm;
    case GPUTextureFormat::kRGB565Unorm:
      if (@available(macOS 11.0, *)) {
        return MTLPixelFormatB5G6R5Unorm;
      }
      [[fallthrough]];
    // @warning: Metal doesn't support 24-bit pixel format
    case GPUTextureFormat::kRGB8Unorm:
    case GPUTextureFormat::kRGBA8Unorm:
      return MTLPixelFormatRGBA8Unorm;
    case GPUTextureFormat::kBGRA8Unorm:
      return MTLPixelFormatBGRA8Unorm;
    case GPUTextureFormat::kStencil8:
      return MTLPixelFormatStencil8;
    case GPUTextureFormat::kDepth24Stencil8:
      // force to use D32 + S8 since D24 + S8 not available on iOS
      return MTLPixelFormatDepth32Float_Stencil8;
    case GPUTextureFormat::kInvalid:
      return MTLPixelFormatInvalid;
  }

  return MTLPixelFormatRGBA8Unorm;
}

constexpr MTLCompareFunction ToMTLCompareFunction(GPUCompareFunction compile_func) {
  switch (compile_func) {
    case GPUCompareFunction::kNever:
      return MTLCompareFunctionNever;
    case GPUCompareFunction::kLess:
      return MTLCompareFunctionLess;
    case GPUCompareFunction::kEqual:
      return MTLCompareFunctionEqual;
    case GPUCompareFunction::kLessEqual:
      return MTLCompareFunctionLessEqual;
    case GPUCompareFunction::kGreater:
      return MTLCompareFunctionGreater;
    case GPUCompareFunction::kNotEqual:
      return MTLCompareFunctionNotEqual;
    case GPUCompareFunction::kGreaterEqual:
      return MTLCompareFunctionGreaterEqual;
    case GPUCompareFunction::kAlways:
      return MTLCompareFunctionAlways;
  }
}

constexpr MTLStencilOperation ToMTLStencilOperation(GPUStencilOperation stencil_op) {
  switch (stencil_op) {
    case GPUStencilOperation::kKeep:
      return MTLStencilOperationKeep;
    case GPUStencilOperation::kZero:
      return MTLStencilOperationZero;
    case GPUStencilOperation::kReplace:
      return MTLStencilOperationReplace;
    case GPUStencilOperation::kInvert:
      return MTLStencilOperationInvert;
    case GPUStencilOperation::kIncrementClamp:
      return MTLStencilOperationIncrementClamp;
    case GPUStencilOperation::kDecrementClamp:
      return MTLStencilOperationDecrementClamp;
    case GPUStencilOperation::kIncrementWrap:
      return MTLStencilOperationIncrementWrap;
    case GPUStencilOperation::kDecrementWrap:
      return MTLStencilOperationDecrementWrap;
  }
}

MTLDepthStencilDescriptor* ToMTLDepthStencilDescriptor(
    const GPUDepthStencilState& depth_stencil_state);

constexpr MTLBlendFactor ToMTLBlendFactor(GPUBlendFactor type) {
  switch (type) {
    case GPUBlendFactor::kZero:
      return MTLBlendFactorZero;
    case GPUBlendFactor::kOne:
      return MTLBlendFactorOne;
    case GPUBlendFactor::kSrc:
      return MTLBlendFactorSourceColor;
    case GPUBlendFactor::kOneMinusSrc:
      return MTLBlendFactorOneMinusSourceColor;
    case GPUBlendFactor::kSrcAlpha:
      return MTLBlendFactorSourceAlpha;
    case GPUBlendFactor::kOneMinusSrcAlpha:
      return MTLBlendFactorOneMinusSourceAlpha;
    case GPUBlendFactor::kDst:
      return MTLBlendFactorDestinationColor;
    case GPUBlendFactor::kOneMinusDst:
      return MTLBlendFactorOneMinusDestinationColor;
    case GPUBlendFactor::kDstAlpha:
      return MTLBlendFactorDestinationAlpha;
    case GPUBlendFactor::kOneMinusDstAlpha:
      return MTLBlendFactorOneMinusDestinationAlpha;
    case GPUBlendFactor::kSrcAlphaSaturated:
      return MTLBlendFactorSourceAlphaSaturated;
  }
  return MTLBlendFactorZero;
}

constexpr MTLVertexStepFunction ToMTLVertexStepFunction(GPUVertexStepMode step_mode) {
  switch (step_mode) {
    case GPUVertexStepMode::kVertex:
      return MTLVertexStepFunctionPerVertex;
    case GPUVertexStepMode::kInstance:
      return MTLVertexStepFunctionPerInstance;
  }
}

constexpr MTLVertexFormat ToMTLVertexFormat(GPUVertexFormat format) {
  switch (format) {
    case GPUVertexFormat::kFloat32:
      return MTLVertexFormatFloat;
    case GPUVertexFormat::kFloat32x2:
      return MTLVertexFormatFloat2;
    case GPUVertexFormat::kFloat32x3:
      return MTLVertexFormatFloat3;
    case GPUVertexFormat::kFloat32x4:
      return MTLVertexFormatFloat4;
  }
}

MTLVertexDescriptor* ToMTLVertexDescriptor(const std::vector<GPUVertexBufferLayout>& buffers);

MTLSamplerDescriptor* ToMTLSamplerDescriptor(const GPUSamplerDescriptor& desc);

MTLRenderPassDescriptor* ToMTLRenderPassDescriptor(const GPURenderPassDescriptor& desc);

TextureFormat ToTextureFormat(MTLPixelFormat format);
GPUTextureFormat ToGPUTextureFormat(MTLPixelFormat format);

NS_ASSUME_NONNULL_END

}  // namespace skity

#endif  // SRC_GPU_MTL_FORMATS_MTL_HPP
