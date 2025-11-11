// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_BLUR_FILTER_HPP
#define SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_BLUR_FILTER_HPP

#include <memory>

#include "src/gpu/gpu_texture.hpp"
#include "src/render/hw/draw/hw_wgsl_fragment.hpp"

namespace skity {

class WGSLBlurFilter : public HWWGSLFragment {
 public:
  WGSLBlurFilter(std::shared_ptr<GPUTexture> texture, Vec2 dir, float radius,
                 Vec2 uv_scale, Vec2 uv_offset);

  ~WGSLBlurFilter() override = default;

  std::string GetShaderName() const override;

  std::string GenSourceWGSL() const override;

  uint32_t NextBindingIndex() const override;

  void PrepareCMD(Command *cmd, HWDrawContext *context) override;

 private:
  std::shared_ptr<GPUTexture> texture_;
  Vec2 dir_;
  float radius_;
  Vec2 uv_scale_;
  Vec2 uv_offset_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_BLUR_FILTER_HPP
