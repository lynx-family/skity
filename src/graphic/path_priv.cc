/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/graphic/path_priv.hpp"

namespace skity {

void PathPriv::CreateDrawArcPath(Path *path, const Rect &oval, float startAngle,
                                 float sweepAngle, bool useCenter,
                                 bool isFillNoPathEffect) {
  if (isFillNoPathEffect && sweepAngle >= 360.f) {
    path->AddOval(oval);
    return;
  }

  if (useCenter) {
    path->MoveTo(oval.CenterX(), oval.CenterY());
  }

  bool forceMoveTo = !useCenter;
  while (sweepAngle <= -360.f) {
    path->ArcTo(oval, startAngle, -180.f, forceMoveTo);
    startAngle -= 180.f;
    path->ArcTo(oval, startAngle, -180.f, false);

    startAngle -= 180.f;
    forceMoveTo = false;
    sweepAngle += 360.f;
  }

  while (sweepAngle >= 360.f) {
    path->ArcTo(oval, startAngle, 180.f, forceMoveTo);
    startAngle += 180.f;
    path->ArcTo(oval, startAngle, 180.f, false);
    startAngle += 180.f;
    forceMoveTo = false;
    sweepAngle -= 360.f;
  }

  path->ArcTo(oval, startAngle, sweepAngle, forceMoveTo);
  if (useCenter) {
    path->Close();
  }
}

int32_t PathPriv::RectMakeDir(float dx, float dy) {
  return ((0 != dx) << 0) | ((dx > 0 || dy > 0) << 1);
}

}  // namespace skity
