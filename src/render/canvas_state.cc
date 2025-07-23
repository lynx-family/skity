// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/canvas_state.hpp"

namespace skity {

void LayerState::Save() { elements_.emplace_back(CurrentMatrix()); }

void LayerState::Restore() {
  if (!CanRestore()) {
    return;
  }
  elements_.pop_back();
}

void LayerState::Translate(float dx, float dy) {
  elements_.back().matrix = CurrentMatrix() * Matrix::Translate(dx, dy);
}

void LayerState::Scale(float sx, float sy) {
  elements_.back().matrix = CurrentMatrix() * Matrix::Scale(sx, sy);
}

void LayerState::Rotate(float degree) {
  elements_.back().matrix =
      CurrentMatrix() * Matrix::RotateDeg(degree, Vec2{0, 0});
}

void LayerState::Rotate(float degree, float px, float py) {
  elements_.back().matrix =
      CurrentMatrix() * Matrix::RotateDeg(degree, Vec2{px, py});
}

void LayerState::Skew(float sx, float sy) {
  elements_.back().matrix = CurrentMatrix() * Matrix::Skew(sx, sy);
}

void LayerState::Concat(const Matrix& matrix) {
  elements_.back().matrix = CurrentMatrix() * matrix;
}

void LayerState::SetMatrix(const Matrix& matrix) {
  elements_.back().matrix = matrix;
}

void LayerState::ResetMatrix() { elements_.back().matrix = Matrix{}; }

}  // namespace skity
