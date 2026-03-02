// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GPU_GPU_CONTEXT_GL_HPP
#define INCLUDE_SKITY_GPU_GPU_CONTEXT_GL_HPP

#include <skity/gpu/gpu_context.hpp>
#include <skity/gpu/gpu_surface.hpp>
#include <skity/gpu/texture.hpp>
#include <skity/macros.hpp>

namespace skity {

/**
 * @enum GLSurfaceType indicate which type the GL backend Surface is target on
 */
enum class GLSurfaceType {
  /**
   * empty type, default value
   */
  kInvalid,
  /**
   * Indicate the Surface is target on a GL texture
   */
  kTexture,
  /**
   * Indicate the Surface is target on a GL framebuffer object
   */
  kFramebuffer,
};

struct GPUSurfaceDescriptorGL : public GPUSurfaceDescriptor {
  GLSurfaceType surface_type = GLSurfaceType::kInvalid;
  /**
   * GL Object id
   *
   *  If surface_type is GLSurfaceType::kTexture, this value is a valid GL
   *  texture id
   *
   *  If surface_type is GLSurfaceType::kFramebuffer, this value is a valid GL
   *  framebuffer id. Can set to 0 which means the GLSurface is target for
   *  on-screen rendering
   */
  uint32_t gl_id = 0;

  /**
   * Indicate whether or not this framebuffer as stencil attachment.
   * Ignored. If surface_type is not GLSurfaceType::kFramebuffer
   */
  bool has_stencil_attachment = false;

  /*
   * If 'enable_blit_from_fbo' is 'true', then skity will blit from the target
   * framebuffer object to the internal framebuffer object before drawing.
   * The value is only valid when 'surface_type' is
   * 'GLSurfaceType::kFramebuffer' and 'has_stencil_attachment' is 'false' and
   * 'sample_count' is 1
   */
  bool can_blit_from_target_fbo = false;
};

struct GPUBackendTextureInfoGL : public GPUBackendTextureInfo {
  /**
   * GL texture id
   */
  uint32_t tex_id = 0;

  /**
   * Indicate whether or not the engine is responsible for deleting the texture
   */
  bool owned_by_engine = false;
};

/**
 * Create a GPUContext instance target on OpenGL or OpenGLES backend.
 *
 * @param proc_loader  Function pointer which is pointer to a **GLProcLoader**
 *                     Skity needs this function to load GL symbol during
 *                     runtime. Since Skity do not link libGL.so or libGLESv2.so
 *                     during compile time
 *
 * @return             GPUContext instance or null if create failed
 */
std::unique_ptr<GPUContext> SKITY_API GLContextCreate(void* proc_loader);

}  // namespace skity

#endif  // INCLUDE_SKITY_GPU_GPU_CONTEXT_GL_HPP
