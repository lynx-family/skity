// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_SHAPE_HPP
#define SRC_RENDER_SHAPE_HPP

#include <skity/graphic/path.hpp>

#include "src/logging.hpp"

namespace skity {
class Shape {
 public:
  enum class Type {
    kPath,
    kRRect,
  };

  explicit Shape(const Path* path) : type_(Type::kPath), data_(path) {}
  explicit Shape(const RRect* rrect) : type_(Type::kRRect), data_(rrect) {}

  ~Shape() = default;

  bool IsPath() const { return type_ == Type::kPath; }

  const Path* GetPath() const {
    DEBUG_CHECK(type_ == Type::kPath);
    return reinterpret_cast<const Path*>(data_);
  }

  bool IsRRect() const { return type_ == Type::kRRect; }

  const RRect* GetRRect() const {
    DEBUG_CHECK(type_ == Type::kRRect);
    return reinterpret_cast<const RRect*>(data_);
  }

  Rect GetBounds() const {
    switch (type_) {
      case Type::kPath:
        return GetPath()->GetBounds();
      case Type::kRRect:
        return GetRRect()->GetBounds();
    }
  }

 private:
  Type type_;
  const void* data_;
};
}  // namespace skity

#endif  // SRC_RENDER_SHAPE_HPP
