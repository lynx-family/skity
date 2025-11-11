// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_SOLID_COLOR_HPP
#define SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_SOLID_COLOR_HPP

#include <skity/graphic/color.hpp>

#include "src/render/hw/draw/hw_wgsl_fragment.hpp"

namespace skity {

class WGSLSolidColor : public HWWGSLFragment {
 public:
  explicit WGSLSolidColor(const Color4f& color);

  ~WGSLSolidColor() override = default;

  uint32_t NextBindingIndex() const override;

  std::string GetShaderName() const override;

  void WriteFSUniforms(std::stringstream& ss) const override;

  void WriteFSMain(std::stringstream& ss) const override;

  void PrepareCMD(Command* cmd, HWDrawContext* context) override;

 private:
  Color4f color_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_SOLID_COLOR_HPP
