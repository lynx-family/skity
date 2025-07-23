// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/step/stencil_step.hpp"

namespace skity {

StencilStep::StencilStep(HWWGSLGeometry* geometry, HWWGSLFragment* fragment,
                         bool no_zero)
    : HWDrawStep(geometry, fragment, true, false), no_zero_(no_zero) {}

GPUStencilState StencilStep::GetStencilState() {
  GPUStencilState state{};

  if (no_zero_) {
    state.front.compare = GPUCompareFunction::kEqual;
    state.front.pass_op = GPUStencilOperation::kIncrementWrap;

    state.back.compare = GPUCompareFunction::kEqual;
    state.back.pass_op = GPUStencilOperation::kIncrementWrap;
  } else {
    state.front.compare = GPUCompareFunction::kAlways;
    state.front.pass_op = GPUStencilOperation::kIncrementWrap;

    state.back.compare = GPUCompareFunction::kAlways;
    state.back.pass_op = GPUStencilOperation::kDecrementWrap;
  }

  return state;
}

}  // namespace skity
