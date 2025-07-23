// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/graphic/path_op.hpp>

#include "src/graphic/pathop/path_op_engine.hpp"

namespace skity {

bool PathOp::Execute(const Path &one, const Path &two, Op op, Path *result) {
  if (result == nullptr) {
    return false;
  }

  PathOpEngine engine{};

  if (op == Op::kDifference) {
    return engine.Difference(one, two, result);
  } else if (op == Op::kIntersect) {
    return engine.Intersect(one, two, result);
  } else if (op == Op::kUnion) {
    return engine.Union(one, two, result);
  } else if (op == Op::kXor) {
    return engine.Xor(one, two, result);
  }

  return false;
}

}  // namespace skity
