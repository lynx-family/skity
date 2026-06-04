// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_FONT_PLATFORM_CORETEXT_ENV_INFO_HPP
#define HARNESS_FONT_PLATFORM_CORETEXT_ENV_INFO_HPP

#include <filesystem>

#include "third_party/jsoncpp/include/json/json.h"

namespace skity {
namespace font_harness {

struct CoreTextEnvRequest {
  std::filesystem::path repo_root;
};

Json::Value BuildCoreTextEnvInfo(const CoreTextEnvRequest& request);
Json::Value BuildCoreTextFontList(const CoreTextEnvRequest& request);

}  // namespace font_harness
}  // namespace skity

#endif  // HARNESS_FONT_PLATFORM_CORETEXT_ENV_INFO_HPP
