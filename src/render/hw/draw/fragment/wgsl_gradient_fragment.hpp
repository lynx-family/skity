// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_GRADIENT_FRAGMENT_HPP
#define SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_GRADIENT_FRAGMENT_HPP

#include <skity/effect/shader.hpp>

#include "src/render/hw/draw/hw_wgsl_fragment.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"

namespace skity {

class WGSLGradientFragment : public HWWGSLFragment {
 public:
  WGSLGradientFragment(Shader::GradientInfo info, Shader::GradientType type,
                       float global_alpha);

  ~WGSLGradientFragment() override = default;

  uint32_t NextBindingIndex() const override;

  std::string GetShaderName() const override;

  std::string GenSourceWGSL() const override;

  const char* GetEntryPoint() const override;

  void PrepareCMD(Command* cmd, HWDrawContext* context) override;

 private:
  Shader::GradientInfo info_;
  Shader::GradientType type_;
  float global_alpha_ = 1.0f;
  WGXGradientFragment gradient_fragment_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_GRADIENT_FRAGMENT_HPP
