// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_FONT_ARTIFACT_ARTIFACT_WRITER_HPP
#define HARNESS_FONT_ARTIFACT_ARTIFACT_WRITER_HPP

#include <filesystem>
#include <string>
#include <vector>

#include "third_party/jsoncpp/include/json/json.h"

namespace skity {
namespace font_harness {

enum class ArtifactKind {
  kCaseInfo,
  kEnvInfo,
  kFontList,
  kSkityResult,
  kCompareReport,
  kRunSummary,
  kPathDump,
  kMatchResult,
};

struct ArtifactDescriptor {
  ArtifactKind kind = ArtifactKind::kCaseInfo;
  std::string command;
  std::string output_flag = "--report";
  std::string case_id;
  std::string manifest_id;
  std::string backend;
  std::filesystem::path input_path;
  std::filesystem::path explicit_output_path;
  std::vector<std::string> repro_args;
};

struct ArtifactWriteResult {
  std::filesystem::path absolute_path;
  std::string stable_path;
  std::string minimal_repro_command;
};

std::filesystem::path ResolveArtifactPath(
    const std::filesystem::path& repo_root,
    const ArtifactDescriptor& descriptor);

bool WriteStableArtifact(const std::filesystem::path& repo_root,
                         const ArtifactDescriptor& descriptor,
                         Json::Value* report, ArtifactWriteResult* result,
                         std::string* error);

std::string BuildHumanSummary(const std::string& command,
                              const Json::Value& report, int exit_code);

Json::Value BuildProbeResultReport(const std::string& case_id,
                                   const std::string& backend,
                                   const std::string& reason_code);
Json::Value BuildCompareReport(const std::string& case_id,
                               const std::string& backend,
                               const std::string& stage,
                               const std::string& reason_code);
Json::Value BuildRunSummaryReport(const std::string& manifest_id,
                                  const std::string& backend,
                                  const std::string& reason_code);
Json::Value BuildPathDumpReport(const std::string& case_id,
                                const std::string& backend,
                                const std::string& reason_code);

}  // namespace font_harness
}  // namespace skity

#endif  // HARNESS_FONT_ARTIFACT_ARTIFACT_WRITER_HPP
