// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/graphic/path_builder.hpp>
#include <utility>

namespace skity {

PathBuilder::PathBuilder(const Path& src) : path_(src) {
  path_.SetConvexityType(Path::ConvexityType::kUnknown);
}

PathBuilder::PathBuilder(Path&& src) : path_(std::move(src)) {
  path_.SetConvexityType(Path::ConvexityType::kUnknown);
  src.Reset();
}

PathBuilder::PathBuilder(PathBuilder&& other) noexcept
    : path_(std::move(other.path_)) {
  other.Reset();
}

PathBuilder& PathBuilder::operator=(PathBuilder&& other) noexcept {
  if (this != &other) {
    Swap(other);
    other.Reset();
  }
  return *this;
}

void PathBuilder::Reserve(size_t point_count, size_t verb_count,
                          size_t conic_weight_count) {
  path_.points_.reserve(point_count);
  path_.verbs_.reserve(verb_count);
  path_.conic_weights_.reserve(conic_weight_count);
}

PathBuilder& PathBuilder::ReversePathTo(const PathBuilder& src) {
  // Self-append must not invalidate the source pointers during growth.
  if (this == &src) {
    Path copy = Snapshot();
    path_.ReversePathTo(copy);
  } else {
    path_.ReversePathTo(src.path_);
  }
  return *this;
}

Path PathBuilder::Snapshot() const { return Path(path_); }

Path PathBuilder::Detach() {
  Path result(std::move(path_));
  path_.Reset();
  return result;
}

void PathBuilder::DetachTo(Path* dst) {
  if (dst) {
    dst->Swap(path_);
    path_.Reset();
  }
}

void PathBuilder::Swap(PathBuilder& other) noexcept { path_.Swap(other.path_); }

}  // namespace skity
