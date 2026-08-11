// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PATH_OP_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PATH_OP_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_path.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Boolean operation kind applied between two paths. Values are
 *         aligned with skity::PathOp::Op. */
typedef enum {
  SKITY_PATH_OP_DIFFERENCE = 0, /**< subtract @p two from @p one */
  SKITY_PATH_OP_INTERSECT,      /**< keep only the overlap of the two paths */
  SKITY_PATH_OP_UNION,          /**< combine the two paths into one */
  SKITY_PATH_OP_XOR,            /**< keep the non-overlapping parts of both */
} skity_path_op;

/**
 * @brief Compute the boolean combination of @p one and @p two, overwriting
 *        @p result with the product. @p result must be an existing skity_path;
 *        its previous contents are discarded. The result is constructed from
 *        non-overlapping contours; curves may degenerate to lines and the
 *        contour direction may change.
 *
 * @param one     the first operand
 * @param two     the second operand
 * @param op      the operation to apply
 * @param result  destination path that receives the product
 * @return 1 on success, 0 on failure or if any handle is null or wrong type.
 */
SKITY_C_API uint32_t skity_path_op_execute(skity_path one, skity_path two,
                                           skity_path_op op, skity_path result);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PATH_OP_H
