// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/platform/ios/runner/ios_runner.hpp"

#import <Foundation/Foundation.h>

#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "harness/font/artifact/json_io.hpp"
#include "harness/font/platform/coretext/env_info.hpp"
#include "harness/font/probe/font_manager_probe.hpp"
#include "harness/font/probe/glyph_path_probe.hpp"
#include "harness/font/probe/metrics_probe.hpp"
#include "harness/font/probe/typeface_probe.hpp"

#ifndef SKITY_FONT_HARNESS_TARGET_PLATFORM
#define SKITY_FONT_HARNESS_TARGET_PLATFORM "ios-sim-coretext"
#endif

namespace skity {
namespace font_harness {
namespace ios {
namespace {

RunResult Failure(std::string message) {
  RunResult result;
  result.ok = false;
  result.message = std::move(message);
  return result;
}

CaseRunResult CaseFailure(std::string case_id, std::string message) {
  CaseRunResult result;
  result.ok = false;
  result.case_id = std::move(case_id);
  result.message = std::move(message);
  return result;
}

bool ReadStringField(const Json::Value& root, const char* field, std::string* value) {
  if (!root.isObject() || !root.isMember(field) || !root[field].isString()) {
    return false;
  }
  *value = root[field].asString();
  return true;
}

bool ReadStringArrayField(const Json::Value& root, const char* field,
                          std::vector<std::string>* values, std::string* error) {
  if (!root.isObject() || !root.isMember(field)) {
    return true;
  }
  const Json::Value& array = root[field];
  if (!array.isArray()) {
    if (error != nullptr) {
      *error = std::string(field) + " must be an array";
    }
    return false;
  }
  values->clear();
  for (Json::ArrayIndex i = 0; i < array.size(); ++i) {
    const Json::Value& item = array[i];
    if (item.isString()) {
      values->push_back(item.asString());
      continue;
    }
    if (item.isObject() && item.isMember("path") && item["path"].isString()) {
      values->push_back(item["path"].asString());
      continue;
    }
    if (error != nullptr) {
      *error = std::string(field) + " entries must be strings or objects with string path";
    }
    return false;
  }
  return true;
}

std::string StemOrFallback(const std::filesystem::path& path, const std::string& fallback) {
  const std::string stem = path.stem().string();
  return stem.empty() ? fallback : stem;
}

bool IsSafeFileChar(char value) {
  return std::isalnum(static_cast<unsigned char>(value)) || value == '-' || value == '_' ||
         value == '.';
}

std::string SanitizeFileComponent(std::string value, const std::string& fallback) {
  if (value.empty()) {
    return fallback;
  }
  for (char& c : value) {
    if (!IsSafeFileChar(c)) {
      c = '_';
    }
  }
  while (!value.empty() && value.front() == '.') {
    value.erase(value.begin());
  }
  return value.empty() ? fallback : value;
}

std::filesystem::path TemporaryArtifactRoot() {
  @autoreleasepool {
    NSString* temporary_dir = NSTemporaryDirectory();
    if (temporary_dir.length > 0) {
      return std::filesystem::path(temporary_dir.fileSystemRepresentation) / "font-harness" /
             "artifacts";
    }
  }

  const char* tmpdir = std::getenv("TMPDIR");
  if (tmpdir != nullptr && tmpdir[0] != '\0') {
    return std::filesystem::path(tmpdir) / "font-harness" / "artifacts";
  }
  return std::filesystem::temp_directory_path() / "font-harness" / "artifacts";
}

std::filesystem::path AppSupportArtifactRoot() {
  @autoreleasepool {
    NSArray<NSURL*>* urls =
        [[NSFileManager defaultManager] URLsForDirectory:NSApplicationSupportDirectory
                                               inDomains:NSUserDomainMask];
    NSURL* support_url = [urls firstObject];
    NSString* support_path = support_url.path;
    if (support_path.length > 0) {
      return std::filesystem::path(support_path.fileSystemRepresentation) / "font-harness" /
             "artifacts";
    }
  }
  return TemporaryArtifactRoot();
}

Json::Value BuildUnsupportedCategoryReport(const std::string& case_id, const std::string& backend,
                                           const std::string& category) {
  Json::Value report(Json::objectValue);
  report["schema_version"] = 1;
  report["artifact_type"] = "font_probe_result";
  report["case_id"] = case_id;
  report["backend"] = backend;
  report["ok"] = false;
  report["reason_code"] = "unsupported_case_category";
  report["message"] = std::string("iOS smoke runner does not support category ") + category;
  return report;
}

Json::Value RunProbeForCase(const std::filesystem::path& repo_root,
                            const std::filesystem::path& case_path, const std::string& category,
                            bool* probe_ok) {
  *probe_ok = false;
  if (category == "typeface_probe") {
    TypefaceProbeRequest request;
    request.repo_root = repo_root;
    request.case_path = case_path;
    request.backend = "coretext";
    TypefaceProbeResult result = RunTypefaceProbe(request);
    *probe_ok = result.status == TypefaceProbeStatus::kSuccess;
    return std::move(result.report);
  }

  if (category == "font_metrics" || category == "glyph_metrics" || category == "scaler_context") {
    MetricsProbeRequest request;
    request.repo_root = repo_root;
    request.case_path = case_path;
    request.backend = "coretext";
    MetricsProbeResult result = RunMetricsProbe(request);
    *probe_ok = result.status == MetricsProbeStatus::kSuccess;
    return std::move(result.report);
  }

  if (category == "glyph_path") {
    GlyphPathProbeRequest request;
    request.repo_root = repo_root;
    request.case_path = case_path;
    request.backend = "coretext";
    GlyphPathProbeResult result = RunGlyphPathProbe(request);
    *probe_ok = result.status == GlyphPathProbeStatus::kSuccess;
    return std::move(result.report);
  }

  if (category == "font_manager" || category == "family_style_set") {
    FontManagerProbeRequest request;
    request.repo_root = repo_root;
    request.case_path = case_path;
    request.backend = "coretext";
    FontManagerProbeResult result = RunFontManagerProbe(request);
    *probe_ok = result.status == FontManagerProbeStatus::kSuccess;
    return std::move(result.report);
  }

  return BuildUnsupportedCategoryReport(StemOrFallback(case_path, "case"), "coretext", category);
}

void FillIOSActualProvenance(const RunConfig& config, const std::filesystem::path& artifact_path,
                             Json::Value* report) {
  (*report)["target_platform"] = config.target_platform;
  (*report)["platform_id"] = config.target_platform;
  (*report)["artifact_role"] = "actual";
  (*report)["producer"] = "skity";
  (*report)["runner"] = "ios-xctest";
  (*report)["runner_mode"] = config.runner_mode;
  (*report)["artifact_path"] = artifact_path.generic_string();
}

CaseRunResult WriteCaseArtifact(const RunConfig& config, const std::filesystem::path& repo_root,
                                const std::string& case_relative_path) {
  const std::filesystem::path case_path = repo_root / config.case_root / case_relative_path;
  Json::Value case_root;
  std::string error;
  std::string case_id = StemOrFallback(case_path, "case");
  if (!LoadJsonFile(case_path, &case_root, &error)) {
    return CaseFailure(case_id, error);
  }

  ReadStringField(case_root, "id", &case_id);
  std::string category;
  if (!ReadStringField(case_root, "category", &category)) {
    return CaseFailure(case_id, "case category is required");
  }

  const std::string safe_case_id = SanitizeFileComponent(case_id, "case");
  const std::filesystem::path artifact_path = config.artifact_output_dir / config.target_platform /
                                              "skity" / (safe_case_id + ".skity.json");

  bool probe_ok = false;
  Json::Value report = RunProbeForCase(repo_root, case_path, category, &probe_ok);
  FillIOSActualProvenance(config, artifact_path, &report);

  if (!WriteJsonFile(artifact_path, report, &error)) {
    return CaseFailure(case_id, error);
  }

  CaseRunResult result;
  result.ok = probe_ok;
  result.case_id = case_id;
  result.artifact_path = artifact_path;
  if (!probe_ok) {
    if (report.isMember("reason_code") && report["reason_code"].isString()) {
      result.message = report["reason_code"].asString();
    } else {
      result.message = "probe failed";
    }
  }
  return result;
}

RunResult WriteEnv(const RunConfig& config) {
  if (config.target_platform.empty()) {
    return Failure("target_platform is required");
  }
  if (config.artifact_output_dir.empty()) {
    return Failure("artifact_output_dir is required");
  }
  if (config.runner_mode != "actual" && config.runner_mode != "expected") {
    return Failure("runner_mode must be actual or expected");
  }

  CoreTextEnvRequest request;
  Json::Value report = BuildCoreTextEnvInfo(request);
  report["artifact_role"] = "env";
  report["target_platform"] = config.target_platform;
  report["runner"] = "ios-xctest";
  report["runner_mode"] = config.runner_mode;
  report["producer"] = config.runner_mode == "expected" ? "skia" : "skity";

  const std::filesystem::path artifact_path =
      config.artifact_output_dir / config.target_platform / "env" / "latest.json";
  report["artifact_path"] = artifact_path.generic_string();

  std::string error;
  if (!WriteJsonFile(artifact_path, report, &error)) {
    return Failure(error);
  }

  RunResult result;
  result.ok = true;
  result.artifact_path = artifact_path;
  return result;
}

}  // namespace

RunConfig DefaultRunConfig() {
  RunConfig config;
  config.target_platform = SKITY_FONT_HARNESS_TARGET_PLATFORM;
  config.artifact_output_dir = AppSupportArtifactRoot();
  config.runner_mode = "actual";
  config.case_root = "harness/font/cases";
  return config;
}

bool LoadRunConfig(const std::filesystem::path& config_path, RunConfig* config,
                   std::string* error) {
  if (config == nullptr) {
    if (error != nullptr) {
      *error = "config must not be null";
    }
    return false;
  }

  Json::Value root;
  if (!LoadJsonFile(config_path, &root, error)) {
    return false;
  }
  if (!root.isObject()) {
    if (error != nullptr) {
      *error = "run config must be a JSON object";
    }
    return false;
  }

  std::string value;
  if (ReadStringField(root, "target_platform", &value)) {
    config->target_platform = value;
  }
  if (ReadStringField(root, "artifact_output_dir", &value)) {
    config->artifact_output_dir = value;
  }
  if (ReadStringField(root, "runner_mode", &value)) {
    config->runner_mode = value;
  }
  if (ReadStringField(root, "case_root", &value)) {
    config->case_root = value;
  }
  if (!ReadStringArrayField(root, "cases", &config->cases, error)) {
    return false;
  }
  return true;
}

RunResult WriteEnvironmentArtifact(const RunConfig& config) { return WriteEnv(config); }

SmokeRunResult WriteArtifactsForConfiguredCases(const RunConfig& config,
                                                const std::filesystem::path& repo_root,
                                                const std::string& run_name) {
  SmokeRunResult result;
  if (config.target_platform.empty()) {
    result.message = "target_platform is required";
    return result;
  }
  if (config.artifact_output_dir.empty()) {
    result.message = "artifact_output_dir is required";
    return result;
  }
  if (config.runner_mode != "actual") {
    result.message = "Skity iOS runner requires runner_mode=actual";
    return result;
  }
  if (config.case_root.empty()) {
    result.message = "case_root is required";
    return result;
  }
  if (config.cases.empty()) {
    result.message = "runner-config cases are required";
    return result;
  }
  if (repo_root.empty()) {
    result.message = "repo_root is required";
    return result;
  }

  result.ok = true;
  for (const std::string& case_relative_path : config.cases) {
    CaseRunResult case_result = WriteCaseArtifact(config, repo_root, case_relative_path);
    if (!case_result.ok) {
      result.ok = false;
    }
    result.cases.push_back(std::move(case_result));
  }

  if (!result.ok) {
    std::ostringstream message;
    message << run_name << " failed";
    for (const CaseRunResult& case_result : result.cases) {
      if (!case_result.ok) {
        message << "; " << case_result.case_id << ": " << case_result.message;
      }
    }
    result.message = message.str();
  }
  return result;
}

SmokeRunResult WriteConfiguredArtifacts(const RunConfig& config,
                                        const std::filesystem::path& repo_root) {
  return WriteArtifactsForConfiguredCases(config, repo_root, "configured run");
}

RunResult WriteEnvironmentArtifactFromConfigFile(const std::filesystem::path& config_path) {
  std::string error;
  RunConfig config = DefaultRunConfig();
  if (!LoadRunConfig(config_path, &config, &error)) {
    return Failure(error);
  }
  return WriteEnv(config);
}

}  // namespace ios
}  // namespace font_harness
}  // namespace skity
