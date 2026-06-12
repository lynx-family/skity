// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_FONT_PLATFORM_IOS_RUNNER_IOS_RUNNER_HPP
#define HARNESS_FONT_PLATFORM_IOS_RUNNER_IOS_RUNNER_HPP

#include <filesystem>
#include <string>
#include <vector>

namespace skity {
namespace font_harness {
namespace ios {

struct RunConfig {
  std::string target_platform;
  std::filesystem::path artifact_output_dir;
  std::string runner_mode;
  std::filesystem::path case_root;
  std::vector<std::string> cases;
};

struct RunResult {
  bool ok = false;
  std::string message;
  std::filesystem::path artifact_path;
};

struct CaseRunResult {
  bool ok = false;
  std::string case_id;
  std::string message;
  std::filesystem::path artifact_path;
};

struct SmokeRunResult {
  bool ok = false;
  std::string message;
  std::vector<CaseRunResult> cases;
};

RunConfig DefaultRunConfig();

bool LoadRunConfig(const std::filesystem::path& config_path, RunConfig* config,
                   std::string* error);

RunResult WriteEnvironmentArtifact(const RunConfig& config);

RunResult WriteEnvironmentArtifactFromConfigFile(
    const std::filesystem::path& config_path);

SmokeRunResult WriteConfiguredArtifacts(const RunConfig& config,
                                        const std::filesystem::path& repo_root);

}  // namespace ios
}  // namespace font_harness
}  // namespace skity

#endif  // HARNESS_FONT_PLATFORM_IOS_RUNNER_IOS_RUNNER_HPP
