// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "test/bench/common/bench_target.hpp"

namespace skity {

Canvas *BenchTarget::LockCanvas() {
  canvas_ = surface_->LockCanvas();
  return canvas_;
}

void BenchTarget::Flush() {
  canvas_->Flush();
  surface_->Flush();
}

}  // namespace skity
