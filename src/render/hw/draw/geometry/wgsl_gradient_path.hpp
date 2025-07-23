// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_GRADIENT_PATH_HPP
#define SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_GRADIENT_PATH_HPP

#include "src/render/hw/draw/geometry/wgsl_path_geometry.hpp"

namespace skity {

class WGSLGradientPath : public WGSLPathGeometry {
 public:
  WGSLGradientPath(const Path& path, const Paint& paint, bool is_stroke,
                   bool contour_aa, const Matrix& local_matrix);

  ~WGSLGradientPath() override = default;

  std::string GenSourceWGSL() const override;

  std::string GetShaderName() const override;

  const char* GetEntryPoint() const override;

  void PrepareCMD(Command* cmd, HWDrawContext* context, const Matrix& transform,
                  float clip_depth, Command* stencil_cmd) override;

 private:
  Matrix local_matrix_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_GRADIENT_PATH_HPP
