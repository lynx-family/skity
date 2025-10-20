// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_VK_VK_ROOT_LAYER_HPP
#define SRC_RENDER_HW_VK_VK_ROOT_LAYER_HPP

#include <volk.h>

#include "src/gpu/gpu_render_pass.hpp"
#include "src/render/hw/layer/hw_root_layer.hpp"

namespace skity {

class GPUTextureVk;

class VkRootLayer : public HWRootLayer {
 public:
  VkRootLayer(uint32_t width, uint32_t height, const Rect &bounds);
  ~VkRootLayer() override = default;

 protected:
  void Draw(GPURenderPass *render_pass) override;
  void OnPostDraw(GPURenderPass *render_pass, GPUCommandBuffer *cmd) override;
};

class VkExternTextureLayer : public VkRootLayer {
 public:
  VkExternTextureLayer(std::shared_ptr<GPUTexture> texture, const Rect &bounds);
  ~VkExternTextureLayer() override = default;

 protected:
  HWDrawState OnPrepare(HWDrawContext *context) override;
  std::shared_ptr<GPURenderPass> OnBeginRenderPass(
      GPUCommandBuffer *cmd) override;

 private:
  std::shared_ptr<GPUTexture> ext_texture_;
  std::shared_ptr<GPUTexture> depth_stencil_texture_;
  GPURenderPassDescriptor render_pass_desc_ = {};

  void CreateDepthStencilTextures(GPUDevice *device);
};

}  // namespace skity

#endif  // SRC_RENDER_HW_VK_VK_ROOT_LAYER_HPP