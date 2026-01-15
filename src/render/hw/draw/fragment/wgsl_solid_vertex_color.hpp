// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_SOLID_VERTEX_COLOR_HPP
#define SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_SOLID_VERTEX_COLOR_HPP

#include <optional>
#include <skity/graphic/color.hpp>

#include "src/render/hw/draw/hw_wgsl_fragment.hpp"

namespace skity {

class WGSLSolidVertexColor : public HWWGSLFragment {
 public:
  explicit WGSLSolidVertexColor();

  ~WGSLSolidVertexColor() override = default;

  uint32_t NextBindingIndex() const override;

  HWFunctionBaseKey GetMainKey() const override;

  std::optional<std::vector<std::string>> GetVarings() const override;

  void WriteVSAssgnShadingVarings(std::stringstream& ss) const override;

  void WriteFSMain(std::stringstream& ss) const override;

  void PrepareCMD(Command* cmd, HWDrawContext* context) override;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_SOLID_VERTEX_COLOR_HPP
