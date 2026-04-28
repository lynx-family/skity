// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_VK_VK_ROOT_LAYER_HPP
#define SRC_RENDER_HW_VK_VK_ROOT_LAYER_HPP

#include "src/render/hw/layer/hw_root_layer.hpp"

namespace skity {

class GPUTexture;

class VKRootLayer : public HWRootLayer {
 public:
  VKRootLayer(uint32_t width, uint32_t height,
              std::shared_ptr<GPUTexture> texture, const Rect& bounds,
              GPUTextureFormat format);

  ~VKRootLayer() override = default;

 protected:
  HWDrawState OnPrepare(HWDrawContext* context) override;

  void OnPostDraw(GPURenderPass* render_pass, GPUCommandBuffer* cmd) override {}

  std::shared_ptr<GPURenderPass> OnBeginRenderPass(
      GPUCommandBuffer* cmd) override;

  bool IsValid() const override { return color_attachment_ != nullptr; }

 private:
  std::shared_ptr<GPUTexture> color_attachment_ = {};
  GPURenderPassDescriptor render_pass_desc_ = {};
};

}  // namespace skity

#endif  // SRC_RENDER_HW_VK_VK_ROOT_LAYER_HPP
