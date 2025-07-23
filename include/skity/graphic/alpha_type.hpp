/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GRAPHIC_ALPHA_TYPE_HPP
#define INCLUDE_SKITY_GRAPHIC_ALPHA_TYPE_HPP

namespace skity {

/** \enum AlphaType
    Describes how to interpret the alpha component of a pixel. A pixel may
    be opaque, or alpha, describing multiple levels of transparency.

    In simple blending, alpha weights the draw color and the destination
    color to create a new color. If alpha describes a weight from zero to one:

    new color = draw color * alpha + destination color * (1 - alpha)

    In practice alpha is encoded in two or more bits, where 1.0 equals all bits
   set.

    RGB may have alpha included in each component value; the stored
    value is the original RGB multiplied by alpha. Premultiplied color
    components improve performance.
*/
enum AlphaType : int {
  kUnknown_AlphaType,   //!< uninitialized
  kOpaque_AlphaType,    //!< pixel is opaque
  kPremul_AlphaType,    //!< pixel components are premultiplied by alpha
  kUnpremul_AlphaType,  //!< pixel components are independent of alpha
  kLastEnum_AlphaType = kUnpremul_AlphaType,  //!< last valid value
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GRAPHIC_ALPHA_TYPE_HPP
