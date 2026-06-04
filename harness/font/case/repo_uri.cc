// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/case/repo_uri.hpp"

#include <system_error>
#include <utility>

namespace skity {
namespace font_harness {

namespace {

constexpr char kRepoScheme[] = "repo://";

bool StartsWith(const std::string& value, const std::string& prefix) {
  return value.rfind(prefix, 0) == 0;
}

bool HasForbiddenRelativeSegment(const std::filesystem::path& path) {
  for (const auto& part : path) {
    if (part == "." || part == "..") {
      return true;
    }
  }
  return false;
}

bool IsInside(const std::filesystem::path& path,
              const std::filesystem::path& root) {
  std::error_code ec;
  auto relative = std::filesystem::relative(path, root, ec);
  if (ec || relative.empty()) {
    return false;
  }
  for (const auto& part : relative) {
    if (part == "..") {
      return false;
    }
  }
  return true;
}

}  // namespace

RepoUriResolver::RepoUriResolver(std::filesystem::path repo_root) {
  std::error_code ec;
  repo_root_ = std::filesystem::weakly_canonical(std::move(repo_root), ec);
  if (ec) {
    repo_root_ = std::filesystem::absolute(repo_root_);
  }
}

bool RepoUriResolver::ResolveExistingFile(const std::string& uri,
                                          const std::string& path,
                                          ValidationContext* context,
                                          ResolvedRepoFile* resolved) const {
  if (uri.empty()) {
    context->AddError(path, "uri must not be empty");
    return false;
  }
  if (!StartsWith(uri, kRepoScheme)) {
    if (!uri.empty() && std::filesystem::path(uri).is_absolute()) {
      context->AddError(path,
                        "absolute paths are not allowed in repository cases");
    } else {
      context->AddError(path, "uri must use repo:// scheme");
    }
    return false;
  }

  std::string relative_text = uri.substr(std::string(kRepoScheme).size());
  std::filesystem::path relative_path(relative_text);
  if (relative_text.empty() || relative_path.is_absolute() ||
      HasForbiddenRelativeSegment(relative_path)) {
    context->AddError(path,
                      "repo:// uri must be a non-empty in-repository path");
    return false;
  }

  std::filesystem::path absolute_path =
      (repo_root_ / relative_path).lexically_normal();
  if (!IsInside(absolute_path, repo_root_)) {
    context->AddError(path, "repo:// uri escapes repository root");
    return false;
  }

  std::error_code ec;
  if (!std::filesystem::exists(absolute_path, ec) || ec) {
    context->AddError(path, "repo:// target does not exist");
    return false;
  }
  if (!std::filesystem::is_regular_file(absolute_path, ec) || ec) {
    context->AddError(path, "repo:// target is not a regular file");
    return false;
  }

  if (resolved != nullptr) {
    resolved->uri = uri;
    resolved->absolute_path = std::move(absolute_path);
  }
  return true;
}

}  // namespace font_harness
}  // namespace skity
