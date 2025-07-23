// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GEOMETRY_STROKE_HPP
#define INCLUDE_SKITY_GEOMETRY_STROKE_HPP

#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>

namespace skity {

class SKITY_API Stroke {
 public:
  explicit Stroke(const Paint& paint);

  void StrokePath(const Path& src, Path* dst) const;

  void QuadPath(const Path& src, Path* dst) const;

 private:
  float width_;
  float miter_limit_;
  Paint::Cap cap_;
  Paint::Join join_;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GEOMETRY_STROKE_HPP
