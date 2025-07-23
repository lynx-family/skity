// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/graphic/path_measure.hpp>

#include "src/graphic/contour_measure.hpp"

namespace skity {
PathMeasure::PathMeasure() = default;

PathMeasure::PathMeasure(Path const& path, bool forceClosed, float resScale)
    : iter_(new ContourMeasureIter(path, forceClosed, resScale)) {
  contour_ = iter_->next();
}

PathMeasure::~PathMeasure() = default;

void PathMeasure::SetPath(const Path* path, bool forceClosed) {
  if (!iter_) {
    skity::Path tmp{};
    iter_ = std::make_unique<ContourMeasureIter>(path != nullptr ? *path : tmp,
                                                 forceClosed, 1.f);
  }

  iter_->reset(path ? *path : Path{}, forceClosed);
  contour_ = iter_->next();
}

float PathMeasure::GetLength() {
  if (contour_) {
    return contour_->length();
  } else {
    return 0;
  }
}

bool PathMeasure::GetPosTan(float distance, Point* position, Vector* tangent) {
  return contour_ && contour_->getPosTan(distance, position, tangent);
}

bool PathMeasure::GetSegment(float startD, float stopD, Path* dst,
                             bool startWithMoveTo) {
  return contour_ && contour_->getSegment(startD, stopD, dst, startWithMoveTo);
}

bool PathMeasure::IsClosed() { return contour_ && contour_->isClosed(); }

bool PathMeasure::NextContour() {
  contour_ = iter_->next();
  return !!contour_;
}

}  // namespace skity
