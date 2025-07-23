// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GL_GPU_CONTEXT_IMPL_GL_HPP
#define SRC_GPU_GL_GPU_CONTEXT_IMPL_GL_HPP

#include "src/gpu/gpu_context_impl.hpp"

namespace skity {

class GPUContextImplGL : public GPUContextImpl {
 public:
  GPUContextImplGL();

  ~GPUContextImplGL() override = default;

  std::unique_ptr<GPUSurface> CreateSurface(
      GPUSurfaceDescriptor *desc) override;

  std::unique_ptr<GPUSurface> CreateFxaaSurface(
      GPUSurfaceDescriptor *desc) override;

 protected:
  std::unique_ptr<GPUDevice> CreateGPUDevice() override;

  std::shared_ptr<GPUTexture> OnWrapTexture(GPUBackendTextureInfo *info,
                                            ReleaseCallback callback,
                                            ReleaseUserData user_data) override;

  std::shared_ptr<Data> OnReadPixels(
      const std::shared_ptr<GPUTexture> &texture) const override;

  std::unique_ptr<GPURenderTarget> OnCreateRenderTarget(
      const GPURenderTargetDescriptor &desc,
      std::shared_ptr<Texture> texture) override;

 private:
  std::unique_ptr<GPUSurface> CreateDirectSurface(
      const GPUSurfaceDescriptor &desc, uint32_t fbo_id, bool need_free);

  std::unique_ptr<GPUSurface> CreateBlitSurface(
      const GPUSurfaceDescriptor &desc, uint32_t fbo_id,
      bool can_blit_from_target_fbo);

  std::unique_ptr<GPUSurface> CreateTextureSurface(
      const GPUSurfaceDescriptor &desc, uint32_t tex_id);

#ifdef SKITY_ANDROID
  std::unique_ptr<GPUSurface> CreateDrawTextureSurface(
      const GPUSurfaceDescriptor &desc, uint32_t fbo_id,
      bool can_blit_from_target_fbo);

#endif
};

}  // namespace skity

#endif  // SRC_GPU_GL_GPU_CONTEXT_IMPL_GL_HPP
