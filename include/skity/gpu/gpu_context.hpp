// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GPU_GPU_CONTEXT_HPP
#define INCLUDE_SKITY_GPU_GPU_CONTEXT_HPP

#include <cstdint>
#include <memory>
#include <skity/geometry/rect.hpp>
#include <skity/gpu/gpu_backend_type.hpp>
#include <skity/gpu/gpu_render_target.hpp>
#include <skity/gpu/gpu_surface.hpp>
#include <skity/gpu/texture.hpp>
#include <skity/macros.hpp>

namespace skity {

/**
 * @enum GPUError indicate the internal state about Skity engine
 */
enum class GPUError {
  /**
   * every thing is fine
   */
  kNoError,
  /**
   * there is error happened during GPU context creation.
   *  like trying to create vulkan backend on no vulkan driver device
   */
  kGPUError,
  /**
   * there is error happened during GPU pipeline creation
   *  like shader compile or link error
   */
  kPipelineError,
};

typedef void (*GPUErrorCallback)(GPUError error, char const* message,
                                 void* userdata);

/**
 * @class GPUContext
 *
 * Hold GPU information and bridge user data and underline GPU object.
 * For general, only needs one instance in the entire RenderThread or
 * in user Application
 *
 */
class SKITY_API GPUContext {
 public:
  virtual ~GPUContext() = default;

  virtual GPUBackendType GetBackendType() const = 0;

  /**
   * Create a GPU backend surface for rendering
   *
   * @param desc describe the information to create the surface
   *             different backends may have different descriptor structures
   * @return GPUSurface instance or null if init failed
   */
  virtual std::unique_ptr<GPUSurface> CreateSurface(
      GPUSurfaceDescriptor* desc) = 0;

  /**
   * Create a GPU backend surface that embeds a Fast Approximate Anti-Aliasing
   * implementation that for rendering
   *
   * Note:
   *   This function is deprecated, and always return null.
   *
   * @param desc describe the information to create the surface
   *             different backends may have different descriptor structures
   * @return GPUSurface instance or null if init failed
   */
  SKITY_EXPERIMENTAL
  virtual std::unique_ptr<GPUSurface> CreateFxaaSurface(
      GPUSurfaceDescriptor* desc) = 0;

  /**
   * Create a Texture instance associated with current GPU context.
   * The underline GPUTexture is owned by this GPUContext
   * The upload logical must happened inside the same thread the GPUContext is
   * created.
   *
   * @param format      Pixel format for this texture
   * @param width       width of this texture
   * @param height      height of this texture
   * @param alpha_type  alpha type of this texture
   * @return            texture instance or null if creation failed
   */
  virtual std::shared_ptr<Texture> CreateTexture(TextureFormat format,
                                                 uint32_t width,
                                                 uint32_t height,
                                                 AlphaType alpha_type) = 0;

  /**
   * Create a Texture from pre created GPUTexture.
   * This may cause ownership transfer
   *
   * @param info      Backend texture information. Different backends may have
   *                  different descriptor structures
   * @param callback  User function called when supplied texture may be deleted.
   * @param user_data User data passed to callback.
   * @return          texture instance or null if creation failed
   */
  virtual std::shared_ptr<Texture> WrapTexture(GPUBackendTextureInfo* info,
                                               ReleaseCallback callback,
                                               ReleaseUserData user_data) = 0;

  /**
   * Create a Texture from pre created GPUTexture.
   * This may cause ownership transfer
   *
   * @param info Backend texture information.
   *             Different backends may have different descriptor structures
   * @return     texture instance or null if creation failed
   */
  std::shared_ptr<Texture> WrapTexture(GPUBackendTextureInfo* info) {
    return WrapTexture(info, nullptr, nullptr);
  }

  /**
   * Create a Render Target object. The format of the texture is decided by the
   * GPUContext
   *
   * @note This API is unstable, and may changed in the future
   *
   * @param width  width of the render target
   * @param height height of the render target
   * @return       render target instance or null if creation failed
   */
  virtual std::unique_ptr<GPURenderTarget> CreateRenderTarget(
      const GPURenderTargetDescriptor& desc) = 0;

  /**
   * Flush the pending draw call holding by the GPURenderTarget. And generate a
   * Image of the render target contents.
   *
   * @note This API is unstable, and may changed in the future
   *
   * @param render_target render target instance
   * @return              Image instance or null if something wrong happened
   */
  virtual std::shared_ptr<Image> MakeSnapshot(
      std::unique_ptr<GPURenderTarget> render_target) = 0;

  /**
   * Controls the buffer size used for all GPU resources create by this
   * GPUContext. This size can effect framebuffer and temporary texture
   * creation.
   *
   * @param size_in_bytes max buffer size, pass 0 can disable resource cache
   */
  SKITY_EXPERIMENTAL
  virtual void SetResourceCacheLimit(size_t size_in_bytes) = 0;

  /**
   * Register a error callback for outside user.
   * Through this callback function, user can obtain the error information
   * inside the engine.
   *
   * @param callback   the callback function to receive error code and message
   * @param user_data  custom data passed to callback function, can be null
   */
  void SetErrorCallback(GPUErrorCallback callback, void* user_data) {
    error_callback_ = callback;
    error_callback_user_data_ = user_data;
  }

  void TriggerErrorCallback(GPUError error, const char* message) {
    if (error_callback_ == nullptr) {
      return;
    }

    error_callback_(error, message, error_callback_user_data_);
  }

  /**
   * Used to specify whether draw calls that have the opportunity to be merged
   * will be merged internally in Skity.
   */
  void SetEnableMergingDrawCall(bool enable_merging_draw_call) {
    enable_merging_draw_call_ = enable_merging_draw_call;
  }

  bool IsEnableMergingDrawCall() const { return enable_merging_draw_call_; }

  /**
   * Used to specify whether to use contour aa for anti-aliasing when msaa and
   * fxaa are both disabled.
   */
  void SetEnableContourAA(bool enable_contour_aa) {
    enable_contour_aa_ = enable_contour_aa;
  }

  bool IsEnableContourAA() const { return enable_contour_aa_; }

  /**
   * Use a larger atlas cache for better performance, but with that comes a
   * larger memory overhead. There are 4 times more memory if the corresponding
   * bit is set.
   * @param larger_atlas_mask  bit 0 represents the A8 format for normal texts,
   * and bit 1 represents the RGBA32 format for emoji texts.
   */
  void SetLargerAtlasMask(std::uint8_t larger_atlas_mask) {
    larger_atlas_mask_ = larger_atlas_mask;
  }

  std::uint8_t GetLargerAtlasMask() { return larger_atlas_mask_; }

  /**
   * Used to set linear filter in text rendering. Normally, text texture
   * sampling should use nearest fliter, but since Skity currently does not
   * support the rendering of rotated text, linear filter is used to reduce
   * the jagged effect.
   *
   * @warning This API should not be used arbitrarily and will be removed in the
   * next major version.
   */
  void SetEnableTextLinearFilter(bool enable_text_linear_filter) {
    enable_text_linear_filter_ = enable_text_linear_filter;
  }

  bool IsEnableTextLinearFilter() const { return enable_text_linear_filter_; }

 private:
  GPUErrorCallback error_callback_ = nullptr;
  void* error_callback_user_data_ = nullptr;
  bool enable_merging_draw_call_ = true;
  bool enable_contour_aa_ = false;
  std::uint8_t larger_atlas_mask_ = 0;
  bool enable_text_linear_filter_ = false;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GPU_GPU_CONTEXT_HPP
