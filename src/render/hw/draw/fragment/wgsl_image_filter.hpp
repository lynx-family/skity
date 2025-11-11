// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_IMAGE_FILTER_HPP
#define SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_IMAGE_FILTER_HPP

#include "src/gpu/gpu_texture.hpp"
#include "src/render/hw/draw/hw_wgsl_fragment.hpp"

namespace skity {

/**
 * Root class for all image filter which used to combine other Color filter, or
 * just blit the origin image source
 */
class WGSLImageFilter : public HWWGSLFragment {
 public:
  explicit WGSLImageFilter(std::shared_ptr<GPUTexture> texture);

  ~WGSLImageFilter() override = default;

  std::string GetShaderName() const override;

  std::string GenSourceWGSL() const override;

  uint32_t NextBindingIndex() const override;

  void PrepareCMD(Command *cmd, HWDrawContext *context) override;

 private:
  std::shared_ptr<GPUTexture> texture_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_IMAGE_FILTER_HPP
