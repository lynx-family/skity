// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_FONT_ARTIFACT_JSON_IO_HPP
#define HARNESS_FONT_ARTIFACT_JSON_IO_HPP

#include <filesystem>
#include <string>

#include "third_party/jsoncpp/include/json/json.h"

namespace skity {
namespace font_harness {

bool LoadJsonFile(const std::filesystem::path& path, Json::Value* root,
                  std::string* error);
bool WriteJsonFile(const std::filesystem::path& path, const Json::Value& root,
                   std::string* error);
std::string WriteJsonString(const Json::Value& root);

}  // namespace font_harness
}  // namespace skity

#endif  // HARNESS_FONT_ARTIFACT_JSON_IO_HPP
