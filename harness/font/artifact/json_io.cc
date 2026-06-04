// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/artifact/json_io.hpp"

#include <fstream>
#include <sstream>
#include <system_error>

namespace skity {
namespace font_harness {

bool LoadJsonFile(const std::filesystem::path& path, Json::Value* root,
                  std::string* error) {
  std::ifstream input(path);
  if (!input.is_open()) {
    if (error != nullptr) {
      *error = "failed to open JSON file: " + path.string();
    }
    return false;
  }

  Json::CharReaderBuilder builder;
  builder["collectComments"] = false;
  std::string parse_errors;
  if (!Json::parseFromStream(builder, input, root, &parse_errors)) {
    if (error != nullptr) {
      *error = parse_errors;
    }
    return false;
  }
  return true;
}

bool WriteJsonFile(const std::filesystem::path& path, const Json::Value& root,
                   std::string* error) {
  std::error_code ec;
  auto parent = path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      if (error != nullptr) {
        *error = "failed to create report directory: " + ec.message();
      }
      return false;
    }
  }

  std::ofstream output(path);
  if (!output.is_open()) {
    if (error != nullptr) {
      *error = "failed to open output JSON file: " + path.string();
    }
    return false;
  }
  output << WriteJsonString(root);
  if (!output.good()) {
    if (error != nullptr) {
      *error = "failed to write output JSON file: " + path.string();
    }
    return false;
  }
  return true;
}

std::string WriteJsonString(const Json::Value& root) {
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "  ";
  return Json::writeString(builder, root);
}

}  // namespace font_harness
}  // namespace skity
