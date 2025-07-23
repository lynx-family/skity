// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/geometry/wgsl_clip_geometry.hpp"

#include "src/tracing.hpp"

namespace skity {

WGSLClipGeometry::WGSLClipGeometry(const Path& path, const Paint& paint,
                                   bool is_stroke, Canvas::ClipOp op)
    : WGSLPathGeometry(path, paint, is_stroke, false), op_(op) {}

void WGSLClipGeometry::PrepareCMD(Command* cmd, HWDrawContext* context,
                                  const Matrix& transform, float clip_depth,
                                  Command* stencil_cmd) {
  SKITY_TRACE_EVENT(WGSLClipGeometry_PrepareCMD);

  if (op_ == Canvas::ClipOp::kIntersect) {
    stencil_cmd = nullptr;
  }

  Matrix identity = Matrix{};

  WGSLPathGeometry::PrepareCMD(cmd, context, identity, clip_depth, stencil_cmd);
}

}  // namespace skity
