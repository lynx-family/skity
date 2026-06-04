// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_FONT_PROBE_GLYPH_PATH_PROBE_HPP
#define HARNESS_FONT_PROBE_GLYPH_PATH_PROBE_HPP

#include <filesystem>
#include <string>

#include "third_party/jsoncpp/include/json/json.h"

namespace skity {
namespace font_harness {

enum class GlyphPathProbeStatus {
  kSuccess,
  kSchemaValidationFailed,
  kBackendUnavailable,
  kProbeFailed,
};

struct GlyphPathProbeRequest {
  std::filesystem::path repo_root;
  std::filesystem::path case_path;
  std::string backend;
};

struct GlyphPathProbeResult {
  GlyphPathProbeStatus status = GlyphPathProbeStatus::kProbeFailed;
  std::string case_id;
  std::string backend;
  Json::Value report;
};

GlyphPathProbeResult RunGlyphPathProbe(const GlyphPathProbeRequest& request);
GlyphPathProbeResult RunGlyphPathDump(const GlyphPathProbeRequest& request);

}  // namespace font_harness
}  // namespace skity

#endif  // HARNESS_FONT_PROBE_GLYPH_PATH_PROBE_HPP
