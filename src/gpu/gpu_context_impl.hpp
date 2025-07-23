// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GPU_CONTEXT_IMPL_HPP
#define SRC_GPU_GPU_CONTEXT_IMPL_HPP

#include <skity/gpu/gpu_context.hpp>

#include "src/gpu/texture_manager.hpp"
#include "src/render/hw/hw_pipeline_lib.hpp"
#include "src/render/hw/hw_render_target_cache.hpp"
#include "src/render/text/atlas/atlas_manager.hpp"

namespace skity {

class GPUContextImpl : public GPUContext {
 public:
  explicit GPUContextImpl(GPUBackendType backend) : backend_type_(backend) {}

  ~GPUContextImpl() override = default;

  bool Init();

  GPUBackendType GetBackendType() const override { return backend_type_; }

  std::shared_ptr<Texture> CreateTexture(TextureFormat format, uint32_t width,
                                         uint32_t height,
                                         skity::AlphaType alpha_type) override;

  std::shared_ptr<Texture> WrapTexture(GPUBackendTextureInfo* info,
                                       ReleaseCallback release_callback,
                                       ReleaseUserData release_data) override;

  void SetResourceCacheLimit(size_t size_in_bytes) override;

  GPUDevice* GetGPUDevice() const { return gpu_device_.get(); }

  HWRenderTargetCache* GetRenderTargetCache() const {
    return render_target_cache_.get();
  }

  HWPipelineLib* GetPipelineLib() const { return pipeline_lib_.get(); }

  AtlasManager* GetAtlasManager() const { return atlas_manager_.get(); }

  TextureManager* GetTextureManager() const { return texture_manager_.get(); }

  std::unique_ptr<GPURenderTarget> CreateRenderTarget(
      const GPURenderTargetDescriptor& desc) override;

  std::shared_ptr<Image> MakeSnapshot(
      std::unique_ptr<GPURenderTarget> render_target) override;

  std::shared_ptr<Data> ReadPixels(const std::shared_ptr<GPUTexture>& texture);

 protected:
  virtual std::unique_ptr<GPUDevice> CreateGPUDevice() = 0;

  virtual std::shared_ptr<GPUTexture> OnWrapTexture(
      GPUBackendTextureInfo* info, ReleaseCallback callback,
      ReleaseUserData user_data) = 0;

  virtual std::unique_ptr<GPURenderTarget> OnCreateRenderTarget(
      const GPURenderTargetDescriptor& desc,
      std::shared_ptr<Texture> texture) = 0;

  virtual std::shared_ptr<Data> OnReadPixels(
      const std::shared_ptr<GPUTexture>& texture) const = 0;

 private:
  GPUBackendType backend_type_;
  std::unique_ptr<GPUDevice> gpu_device_ = {};
  std::shared_ptr<TextureManager> texture_manager_ = {};
  std::unique_ptr<HWRenderTargetCache> render_target_cache_ = {};
  std::unique_ptr<HWPipelineLib> pipeline_lib_ = {};
  std::unique_ptr<AtlasManager> atlas_manager_ = {};
};

}  // namespace skity

#endif  // SRC_GPU_GPU_CONTEXT_IMPL_HPP
