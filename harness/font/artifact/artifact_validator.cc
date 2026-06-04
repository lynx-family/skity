// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/artifact/artifact_validator.hpp"

namespace skity {
namespace font_harness {

namespace {

ArtifactValidationResult ValidateBaseArtifact(
    const Json::Value& root, const std::string& expected_type) {
  ArtifactValidationResult result;
  if (!RequireObject(root, "$", &result.errors)) {
    result.valid = false;
    return result;
  }

  int schema_version = 0;
  if (RequireIntField(root, "schema_version", "$", &result.errors,
                      &schema_version) &&
      schema_version != 1) {
    result.errors.AddError("$.schema_version", "expected schema_version 1");
  }

  RequireStringField(root, "artifact_type", "$", &result.errors,
                     &result.artifact_type);
  RequireStringField(root, "case_id", "$", &result.errors, &result.case_id);
  ValidateNonEmptyString(result.case_id, "$.case_id", &result.errors);
  if (result.artifact_type != expected_type) {
    result.errors.AddError("$.artifact_type",
                           "expected artifact_type " + expected_type);
  }
  return result;
}

}  // namespace

ArtifactValidationResult ValidateProbeResultDocument(const Json::Value& root) {
  ArtifactValidationResult result =
      ValidateBaseArtifact(root, "font_probe_result");
  RequireStringField(root, "backend", "$", &result.errors, nullptr);
  result.valid = result.errors.IsValid();
  return result;
}

ArtifactValidationResult ValidateCompareReportDocument(
    const Json::Value& root) {
  ArtifactValidationResult result =
      ValidateBaseArtifact(root, "font_compare_report");
  RequireStringField(root, "stage", "$", &result.errors, nullptr);
  RequireStringField(root, "reason_code", "$", &result.errors, nullptr);
  result.valid = result.errors.IsValid();
  return result;
}

}  // namespace font_harness
}  // namespace skity
