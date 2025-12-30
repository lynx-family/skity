// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gl/gpu_texture_gl.hpp"

#include "src/gpu/gl/formats_gl.h"
#include "src/gpu/gl/gl_interface.hpp"
#include "src/gpu/gl/gpu_sampler_gl.hpp"
#include "src/logging.hpp"
#include "src/tracing.hpp"

namespace skity {

GLFramebufferHolder::~GLFramebufferHolder() {
  if (need_free && fbo_id != 0) {
    GL_CALL(DeleteFramebuffers, 1, &fbo_id);
  }

  fbo_id = 0;
  need_free = false;
}

GPUTextureGL::GPUTextureGL(const GPUTextureDescriptor& descriptor)
    : GPUTexture(descriptor) {
  if (descriptor.sample_count != 1) {
    texture_target_ = GL_TEXTURE_2D_MULTISAMPLE;
  } else {
    texture_target_ = GL_TEXTURE_2D;
  }
}

GPUTextureGL::~GPUTextureGL() { Destroy(); }

std::shared_ptr<GPUTextureGL> GPUTextureGL::Create(
    const GPUTextureDescriptor& descriptor) {
  auto texture = std::make_shared<GPUTextureGL>(descriptor);
  texture->Initialize();
  return texture;
}

void GPUTextureGL::UploadData(uint32_t offset_x, uint32_t offset_y,
                              uint32_t width, uint32_t height, void* data) {
  SKITY_TRACE_EVENT(GPUTextureGL_UploadData);
  if (texture_target_ != GL_TEXTURE_2D) {
    // MSAA texture can not upload data directly from CPU
    LOGW("Trying to upload data to a texture not target GL_TEXTURE_2D !!");
    return;
  }

  if (desc_.height == 0 || desc_.width == 0) {
    LOGW("Uploading data to a texture with width or height is 0 !!");
    return;
  }
  Bind();
  GL_CALL(TexSubImage2D, texture_target_, 0, offset_x, offset_y, width, height,
          ExternalFormatFrom(desc_.format), ExternalTypeFrom(desc_.format),
          data);
  Unbind();
}

void GPUTextureGL::Initialize() {
  GL_CALL(GenTextures, 1, &texture_id_);

  if (texture_id_ == 0) {
    LOGE(
        "Failed to create GL Texture, maybe out of memory or GL context is not "
        "valid !!");
  }

  Bind();

  if (desc_.format != GPUTextureFormat::kStencil8 &&
      texture_target_ == GL_TEXTURE_2D) {
    GL_CALL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL_CALL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (GPUTextureFormat::kBGRA8Unorm == desc_.format) {
      GL_CALL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
      GL_CALL(TexParameteri, GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    }

    // TODO(jondong): Sampling filter configs goes here.

    if (desc_.format == GPUTextureFormat::kR8Unorm) {
      GL_CALL(PixelStorei, GL_UNPACK_ALIGNMENT, 1);
    }
  }

  if (GetDescriptor().sample_count == 1) {
    GL_CALL(TexImage2D, texture_target_, 0, InternalFormatFrom(desc_.format),
            desc_.width, desc_.height, 0, ExternalFormatFrom(desc_.format),
            ExternalTypeFrom(desc_.format), nullptr);
  } else {
    GL_CALL(TexImage2DMultisample, texture_target_,
            GetDescriptor().sample_count, InternalFormatFrom(desc_.format),
            desc_.width, desc_.height, GL_TRUE);
  }

  Unbind();
}

void GPUTextureGL::Bind() const {
  GL_CALL(BindTexture, texture_target_, texture_id_);
}

void GPUTextureGL::Unbind() const { GL_CALL(BindTexture, texture_target_, 0); }

void GPUTextureGL::Destroy() {
  if (texture_id_) {
    GL_CALL(DeleteTextures, 1, &texture_id_);
  }
  texture_id_ = 0;
}

void GPUTextureGL::CombineSampler(skity::GPUSamplerGL* sampler) {
  if (combined_sampler_ == sampler) {
    return;
  }

  combined_sampler_ = sampler;

  combined_sampler_->ConfigureTexture(this);
}

size_t GPUTextureGL::GetBytes() const {
  auto& desc = GetDescriptor();
  return desc.width * desc.height * GetTextureFormatBytesPerPixel(desc.format) *
         desc.sample_count;
}

void GPUTextureGL::SetFramebuffer(uint32_t fbo_id, bool need_free) {
  if (fbo_) {
    fbo_.reset();
  }

  fbo_ = GLFramebufferHolder(fbo_id, need_free);
}

/// GPUExternalTextureGL
GPUExternalTextureGL::~GPUExternalTextureGL() {
  if (!owned_by_engine_) {
    // Set texture_id to 0, otherwise GLTexture's destructor will delete the
    // texture.
    texture_id_ = 0;
  }
}

std::shared_ptr<GPUTexture> GPUExternalTextureGL::Make(
    const GPUTextureDescriptor& descriptor, uint32_t id, bool owned_by_engine,
    ReleaseCallback callback, ReleaseUserData user_data) {
  auto result = std::make_shared<GPUExternalTextureGL>(
      descriptor, owned_by_engine, callback, user_data);
  result->texture_id_ = id;
  return std::move(result);
}

GPUTextureRenderBufferGL::~GPUTextureRenderBufferGL() {
  if (buffer_id_ == 0) {
    return;
  }

  GL_CALL(DeleteRenderbuffers, 1, &buffer_id_);
}

size_t GPUTextureRenderBufferGL::GetBytes() const {
  const auto& desc = this->GetDescriptor();

  return desc.width * desc.height * desc.sample_count;
}

std::shared_ptr<GPUTextureRenderBufferGL> GPUTextureRenderBufferGL::Create(
    const GPUTextureDescriptor& desc) {
  GLuint buffer_id = 0;

  GL_CALL(GenRenderbuffers, 1, &buffer_id);

  if (buffer_id == 0) {
    return nullptr;
  }

  GL_CALL(BindRenderbuffer, GL_RENDERBUFFER, buffer_id);

  GLenum gl_format = GL_STENCIL_INDEX8;

  if (desc.format == GPUTextureFormat::kRGBA8Unorm) {
    gl_format = GL_RGBA8;
  } else if (desc.format == GPUTextureFormat::kBGRA8Unorm) {
    gl_format = GL_RGBA8;
  } else if (desc.format == GPUTextureFormat::kRGB8Unorm) {
    gl_format = GL_RGB8;
  } else if (desc.format == GPUTextureFormat::kDepth24Stencil8) {
    gl_format = GL_DEPTH24_STENCIL8;
  }

  if (desc.sample_count > 1) {
#ifdef SKITY_ANDROID
    if (GLInterface::GlobalInterface()->ext_multisampled_render_to_texture) {
      GL_CALL(RenderbufferStorageMultisampleEXT, GL_RENDERBUFFER,
              desc.sample_count, gl_format, desc.width, desc.height);
    } else {
      GL_CALL(RenderbufferStorageMultisample, GL_RENDERBUFFER,
              desc.sample_count, gl_format, desc.width, desc.height);
    }
#else
    GL_CALL(RenderbufferStorageMultisample, GL_RENDERBUFFER, desc.sample_count,
            gl_format, desc.width, desc.height);
#endif
  } else {
    GL_CALL(RenderbufferStorage, GL_RENDERBUFFER, gl_format, desc.width,
            desc.height);
  }

  GL_CALL(BindRenderbuffer, GL_RENDERBUFFER, 0);

  return std::make_shared<GPUTextureRenderBufferGL>(desc, buffer_id);
}

void GPUTextureRenderBufferGL::SetFramebuffer(uint32_t fbo_id, bool need_free) {
  if (fbo_) {
    fbo_.reset();
  }

  fbo_ = GLFramebufferHolder(fbo_id, need_free);
}

}  // namespace skity
