// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_LAYER_HW_SUB_LAYER_HPP
#define SRC_RENDER_HW_LAYER_HW_SUB_LAYER_HPP

#include "src/geometry/math.hpp"
#include "src/render/hw/hw_layer.hpp"

namespace skity {

class HWSubLayer : public HWLayer {
 public:
  HWSubLayer(Matrix matrix, int32_t depth, Rect bounds, uint32_t width,
             uint32_t height)
      : HWLayer(matrix, depth, bounds, width, height),
        texture_size_(width, height) {}

  ~HWSubLayer() override = default;

  void SetAlpha(float alpha) { alpha_ = alpha; }

  void SetBlendMode(BlendMode blend_mode) { blend_mode_ = blend_mode; }

  void ExpandTextureSizeToNextPow2() {
    texture_size_ = MakeApprox(texture_size_);
  }

  GPUTextureDescriptor GetColorTextureDesc() const {
    GPUTextureDescriptor desc;
    desc.width = texture_size_.x;
    desc.height = texture_size_.y;
    desc.format = GetColorFormat();
    desc.storage_mode = GPUTextureStorageMode::kPrivate;
    desc.usage =
        static_cast<GPUTextureUsageMask>(GPUTextureUsage::kTextureBinding) |
        static_cast<GPUTextureUsageMask>(GPUTextureUsage::kRenderAttachment);

    // color_texture always use sample count 1 since it used in rendering
    // command
    desc.sample_count = 1;
    return desc;
  }

  void SetTextures(const std::shared_ptr<GPUTexture>& color_texture,
                   const std::shared_ptr<GPUTexture>& layer_back_draw_texture) {
    color_texture_ = color_texture;
    layer_back_draw_texture_ = layer_back_draw_texture;
  }

 protected:
  const std::shared_ptr<GPUTexture>& GetColorTexture() {
    return color_texture_;
  }

  HWDrawState OnPrepare(HWDrawContext* context) override;

  void OnGenerateCommand(HWDrawContext* context, HWDrawState state) override;

  std::shared_ptr<GPURenderPass> OnBeginRenderPass(
      GPUCommandBuffer* cmd) override;

  void OnPostDraw(GPURenderPass* render_pass, GPUCommandBuffer* cmd) override;

  virtual Rect GetLayerBackDrawBounds() { return GetBounds(); }

 private:
  void InitTexture(GPUContextImpl* gpu_context,
                   HWRenderTargetCache::Pool* pool);

  void PrepareRenderPassDesc(HWDrawContext* context);

 private:
  float alpha_ = 1.0f;
  BlendMode blend_mode_ = BlendMode::kSrcOver;
  HWDraw* layer_back_draw_ = {};
  std::shared_ptr<GPUTexture> color_texture_ = {};
  std::shared_ptr<GPUTexture> layer_back_draw_texture_ = {};
  GPURenderPassDescriptor render_pass_desc_ = {};
  glm::uvec2 texture_size_ = {};
};

}  // namespace skity

#endif  // SRC_RENDER_HW_LAYER_HW_SUB_LAYER_HPP
