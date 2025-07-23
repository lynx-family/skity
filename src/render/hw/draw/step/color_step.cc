// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/step/color_step.hpp"

namespace skity {

ColorStep::ColorStep(HWWGSLGeometry* geometry, HWWGSLFragment* fragment,
                     CoverageType coverage)
    : HWDrawStep(geometry, fragment, coverage != CoverageType::kNone, false),
      coverage_(coverage) {}

GPUStencilState ColorStep::GetStencilState() {
  if (coverage_ == CoverageType::kNone) {
    return {};
  }

  GPUStencilState state{};

  if (coverage_ == CoverageType::kNoZero) {
    state.front.compare = GPUCompareFunction::kNotEqual;
    state.front.pass_op = GPUStencilOperation::kReplace;

    state.back.compare = GPUCompareFunction::kNotEqual;
    state.back.pass_op = GPUStencilOperation::kReplace;
  } else {
    state.front.compare = GPUCompareFunction::kNotEqual;
    state.front.pass_op = GPUStencilOperation::kReplace;

    state.back.compare = GPUCompareFunction::kNotEqual;
    state.back.pass_op = GPUStencilOperation::kReplace;

    if (coverage_ == CoverageType::kEvenOdd) {
      state.front.stencil_read_mask = 0x01;
      state.back.stencil_read_mask = 0x01;
      state.front.fail_op = GPUStencilOperation::kReplace;
      state.back.fail_op = GPUStencilOperation::kReplace;
    }
  }

  return state;
}

ColorAAStep::ColorAAStep(HWWGSLGeometry* geometry, HWWGSLFragment* fragment,
                         CoverageType coverage)
    : HWDrawStep(geometry, fragment, true, false), coverage_(coverage) {}

GPUStencilState ColorAAStep::GetStencilState() {
  GPUStencilState state{};

  state.front.compare = GPUCompareFunction::kEqual;
  state.front.pass_op = GPUStencilOperation::kKeep;

  state.back.compare = GPUCompareFunction::kEqual;
  state.back.pass_op = GPUStencilOperation::kKeep;

  if (coverage_ == CoverageType::kEvenOdd) {
    state.front.stencil_read_mask = 0x01;
    state.back.stencil_read_mask = 0x01;
  }

  return state;
}

}  // namespace skity
