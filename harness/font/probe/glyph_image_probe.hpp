// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_FONT_PROBE_GLYPH_IMAGE_PROBE_HPP
#define HARNESS_FONT_PROBE_GLYPH_IMAGE_PROBE_HPP

#include <filesystem>
#include <string>

#include "third_party/jsoncpp/include/json/json.h"

namespace skity {
namespace font_harness {

enum class GlyphImageProbeStatus {
  kSuccess,
  kSchemaValidationFailed,
  kBackendUnavailable,
  kProbeFailed,
};

struct GlyphImageProbeRequest {
  std::filesystem::path repo_root;
  std::filesystem::path case_path;
  std::string backend;
};

struct GlyphImageProbeResult {
  GlyphImageProbeStatus status = GlyphImageProbeStatus::kProbeFailed;
  std::string case_id;
  std::string backend;
  Json::Value report;
};

GlyphImageProbeResult RunGlyphImageProbe(const GlyphImageProbeRequest& request);

}  // namespace font_harness
}  // namespace skity

#endif  // HARNESS_FONT_PROBE_GLYPH_IMAGE_PROBE_HPP
