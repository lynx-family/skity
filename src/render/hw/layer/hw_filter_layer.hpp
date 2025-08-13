// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_LAYER_HW_FILTER_LAYER_HPP
#define SRC_RENDER_HW_LAYER_HW_FILTER_LAYER_HPP

#include <memory>

#include "src/gpu/gpu_command_buffer.hpp"
#include "src/render/hw/filters/hw_filter.hpp"
#include "src/render/hw/layer/hw_sub_layer.hpp"

namespace skity {
class HWFilterLayer : public HWSubLayer {
 public:
  HWFilterLayer(Matrix matrix, int32_t depth, Rect bounds, uint32_t width,
                uint32_t height, std::shared_ptr<HWFilter> filter);

 protected:
  HWDrawState OnPrepare(HWDrawContext* context) override;

  void OnGenerateCommand(HWDrawContext* context, HWDrawState state) override;

  void OnPostDraw(GPURenderPass* render_pass, GPUCommandBuffer* cmd) override;

  Rect GetLayerBackDrawBounds() override { return filted_bounds_; }

 private:
  std::shared_ptr<HWFilter> filter_;
  std::shared_ptr<GPUCommandBuffer> command_buffer_;
  Rect filted_bounds_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_LAYER_HW_FILTER_LAYER_HPP
