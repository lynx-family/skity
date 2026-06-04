// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_FONT_COMPARE_COMPARE_ENGINE_HPP
#define HARNESS_FONT_COMPARE_COMPARE_ENGINE_HPP

#include <filesystem>
#include <string>

#include "third_party/jsoncpp/include/json/json.h"

namespace skity {
namespace font_harness {

enum class CompareStatus {
  kPass,
  kMismatch,
  kInputFailed,
};

struct CompareRequest {
  std::filesystem::path repo_root;
  std::filesystem::path case_path;
  std::filesystem::path expected_path;
  std::filesystem::path actual_path;
  std::string backend;
};

struct CompareResult {
  CompareStatus status = CompareStatus::kInputFailed;
  std::string case_id;
  std::string backend;
  Json::Value report;
};

CompareResult RunCompare(const CompareRequest& request);

}  // namespace font_harness
}  // namespace skity

#endif  // HARNESS_FONT_COMPARE_COMPARE_ENGINE_HPP
