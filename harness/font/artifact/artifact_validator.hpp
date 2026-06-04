// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_FONT_ARTIFACT_ARTIFACT_VALIDATOR_HPP
#define HARNESS_FONT_ARTIFACT_ARTIFACT_VALIDATOR_HPP

#include <string>

#include "harness/font/case/validation.hpp"

namespace skity {
namespace font_harness {

struct ArtifactValidationResult {
  bool valid = false;
  std::string artifact_type;
  std::string case_id;
  ValidationContext errors;
};

ArtifactValidationResult ValidateProbeResultDocument(const Json::Value& root);
ArtifactValidationResult ValidateCompareReportDocument(const Json::Value& root);

}  // namespace font_harness
}  // namespace skity

#endif  // HARNESS_FONT_ARTIFACT_ARTIFACT_VALIDATOR_HPP
