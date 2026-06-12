// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_FONT_CASE_PLATFORM_TARGET_HPP
#define HARNESS_FONT_CASE_PLATFORM_TARGET_HPP

#include <string>
#include <vector>

#include "third_party/jsoncpp/include/json/json.h"

namespace skity {
namespace font_harness {

struct PlatformTargetInfo {
  std::string id;
  std::string backend;
  std::string runner;
};

const std::vector<std::string>& AllowedBackendIds();

std::string CanonicalPlatformTarget(const std::string& platform);

const PlatformTargetInfo* FindPlatformTargetInfo(const std::string& platform);

bool PlatformTargetMatchesBackend(const std::string& backend,
                                  const std::string& platform);

bool PlatformArrayMatchesBackend(const std::string& backend,
                                 const Json::Value& platforms);

bool PlatformArrayContainsTarget(const Json::Value& platforms,
                                 const std::string& target_platform);

std::string InferTargetPlatformFromArray(const Json::Value& platforms,
                                         const std::string& backend);

}  // namespace font_harness
}  // namespace skity

#endif  // HARNESS_FONT_CASE_PLATFORM_TARGET_HPP
