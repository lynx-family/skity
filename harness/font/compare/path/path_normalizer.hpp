// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_FONT_COMPARE_PATH_PATH_NORMALIZER_HPP
#define HARNESS_FONT_COMPARE_PATH_PATH_NORMALIZER_HPP

#include <skity/graphic/path.hpp>

#include "third_party/jsoncpp/include/json/json.h"

namespace skity {
namespace font_harness {

inline constexpr double kDefaultPathEpsilon = 0.0001;

struct PathNormalizeOptions {
  double epsilon = kDefaultPathEpsilon;
  bool drop_zero_length_segments = true;
};

Json::Value BuildNormalizedPathJson(
    const Path& path,
    const PathNormalizeOptions& options = PathNormalizeOptions{});

}  // namespace font_harness
}  // namespace skity

#endif  // HARNESS_FONT_COMPARE_PATH_PATH_NORMALIZER_HPP
