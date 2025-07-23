// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GRAPHIC_PATHOP_PATH_OP_ENGINE_HPP
#define SRC_GRAPHIC_PATHOP_PATH_OP_ENGINE_HPP

#include <skity/graphic/path.hpp>
#include <skity/graphic/path_op.hpp>

namespace skity {

class PathOpEngine final {
 public:
  PathOpEngine() = default;
  ~PathOpEngine() = default;

  bool Union(Path const& one, Path const& two, Path* result);

  bool Intersect(Path const& one, Path const& two, Path* result);

  bool Xor(Path const& one, Path const& two, Path* result);

  bool Difference(Path const& one, Path const& two, Path* result);

 private:
  PathOp::Op op_type_ = PathOp::Op::kIntersect;

 private:
  bool ExecuteInternal(Path const& one, Path const& two, Path* result);
};

}  // namespace skity

#endif  // SRC_GRAPHIC_PATHOP_PATH_OP_ENGINE_HPP
