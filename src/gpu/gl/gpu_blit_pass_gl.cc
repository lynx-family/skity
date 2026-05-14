// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gl/gpu_blit_pass_gl.hpp"

#include "src/gpu/gl/formats_gl.h"
#include "src/gpu/gl/gl_interface.hpp"
#include "src/gpu/gl/gpu_buffer_gl.hpp"
#include "src/gpu/gl/gpu_sampler_gl.hpp"
#include "src/gpu/gl/gpu_texture_gl.hpp"
#include "src/logging.hpp"
#include "src/tracing.hpp"

namespace skity {

void GPUBlitPassGL::UploadTextureData(std::shared_ptr<GPUTexture> texture,
                                      uint32_t offset_x, uint32_t offset_y,
                                      uint32_t width, uint32_t height,
                                      void* data) {
  auto gl_texture = static_cast<GPUTextureGL*>(texture.get());
  gl_texture->UploadData(offset_x, offset_y, width, height, data);
}

void GPUBlitPassGL::UploadBufferData(GPUBuffer* buffer, void* data,
                                     size_t size) {
  auto gl_buffer = static_cast<GPUBufferGL*>(buffer);
  gl_buffer->UploadData(data, size);
}

void GPUBlitPassGL::GenerateMipmaps(
    const std::shared_ptr<GPUTexture>& texture) {
  auto gl_texture = static_cast<GPUTextureGL*>(texture.get());

  gl_texture->GenerateMipmaps();
}

void GPUBlitPassGL::CopyTextureToTexture(std::shared_ptr<GPUTexture> src,
                                         std::shared_ptr<GPUTexture> dst,
                                         const GPUBlitPass::TextureCopyRegion&
                                             region) {
  auto src_gl = static_cast<GPUTextureGL*>(src.get());
  auto dst_gl = static_cast<GPUTextureGL*>(dst.get());

  const auto& src_fbo_opt = src_gl->GetFramebuffer();
  uint32_t src_fbo;
  if (src_fbo_opt) {
    src_fbo = src_fbo_opt->fbo_id;
    GL_CALL(BindFramebuffer, GL_READ_FRAMEBUFFER, src_fbo);
  } else {
    GL_CALL(GenFramebuffers, 1, &src_fbo);
    src_gl->SetFramebuffer(src_fbo, true);
    GL_CALL(BindFramebuffer, GL_READ_FRAMEBUFFER, src_fbo);
    GL_CALL(FramebufferTexture2D, GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, src_gl->GetGLTextureID(), 0);
  }

  const auto& dst_fbo_opt = dst_gl->GetFramebuffer();
  uint32_t dst_fbo;
  if (dst_fbo_opt) {
    dst_fbo = dst_fbo_opt->fbo_id;
    GL_CALL(BindFramebuffer, GL_DRAW_FRAMEBUFFER, dst_fbo);
  } else {
    GL_CALL(GenFramebuffers, 1, &dst_fbo);
    dst_gl->SetFramebuffer(dst_fbo, true);
    GL_CALL(BindFramebuffer, GL_DRAW_FRAMEBUFFER, dst_fbo);
    GL_CALL(FramebufferTexture2D, GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, dst_gl->GetGLTextureID(), 0);
  }

  uint32_t src_tex_height = src->GetDescriptor().height;
  uint32_t dst_tex_height = dst->GetDescriptor().height;

  if (GL_CALL(CheckFramebufferStatus, GL_READ_FRAMEBUFFER) !=
      GL_FRAMEBUFFER_COMPLETE) {
    LOGE("src_fbo = %d is not complete", src_fbo);
    return;
  }

  if (GL_CALL(CheckFramebufferStatus, GL_DRAW_FRAMEBUFFER) !=
      GL_FRAMEBUFFER_COMPLETE) {
    LOGE("dst_fbo = %d is not complete", dst_fbo);
    return;
  }

  GL_CALL(BlitFramebuffer,                                //
          region.src_x,                                   //
          src_tex_height - region.src_y - region.height,  //
          region.src_x + region.width,                    //
          src_tex_height - region.src_y,                  //
          region.dst_x,                                   //
          dst_tex_height - region.dst_y - region.height,  //
          region.dst_x + region.width,                    //
          dst_tex_height - region.dst_y,                  //
          GL_COLOR_BUFFER_BIT,                            //
          GL_NEAREST);
}

}  // namespace skity
