// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_FONT_PROBE_FONT_MANAGER_PROBE_HPP
#define HARNESS_FONT_PROBE_FONT_MANAGER_PROBE_HPP

#include <filesystem>
#include <string>

#include "third_party/jsoncpp/include/json/json.h"

namespace skity {
namespace font_harness {

enum class FontManagerProbeStatus {
  kSuccess,
  kSchemaValidationFailed,
  kBackendUnavailable,
  kProbeFailed,
};

struct FontManagerProbeRequest {
  std::filesystem::path repo_root;
  std::filesystem::path case_path;
  std::string backend;
};

struct FontManagerProbeResult {
  FontManagerProbeStatus status = FontManagerProbeStatus::kProbeFailed;
  std::string case_id;
  std::string backend;
  Json::Value report;
};

FontManagerProbeResult RunFontManagerProbe(
    const FontManagerProbeRequest& request);

}  // namespace font_harness
}  // namespace skity

#endif  // HARNESS_FONT_PROBE_FONT_MANAGER_PROBE_HPP
