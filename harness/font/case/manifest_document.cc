// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/case/manifest_document.hpp"

#include <filesystem>

#include "harness/font/case/platform_target.hpp"

namespace skity {
namespace font_harness {

namespace {

bool IsSafeRelativePath(const std::string& value) {
  if (value.empty()) {
    return false;
  }
  std::filesystem::path path(value);
  if (path.is_absolute()) {
    return false;
  }
  for (const auto& part : path) {
    if (part == "." || part == "..") {
      return false;
    }
  }
  return true;
}

void ValidateArtifactPath(const Json::Value& artifacts,
                          const std::string& field, ValidationContext* errors) {
  std::string value;
  if (!OptionalStringField(artifacts, field, "$.artifacts", errors, &value)) {
    return;
  }
  ValidateNonEmptyString(value, ChildPath("$.artifacts", field), errors);
}

}  // namespace

ManifestValidationResult ValidateManifestDocument(const Json::Value& root,
                                                  const RepoUriResolver&) {
  ManifestValidationResult result;
  result.normalized_manifest = root;

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

  RequireStringField(root, "id", "$", &result.errors, &result.manifest_id);
  RequireStringField(root, "backend", "$", &result.errors, &result.backend);
  ValidateNonEmptyString(result.manifest_id, "$.id", &result.errors);
  ValidateStringEnum(result.backend, "$.backend", AllowedBackendIds(),
                     &result.errors);

  const Json::Value* platforms = nullptr;
  if (RequireArrayField(root, "platforms", "$", &result.errors, &platforms) &&
      platforms->empty()) {
    result.errors.AddError("$.platforms", "platforms must not be empty");
  }
  if (platforms != nullptr) {
    bool backend_matches_platforms = false;
    for (Json::ArrayIndex i = 0; i < platforms->size(); ++i) {
      const std::string path = IndexPath("$.platforms", i);
      const Json::Value& platform = (*platforms)[i];
      if (!platform.isString()) {
        result.errors.AddError(path, "expected string");
        continue;
      }
      const std::string value = platform.asString();
      ValidateNonEmptyString(value, path, &result.errors);
      if (PlatformTargetMatchesBackend(result.backend, value)) {
        backend_matches_platforms = true;
      }
    }
    if (!result.backend.empty() && !backend_matches_platforms) {
      result.errors.AddError(
          "$.platforms", "platforms do not match backend: " + result.backend);
    }
  }

  if (OptionalStringField(root, "target_platform", "$", &result.errors,
                          &result.target_platform)) {
    ValidateNonEmptyString(result.target_platform, "$.target_platform",
                           &result.errors);
    result.target_platform = CanonicalPlatformTarget(result.target_platform);
    if (platforms != nullptr &&
        !PlatformArrayContainsTarget(*platforms, result.target_platform)) {
      result.errors.AddError("$.target_platform",
                             "target_platform must be listed in platforms");
    }
    if (!result.backend.empty() &&
        !PlatformTargetMatchesBackend(result.backend, result.target_platform)) {
      result.errors.AddError(
          "$.target_platform",
          "target_platform does not match backend: " + result.backend);
    }
  }

  std::string case_root;
  if (RequireStringField(root, "case_root", "$", &result.errors, &case_root) &&
      !IsSafeRelativePath(case_root)) {
    result.errors.AddError("$.case_root",
                           "case_root must be a safe relative path");
  }

  const Json::Value* cases = nullptr;
  if (RequireArrayField(root, "cases", "$", &result.errors, &cases)) {
    if (cases->empty()) {
      result.errors.AddError("$.cases", "cases must not be empty");
    }
    for (Json::ArrayIndex i = 0; i < cases->size(); ++i) {
      const std::string path = IndexPath("$.cases", i);
      if (!(*cases)[i].isString()) {
        result.errors.AddError(path, "expected string");
        continue;
      }
      if (!IsSafeRelativePath((*cases)[i].asString())) {
        result.errors.AddError(path, "case path must be relative to case_root");
      }
    }
  }

  const Json::Value* artifacts = nullptr;
  if (RequireObjectField(root, "artifacts", "$", &result.errors, &artifacts)) {
    ValidateArtifactPath(*artifacts, "root", &result.errors);
    ValidateArtifactPath(*artifacts, "report_root", &result.errors);
    ValidateArtifactPath(*artifacts, "skia_dir", &result.errors);
    ValidateArtifactPath(*artifacts, "skity_dir", &result.errors);
    ValidateArtifactPath(*artifacts, "compare_dir", &result.errors);
  }

  result.valid = result.errors.IsValid();
  return result;
}

}  // namespace font_harness
}  // namespace skity
