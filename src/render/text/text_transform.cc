// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/text_transform.hpp"

namespace skity {

void Matrix22::QRDecompose(Matrix22* q, Matrix22* r) const {
  const float& a = scale_x_;
  const float& b = skew_y_;
  float alpha = sqrt(a * a + b * b);
  if (FloatNearlyZero(alpha)) {
    q->scale_x_ = 1.f;
    q->skew_x_ = 0.f;
    q->skew_y_ = 0.f;
    q->scale_y_ = 1.f;
  } else {
    if (a >= 0) alpha = -alpha;
    float rr = 0.5 * (1 - a / alpha);
    float sr = (b / (2 * alpha));

    // Matrix H
    q->scale_x_ = 2 * rr - 1;
    q->skew_x_ = -2 * sr;
    q->skew_y_ = -q->skew_x_;
    q->scale_y_ = q->scale_x_;
  }

  *r = *q * (*this);
}

void Matrix22::MapPoints(Vec2* dst, const Vec2* src, int count) const {
  for (int i = 0; i < count; ++i) {
    dst[i] = (*this) * src[i];
  }
}

}  // namespace skity
