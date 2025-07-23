// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_PATH_GEOMETRY_HPP
#define SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_PATH_GEOMETRY_HPP

#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>

#include "src/render/hw/draw/hw_wgsl_geometry.hpp"

namespace skity {

class WGSLPathGeometry : public HWWGSLGeometry {
 public:
  WGSLPathGeometry(const Path& path, const Paint& paint, bool is_stroke,
                   bool contour_aa);

  ~WGSLPathGeometry() override = default;

  const std::vector<GPUVertexBufferLayout>& GetBufferLayout() const override;

  std::string GenSourceWGSL() const override;

  std::string GetShaderName() const override;

  const char* GetEntryPoint() const override;

  void PrepareCMD(Command* cmd, HWDrawContext* context, const Matrix& transform,
                  float clip_depth, Command* stencil_cmd) override;

 protected:
  bool IsContourAA() const { return contour_aa_; }

 private:
  const Path& path_;
  const Paint& paint_;
  bool is_stroke_;
  bool contour_aa_;
  std::vector<GPUVertexBufferLayout> layout_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_PATH_GEOMETRY_HPP
