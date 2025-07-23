// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GRAPHIC_PATH_OP_HPP
#define INCLUDE_SKITY_GRAPHIC_PATH_OP_HPP

#include <skity/graphic/path.hpp>
#include <skity/macros.hpp>

namespace skity {

/**
 * Helper class to do boolean operation on Path.
 */
class SKITY_API PathOp final {
 public:
  PathOp() = delete;
  ~PathOp() = delete;
  PathOp& operator=(const PathOp&) = delete;

  // Logical operations that can be performed when combining two paths
  enum class Op {
    // subtract the op path from the first path
    kDifference,
    // intersect two paths
    kIntersect,
    // union the two paths
    kUnion,
    // exclusive-or the two paths
    kXor,
  };

  /**
   * Do the boolean operation on two paths, and set to the result.
   *
   * The resulting path will be constructed from non-overlapping contours. And
   * the curve may degenerate to lines. And Path direction may changed.
   *
   * @param one     The first operand
   * @param two     The second operand
   * @param op      The operator to apply
   * @param result  The product of the operands.
   * @return true   The operation succeeded
   */
  static bool Execute(Path const& one, Path const& two, Op op, Path* result);
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GRAPHIC_PATH_OP_HPP
