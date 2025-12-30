/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_IO_INCLUDE_SKITY_IO_PARSE_PATH_HPP
#define MODULE_IO_INCLUDE_SKITY_IO_PARSE_PATH_HPP

#include <optional>
#include <skity/graphic/path.hpp>

namespace skity {

class SKITY_API ParsePath {
 public:
  static std::optional<Path> FromSVGString(const char str[]);

  enum class PathEncoding { Absolute, Relative };
  static std::string ToSVGString(const Path&,
                                 PathEncoding = PathEncoding::Absolute);
};
}  // namespace skity

#endif  // MODULE_IO_INCLUDE_SKITY_IO_PARSE_PATH_HPP
