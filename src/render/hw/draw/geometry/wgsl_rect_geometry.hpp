// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_RECT_GEOMETRY_HPP
#define SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_RECT_GEOMETRY_HPP

#include <skity/geometry/vector.hpp>
#include <vector>

#include "src/render/hw/draw/hw_wgsl_geometry.hpp"

namespace skity {

class WGSLRectGeometry : public HWWGSLGeometry {
 public:
  struct Instance {
    Vec4 rect;
    Vec4 scale_translate;
    Vec4 color;
  };

  WGSLRectGeometry(const Instance& first_instance,
                   const std::vector<Instance>& additional_instances);

  static std::vector<GPUVertexBufferLayout> GetBufferLayout();

  HWFunctionBaseKey GetMainKey() const override {
    return HWGeometryKeyType::kRect;
  }

  void WriteVSFunctionsAndStructs(std::stringstream& ss) const override;
  void WriteVSUniforms(std::stringstream& ss) const override;
  void WriteVSInput(std::stringstream& ss) const override;
  void WriteVSMain(std::stringstream& ss) const override;

  void PrepareCMD(Command* cmd, HWDrawContext* context, const Matrix& transform,
                  float clip_depth, Command* stencil_cmd) override;

 private:
  const Instance& first_instance_;
  const std::vector<Instance>& additional_instances_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_RECT_GEOMETRY_HPP
