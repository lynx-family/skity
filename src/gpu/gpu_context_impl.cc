// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gpu_context_impl.hpp"

namespace skity {

bool GPUContextImpl::Init() {
  gpu_device_ = CreateGPUDevice();

  if (gpu_device_ == nullptr) {
    return false;
  }

  texture_manager_ = std::make_shared<TextureManager>(gpu_device_.get());
  atlas_manager_ = std::make_unique<AtlasManager>(gpu_device_.get(), this);
  render_target_cache_ = HWRenderTargetCache::Create(gpu_device_.get());

  pipeline_lib_ = std::make_unique<HWPipelineLib>(this, GetBackendType(),
                                                  gpu_device_.get());

  return true;
}

std::shared_ptr<Texture> GPUContextImpl::CreateTexture(TextureFormat format,
                                                       uint32_t width,
                                                       uint32_t height,
                                                       AlphaType alpha_type) {
  return texture_manager_->RegisterTexture(format, width, height, alpha_type,
                                           nullptr);
}

std::shared_ptr<Texture> GPUContextImpl::WrapTexture(
    GPUBackendTextureInfo* info, ReleaseCallback callback,
    ReleaseUserData user_data) {
  if (info == nullptr || info->backend != backend_type_) {
    return nullptr;
  }

  auto gpu_texture = OnWrapTexture(info, callback, user_data);

  if (gpu_texture == nullptr) {
    return nullptr;
  }

  return texture_manager_->RegisterTexture(info->format, info->width,
                                           info->height, info->alpha_type,
                                           std::move(gpu_texture));
}

void GPUContextImpl::SetResourceCacheLimit(size_t size_in_bytes) {
  render_target_cache_->SetMaxBytes(size_in_bytes);
}

std::unique_ptr<GPURenderTarget> GPUContextImpl::CreateRenderTarget(
    const GPURenderTargetDescriptor& desc) {
  if (desc.width == 0 || desc.height == 0) {
    return {};
  }

  GPUTextureDescriptor texture_desc{};
  texture_desc.usage =
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kTextureBinding) |
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kRenderAttachment);
  texture_desc.format = GPUTextureFormat::kRGBA8Unorm;
  texture_desc.width = desc.width;
  texture_desc.height = desc.height;
  texture_desc.storage_mode = GPUTextureStorageMode::kPrivate;

  auto gpu_texture = GetGPUDevice()->CreateTexture(texture_desc);

  return OnCreateRenderTarget(
      desc, GetTextureManager()->RegisterTexture(
                TextureFormat::kRGBA, desc.width, desc.height,
                AlphaType::kPremul_AlphaType, std::move(gpu_texture)));
}

std::shared_ptr<Image> GPUContextImpl::MakeSnapshot(
    std::unique_ptr<GPURenderTarget> render_target) {
  auto dl = render_target->recorder_.FinishRecording();

  if (dl == nullptr) {
    return nullptr;
  }

  auto canvas = render_target->surface_->LockCanvas();

  if (GetBackendType() == GPUBackendType::kOpenGL) {
    // GL framebuffer is flipped, so we need to flip it back
    canvas->Translate(0, render_target->surface_->GetHeight());
    canvas->Scale(1, -1);
  }

  dl->Draw(canvas);

  canvas->Flush();

  render_target->surface_->Flush();

  return Image::MakeHWImage(render_target->texture_);
}

std::shared_ptr<Data> GPUContextImpl::ReadPixels(
    const std::shared_ptr<GPUTexture>& texture) {
  if (!texture || texture->GetDescriptor().sample_count > 1 ||
      texture->GetBytes() == 0) {
    return nullptr;
  }
  return OnReadPixels(texture);
}

}  // namespace skity
