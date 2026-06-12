// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_FONT_CASE_MANIFEST_DOCUMENT_HPP
#define HARNESS_FONT_CASE_MANIFEST_DOCUMENT_HPP

#include "harness/font/case/repo_uri.hpp"
#include "harness/font/case/validation.hpp"

namespace skity {
namespace font_harness {

struct ManifestValidationResult {
  bool valid = false;
  std::string manifest_id;
  std::string backend;
  std::string target_platform;
  Json::Value normalized_manifest;
  ValidationContext errors;
};

ManifestValidationResult ValidateManifestDocument(
    const Json::Value& root, const RepoUriResolver& resolver);

}  // namespace font_harness
}  // namespace skity

#endif  // HARNESS_FONT_CASE_MANIFEST_DOCUMENT_HPP
