/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_TEXT_FONT_METRICS_HPP
#define INCLUDE_SKITY_TEXT_FONT_METRICS_HPP

#include <skity/macros.hpp>

namespace skity {
class SKITY_API FontMetrics {
 public:
  float top_;      //!< greatest extent above origin of any glyph bounding box,
                   //!< typically negative; deprecated with variable fonts
  float ascent_;   //!< distance to reserve above baseline, typically negative
  float descent_;  //!< distance to reserve below baseline, typically positive
  float bottom_;   //!< greatest extent below origin of any glyph bounding box,
                   //!< typically positive; deprecated with variable fonts
  float
      leading_;  //!< distance to add between lines, typically positive or zero
  float avg_char_width_;  //!< average character width, zero if unknown
  float max_char_width_;  //!< maximum character width, zero if unknown
  float x_min_;  //!< greatest extent to left of origin of any glyph bounding
                 //!< box, typically negative; deprecated with variable fonts
  float x_max_;  //!< greatest extent to right of origin of any glyph bounding
                 //!< box, typically positive; deprecated with variable fonts
  float x_height_;    //!< height of lower-case 'x', zero if unknown, typically
                      //!< negative
  float cap_height_;  //!< height of an upper-case letter, zero if unknown,
                      //!< typically negativeÏ
  float underline_thickness_;  //!< underline thickness
  float underline_position_;   //!< distance from baseline to top of stroke,
                               //!< typically positive
  float strikeout_thickness_;  //!< strikeout thickness
  float strikeout_position_;   //!< distance from baseline to bottom of stroke,
                               //!< typically negative
};

}  // namespace skity

#endif  // INCLUDE_SKITY_TEXT_FONT_METRICS_HPP
