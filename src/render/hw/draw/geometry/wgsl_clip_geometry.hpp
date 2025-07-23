// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_CLIP_GEOMETRY_HPP
#define SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_CLIP_GEOMETRY_HPP

#include <skity/render/canvas.hpp>

#include "src/render/hw/draw/geometry/wgsl_path_geometry.hpp"

namespace skity {

class WGSLClipGeometry : public WGSLPathGeometry {
 public:
  WGSLClipGeometry(const Path& path, const Paint& paint, bool is_stroke,
                   Canvas::ClipOp op);

  ~WGSLClipGeometry() override = default;

  void PrepareCMD(Command* cmd, HWDrawContext* context, const Matrix& transform,
                  float clip_depth, Command* stencil_cmd) override;

 private:
  Canvas::ClipOp op_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_CLIP_GEOMETRY_HPP
