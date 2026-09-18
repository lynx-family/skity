// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_PATH_OP_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_PATH_OP_HPP

// Path boolean-op wrapper of the header-only RAII layer. The legacy API
// exposed this as the static PathOp::Op; here it is a free function in
// skity::raii (zero out-of-line symbols, one header per C domain). See
// skity.hpp for the layer's design.

#include <skity_c/skity_path_op.h>

#include <cstdint>
#include <skity_hpp/skity_path.hpp>

namespace skity {
namespace raii {

/** Boolean operation kind between two paths (legacy skity::PathOp::Op). */
enum class PathOp : uint32_t {
  kDifference = SKITY_PATH_OP_DIFFERENCE,
  kIntersect = SKITY_PATH_OP_INTERSECT,
  kUnion = SKITY_PATH_OP_UNION,
  kXor = SKITY_PATH_OP_XOR,
};

/**
 * Compute the boolean combination of @p one and @p two, overwriting @p out
 * with the product (its previous contents are discarded). The result is
 * built from non-overlapping contours; curves may degenerate to lines and
 * the contour direction may change.
 */
inline bool Op(const Path& one, const Path& two, PathOp op, Path* out) {
  return out != nullptr &&
         skity_path_op_execute(one.get(), two.get(),
                               static_cast<skity_path_op>(op),
                               out->get()) != 0;
}

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_PATH_OP_HPP
