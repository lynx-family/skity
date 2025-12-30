// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gl/gpu_sampler_gl.hpp"

#include <optional>

#include "src/gpu/gl/gl_interface.hpp"
#include "src/gpu/gl/gpu_texture_gl.hpp"
#include "src/logging.hpp"

namespace skity {

namespace {

uint32_t ToGLParam(GPUAddressMode address) {
  switch (address) {
    case GPUAddressMode::kRepeat:
      return GL_REPEAT;
    case GPUAddressMode::kMirrorRepeat:
      return GL_MIRRORED_REPEAT;
    case GPUAddressMode::kClampToEdge:
    default:
      break;
  }
  return GL_CLAMP_TO_EDGE;
}

uint32_t ToGLParam(GPUFilterMode filter,
                   std::optional<GPUMipmapMode> mipmap = std::nullopt) {
  if (!mipmap.has_value()) {
    switch (filter) {
      case GPUFilterMode::kLinear:
        return GL_LINEAR;
      case GPUFilterMode::kNearest:
      default:
        break;
    }
    return GL_NEAREST;
  }

  switch (*mipmap) {
    case GPUMipmapMode::kNearest:
      switch (filter) {
        case GPUFilterMode::kLinear:
          return GL_LINEAR_MIPMAP_NEAREST;
        case GPUFilterMode::kNearest:
        default:
          break;
      }
      return GL_NEAREST_MIPMAP_NEAREST;
    case GPUMipmapMode::kLinear:
      switch (filter) {
        case GPUFilterMode::kLinear:
          return GL_LINEAR_MIPMAP_LINEAR;
        case GPUFilterMode::kNearest:
        default:
          break;
      }
      return GL_NEAREST_MIPMAP_LINEAR;
    default:
      break;
  }
  CHECK(false);  // Should not reach here
  return GL_NONE;
}

}  // namespace

std::shared_ptr<GPUSamplerGL> GPUSamplerGL::Create(
    const GPUSamplerDescriptor& descriptor) {
  return std::make_shared<GPUSamplerGL>(descriptor);
}

GPUSamplerGL::GPUSamplerGL(const GPUSamplerDescriptor& descriptor)
    : GPUSampler(descriptor) {
  GL_CALL(GenSamplers, 1, &sampler_id_);

  GL_CALL(SamplerParameteri, sampler_id_, GL_TEXTURE_MIN_FILTER,
          ToGLParam(descriptor.min_filter));
  GL_CALL(SamplerParameteri, sampler_id_, GL_TEXTURE_MAG_FILTER,
          ToGLParam(descriptor.mag_filter));
  GL_CALL(SamplerParameteri, sampler_id_, GL_TEXTURE_WRAP_S,
          ToGLParam(descriptor.address_mode_u));
  GL_CALL(SamplerParameteri, sampler_id_, GL_TEXTURE_WRAP_T,
          ToGLParam(descriptor.address_mode_v));

  if (sampler_id_ == 0) {
    LOGE("Failed to create GL Sampler !!");
  }
}

GPUSamplerGL::~GPUSamplerGL() {
  if (sampler_id_ != 0) {
    GL_CALL(DeleteSamplers, 1, &sampler_id_);
    sampler_id_ = 0;
  }
}

void GPUSamplerGL::ConfigureTexture(GPUTextureGL* texture) const {
  std::optional<GPUMipmapMode> mip_filter = std::nullopt;
  if (texture->GetDescriptor().mip_level_count > 1) {
    mip_filter = desc_.mipmap_filter;
  }

  GL_CALL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
          ToGLParam(desc_.min_filter, mip_filter));
  GL_CALL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
          ToGLParam(desc_.mag_filter));
  GL_CALL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
          ToGLParam(desc_.address_mode_u));
  GL_CALL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
          ToGLParam(desc_.address_mode_v));
}

}  // namespace skity
