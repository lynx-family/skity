// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_FONT_PLATFORM_LINUX_ENV_INFO_HPP
#define HARNESS_FONT_PLATFORM_LINUX_ENV_INFO_HPP

#include <filesystem>
#include <string>

#include "third_party/jsoncpp/include/json/json.h"

namespace skity {
namespace font_harness {

Json::Value BuildLinuxEnvInfo(const std::filesystem::path& repo_root,
                              const std::string& backend, bool list_fonts);

}  // namespace font_harness
}  // namespace skity

#endif  // HARNESS_FONT_PLATFORM_LINUX_ENV_INFO_HPP
