// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/case/manifest_document.hpp"

#include <filesystem>

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
  ValidateStringEnum(result.backend, "$.backend",
                     {"coretext", "directwrite", "host-ft"}, &result.errors);

  const Json::Value* platforms = nullptr;
  if (RequireArrayField(root, "platforms", "$", &result.errors, &platforms) &&
      platforms->empty()) {
    result.errors.AddError("$.platforms", "platforms must not be empty");
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
  RequireObjectField(root, "artifacts", "$", &result.errors, &artifacts);

  result.valid = result.errors.IsValid();
  return result;
}

}  // namespace font_harness
}  // namespace skity
