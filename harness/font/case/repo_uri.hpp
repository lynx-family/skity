// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_FONT_CASE_REPO_URI_HPP
#define HARNESS_FONT_CASE_REPO_URI_HPP

#include <filesystem>
#include <string>

#include "harness/font/case/validation.hpp"

namespace skity {
namespace font_harness {

struct ResolvedRepoFile {
  std::string uri;
  std::filesystem::path absolute_path;
};

class RepoUriResolver {
 public:
  explicit RepoUriResolver(std::filesystem::path repo_root);

  const std::filesystem::path& RepoRoot() const { return repo_root_; }

  bool ResolveExistingFile(const std::string& uri, const std::string& path,
                           ValidationContext* context,
                           ResolvedRepoFile* resolved) const;

 private:
  std::filesystem::path repo_root_;
};

}  // namespace font_harness
}  // namespace skity

#endif  // HARNESS_FONT_CASE_REPO_URI_HPP
