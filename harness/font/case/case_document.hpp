// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_FONT_CASE_CASE_DOCUMENT_HPP
#define HARNESS_FONT_CASE_CASE_DOCUMENT_HPP

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "harness/font/case/repo_uri.hpp"
#include "harness/font/case/validation.hpp"

namespace skity {
namespace font_harness {

struct ResolvedFontFile {
  std::string id;
  std::string uri;
  std::filesystem::path absolute_path;
  int collection_index = 0;
};

struct CaseValidationResult {
  bool valid = false;
  std::string case_id;
  std::string backend;
  Json::Value normalized_case;
  std::vector<ResolvedFontFile> resolved_font_files;
  ValidationContext errors;
};

CaseValidationResult ValidateCaseDocument(const Json::Value& root,
                                          const RepoUriResolver& resolver);
Json::Value BuildCaseInfoReport(const CaseValidationResult& result);

}  // namespace font_harness
}  // namespace skity

#endif  // HARNESS_FONT_CASE_CASE_DOCUMENT_HPP
