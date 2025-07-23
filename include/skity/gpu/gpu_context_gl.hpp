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

/**
 * Extra struct to pass information to support rendering content to part of the
 * target framebuffer. For now, it only used in Android for create GPUSurface
 * for FunctorView.
 */
struct PartialFrameInfo {
  /**
   * The width in pixel of the target Framebuffer
   */
  uint32_t width = 0;

  /**
   * The height in pixel of the target Framebuffer
   */
  uint32_t height = 0;

  /**
   * The bounding rect of the target rendering area inside the Framebuffer.
   * It is in OpenGL coordinate system, which means the origin point located in
   * bottom-left.
   *
   * The value passed from Android system.
   */
  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
};

/**
 * Create A special GPUSurface instance for rendering content into part of the
 * framebuffer target. It is only used in Android system to support FunctorView.
 *
 * @note This function must be called in Android main-thread.
 *
 * @param context       The GPUContext create in Android main-thread.
 * @param desc          The Surface description, it must be
 * GLSurfaceType::kFramebuffer
 * @param frame_info    The description of the partial rendering
 * @return              GPUSurface instance for partial rendering
 */
std::unique_ptr<GPUSurface> SKITY_API
GLCreatePartialSurface(GPUContext* context, const GPUSurfaceDescriptorGL& desc,
                       const PartialFrameInfo& frame_info);

/**
 * Update translate information for a given GPUSurface instance. The GPUSurface
 * must be a PartialSurface create by **GLCreatePartialSurface**.
 *
 * @note This function does not check the GPUSurface type info. The caller must
 * ensure it is a PartialSurface instance.
 *
 * @param surface   The surface needs to be update translate.
 * @param dx        Translate value in x axis
 * @param dy        Translate value in y axis
 */
void SKITY_API GLUpdateSurfaceTranslate(GPUSurface* surface, float dx,
                                        float dy);

}  // namespace skity

#endif  // INCLUDE_SKITY_GPU_GPU_CONTEXT_GL_HPP
