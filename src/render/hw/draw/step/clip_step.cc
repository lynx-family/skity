// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/step/clip_step.hpp"

namespace skity {

ClipStep::ClipStep(HWWGSLGeometry* geometry, HWWGSLFragment* fragment,
                   Path::PathFillType fill_type, Canvas::ClipOp op)
    : HWDrawStep(geometry, fragment, true, true),
      fill_type_(fill_type),
      op_(op) {}

GPUStencilState ClipStep::GetStencilState() {
  GPUStencilState state{};

  if (op_ == Canvas::ClipOp::kDifference) {
    // difference needs to write depth value into stencil marked fragments
    state.front.compare = GPUCompareFunction::kNotEqual;
    state.front.pass_op = GPUStencilOperation::kReplace;
    state.front.fail_op = GPUStencilOperation::kReplace;

    state.back.compare = GPUCompareFunction::kNotEqual;
    state.back.pass_op = GPUStencilOperation::kReplace;
    state.back.fail_op = GPUStencilOperation::kReplace;
  } else {
    // normal clip needs to write depth value into non-marked fragments
    state.front.compare = GPUCompareFunction::kEqual;
    state.front.pass_op = GPUStencilOperation::kReplace;
    state.front.fail_op = GPUStencilOperation::kReplace;
    state.back.compare = GPUCompareFunction::kEqual;
    state.back.pass_op = GPUStencilOperation::kReplace;
    state.back.fail_op = GPUStencilOperation::kReplace;
  }

  if (fill_type_ == Path::PathFillType::kEvenOdd) {
    state.front.stencil_read_mask = 0x01;
    state.back.stencil_read_mask = 0x01;
  }

  return state;
}

}  // namespace skity
