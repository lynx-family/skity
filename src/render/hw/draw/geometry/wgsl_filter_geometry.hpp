// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_FILTER_GEOMETRY_HPP
#define SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_FILTER_GEOMETRY_HPP

#include <array>

#include "src/render/hw/draw/hw_wgsl_geometry.hpp"

namespace skity {

class WGSLFilterGeometry : public HWWGSLGeometry {
 public:
  WGSLFilterGeometry(float u_factor, float v_factor);

  WGSLFilterGeometry(float u_factor, float v_factor,
                     const std::array<float, 8> &vertex_buffer);

  ~WGSLFilterGeometry() override = default;

  const std::vector<GPUVertexBufferLayout> &GetBufferLayout() const override;

  std::string GetShaderName() const override;

  std::string GenSourceWGSL() const override;

  void PrepareCMD(Command *cmd, HWDrawContext *context, const Matrix &transform,
                  float clip_depth, Command *stencil_cmd) override;

 private:
  float u_factor_;
  float v_factor_;
  std::array<float, 8> vertex_buffer_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_FILTER_GEOMETRY_HPP
