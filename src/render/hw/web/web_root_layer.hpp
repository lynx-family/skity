// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_WEB_WEB_ROOT_LAYER_HPP
#define SRC_RENDER_HW_WEB_WEB_ROOT_LAYER_HPP

#include <webgpu/webgpu.h>

#include "src/render/hw/layer/hw_root_layer.hpp"

namespace skity {

class WebRootLayer : public HWRootLayer {
 public:
  WebRootLayer(uint32_t width, uint32_t height, const Rect& bounds,
               GPUTextureFormat format, WGPUTexture texture);

  ~WebRootLayer() override;

 protected:
  HWDrawState OnPrepare(HWDrawContext* context) override;

  void OnPostDraw(GPURenderPass* render_pass, GPUCommandBuffer* cmd) override {}

  std::shared_ptr<GPURenderPass> OnBeginRenderPass(
      GPUCommandBuffer* cmd) override;

  bool IsValid() const override { return texture_ != nullptr; }

 private:
  void PrepareAttachments(HWDrawContext* context);

  void PrepareRenderPassDesc(HWDrawContext* context);

 private:
  WGPUTexture texture_ = nullptr;

  std::shared_ptr<GPUTexture> color_attachment_ = {};

  GPURenderPassDescriptor render_pass_desc_ = {};
};

}  // namespace skity

#endif  // SRC_RENDER_HW_WEB_WEB_ROOT_LAYER_HPP
