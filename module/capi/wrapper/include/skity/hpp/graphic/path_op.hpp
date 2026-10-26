// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_WRAPPER_INCLUDE_SKITY_HPP_GRAPHIC_PATH_OP_HPP
#define MODULE_CAPI_WRAPPER_INCLUDE_SKITY_HPP_GRAPHIC_PATH_OP_HPP

#include <skity/c/skity_path_op.h>

#include <cstdint>
#include <skity/hpp/graphic/path.hpp>

namespace skity {

/** Boolean operation kind between two paths (legacy skity::PathOp::Op). */
enum class PathOp : uint32_t {
  kDifference = SKITY_PATH_OP_DIFFERENCE,
  kIntersect = SKITY_PATH_OP_INTERSECT,
  kUnion = SKITY_PATH_OP_UNION,
  kXor = SKITY_PATH_OP_XOR,
};

/**
 * Compute the boolean combination of one and two, overwriting out
 * with the product (its previous contents are discarded). The result is
 * built from non-overlapping contours; curves may degenerate to lines and
 * the contour direction may change.
 */
inline bool Op(const Path& one, const Path& two, PathOp op, Path* out) {
  return out != nullptr &&
         skity_path_op_execute(one.get(), two.get(),
                               static_cast<skity_path_op>(op), out->get()) != 0;
}

}  // namespace skity

#endif  // MODULE_CAPI_WRAPPER_INCLUDE_SKITY_HPP_GRAPHIC_PATH_OP_HPP
