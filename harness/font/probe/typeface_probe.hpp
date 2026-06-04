// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_FONT_PROBE_TYPEFACE_PROBE_HPP
#define HARNESS_FONT_PROBE_TYPEFACE_PROBE_HPP

#include <filesystem>
#include <string>

#include "third_party/jsoncpp/include/json/json.h"

namespace skity {
namespace font_harness {

enum class TypefaceProbeStatus {
  kSuccess,
  kSchemaValidationFailed,
  kBackendUnavailable,
  kProbeFailed,
};

struct TypefaceProbeRequest {
  std::filesystem::path repo_root;
  std::filesystem::path case_path;
  std::string backend;
};

struct TypefaceProbeResult {
  TypefaceProbeStatus status = TypefaceProbeStatus::kProbeFailed;
  std::string case_id;
  std::string backend;
  Json::Value report;
};

TypefaceProbeResult RunTypefaceProbe(const TypefaceProbeRequest& request);

}  // namespace font_harness
}  // namespace skity

#endif  // HARNESS_FONT_PROBE_TYPEFACE_PROBE_HPP
