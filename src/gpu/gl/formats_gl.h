// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GL_FORMATS_GL_HPP
#define SRC_GPU_GL_FORMATS_GL_HPP

#include "src/gpu/gl/gl_interface.hpp"
#include "src/gpu/gpu_render_pipeline.hpp"
#include "src/gpu/gpu_sampler.hpp"

namespace skity {

constexpr GLint ToMinMagFilter(GPUFilterMode op) {
  switch (op) {
    case GPUFilterMode::kNearest:
      return GL_NEAREST;
    case GPUFilterMode::kLinear:
      return GL_LINEAR;
  }
}

constexpr GLint ToAddressMode(GPUAddressMode mode) {
  switch (mode) {
    case GPUAddressMode::kClampToEdge:
      return GL_CLAMP_TO_EDGE;
    case GPUAddressMode::kRepeat:
      return GL_REPEAT;
    case GPUAddressMode::kMirrorRepeat:
      return GL_MIRRORED_REPEAT;
  }
}

constexpr GLenum ToStencilOp(GPUStencilOperation op) {
  switch (op) {
    case GPUStencilOperation::kKeep:
      return GL_KEEP;
    case GPUStencilOperation::kZero:
      return GL_ZERO;
    case GPUStencilOperation::kReplace:
      return GL_REPLACE;
    case GPUStencilOperation::kIncrementClamp:
      return GL_INCR;
    case GPUStencilOperation::kDecrementClamp:
      return GL_DECR;
    case GPUStencilOperation::kInvert:
      return GL_INVERT;
    case GPUStencilOperation::kIncrementWrap:
      return GL_INCR_WRAP;
    case GPUStencilOperation::kDecrementWrap:
      return GL_DECR_WRAP;
  }
}

constexpr GLenum ToCompareFunction(GPUCompareFunction func) {
  switch (func) {
    case GPUCompareFunction::kNever:
      return GL_NEVER;
    case GPUCompareFunction::kAlways:
      return GL_ALWAYS;
    case GPUCompareFunction::kLess:
      return GL_LESS;
    case GPUCompareFunction::kEqual:
      return GL_EQUAL;
    case GPUCompareFunction::kLessEqual:
      return GL_LEQUAL;
    case GPUCompareFunction::kGreater:
      return GL_GREATER;
    case GPUCompareFunction::kNotEqual:
      return GL_NOTEQUAL;
    case GPUCompareFunction::kGreaterEqual:
      return GL_GEQUAL;
  }
}

constexpr GLenum ToBlendFactor(GPUBlendFactor factor) {
  switch (factor) {
    case GPUBlendFactor::kZero:
      return GL_ZERO;
    case GPUBlendFactor::kOne:
      return GL_ONE;
    case GPUBlendFactor::kSrc:
      return GL_SRC_COLOR;
    case GPUBlendFactor::kOneMinusSrc:
      return GL_ONE_MINUS_SRC_COLOR;
    case GPUBlendFactor::kSrcAlpha:
      return GL_SRC_ALPHA;
    case GPUBlendFactor::kOneMinusSrcAlpha:
      return GL_ONE_MINUS_SRC_ALPHA;
    case GPUBlendFactor::kDst:
      return GL_DST_COLOR;
    case GPUBlendFactor::kOneMinusDst:
      return GL_ONE_MINUS_DST_COLOR;
    case GPUBlendFactor::kDstAlpha:
      return GL_DST_ALPHA;
    case GPUBlendFactor::kOneMinusDstAlpha:
      return GL_ONE_MINUS_DST_ALPHA;
    case GPUBlendFactor::kSrcAlphaSaturated:
      return GL_SRC_ALPHA_SATURATE;
  }
}

constexpr GLint ExternalFormatFrom(GPUTextureFormat format) {
  switch (format) {
    case GPUTextureFormat::kR8Unorm:
      return GL_RED;
    case GPUTextureFormat::kRGB8Unorm:
    case GPUTextureFormat::kRGB565Unorm:
      return GL_RGB;
    case GPUTextureFormat::kRGBA8Unorm:
      return GL_RGBA;
    case GPUTextureFormat::kBGRA8Unorm:
      // return rgba here as we swizzle r and b in sampling later
      return GL_RGBA;
    case GPUTextureFormat::kStencil8:
    case GPUTextureFormat::kDepth24Stencil8:
      return GL_DEPTH_STENCIL;
    case GPUTextureFormat::kInvalid:
      return GL_RGBA;
  }
}

constexpr GLint ExternalTypeFrom(GPUTextureFormat format) {
  switch (format) {
    case GPUTextureFormat::kRGB565Unorm:
      return GL_UNSIGNED_SHORT_5_6_5;
    case GPUTextureFormat::kStencil8:
    case GPUTextureFormat::kDepth24Stencil8:
      return GL_UNSIGNED_INT_24_8;
    case GPUTextureFormat::kR8Unorm:
    case GPUTextureFormat::kRGB8Unorm:
    case GPUTextureFormat::kRGBA8Unorm:
    case GPUTextureFormat::kBGRA8Unorm:
    case GPUTextureFormat::kInvalid:
      return GL_UNSIGNED_BYTE;
  }
}

constexpr GLint InternalFormatFrom(GPUTextureFormat format) {
  switch (format) {
    case GPUTextureFormat::kR8Unorm:
      return GL_R8;
    case GPUTextureFormat::kRGB8Unorm:
      return GL_RGB8;
    case GPUTextureFormat::kRGB565Unorm:
      return GL_RGB565;
    case GPUTextureFormat::kRGBA8Unorm:
    case GPUTextureFormat::kBGRA8Unorm:
      return GL_RGBA8;
    case GPUTextureFormat::kStencil8:
    case GPUTextureFormat::kDepth24Stencil8:
      return GL_DEPTH24_STENCIL8;
    case GPUTextureFormat::kInvalid:
      return GL_RGBA8;
  }
}

}  // namespace skity

#endif  // SRC_GPU_GL_FORMATS_GL_HPP
