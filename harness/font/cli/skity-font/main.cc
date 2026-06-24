// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cctype>
#include <filesystem>
#include <iostream>
#include <skity/utils/settings.hpp>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "harness/font/artifact/artifact_writer.hpp"
#include "harness/font/artifact/json_io.hpp"
#include "harness/font/case/case_document.hpp"
#include "harness/font/case/manifest_document.hpp"
#include "harness/font/case/platform_target.hpp"
#include "harness/font/compare/compare_engine.hpp"
#include "harness/font/platform/coretext/env_info.hpp"
#include "harness/font/platform/directwrite/env_info.hpp"
#include "harness/font/probe/font_manager_probe.hpp"
#include "harness/font/probe/glyph_image_probe.hpp"
#include "harness/font/probe/glyph_path_probe.hpp"
#include "harness/font/probe/metrics_probe.hpp"
#include "harness/font/probe/typeface_probe.hpp"

#ifndef SKITY_FONT_HARNESS_REPO_ROOT
#define SKITY_FONT_HARNESS_REPO_ROOT "."
#endif

namespace {

constexpr int kExitSuccess = 0;
constexpr int kExitCompareMismatch = 1;
constexpr int kExitUsageError = 2;
constexpr int kExitSchemaValidationFailed = 3;
constexpr int kExitIOFailure = 4;
constexpr int kExitBackendUnavailable = 5;
constexpr int kExitCompareInputFailure = 6;
constexpr int kExitSkityProbeFailure = 7;

void ConfigureFontManagerBackend(const std::string& backend) {
#if defined(SKITY_WIN)
  skity::Settings::GetSettings().SetEnableDWriteFontManager(backend ==
                                                            "directwrite");
#else
  (void)backend;
#endif
}

struct CaseMetadata {
  std::string case_id;
  std::string backend;
};

struct ManifestMetadata {
  std::string manifest_id;
  std::string backend;
};

struct ProbeInvocationResult {
  Json::Value report;
  int exit_code = kExitSkityProbeFailure;
  std::string case_id;
  std::string backend;
};

struct CompareInvocationResult {
  Json::Value report;
  int exit_code = kExitCompareInputFailure;
  std::string case_id;
  std::string backend;
};

struct ManifestRunPaths {
  std::filesystem::path case_root;
  std::filesystem::path repo_root;
  std::filesystem::path resolved_skia_dir;
  std::filesystem::path resolved_skity_dir;
  std::filesystem::path resolved_compare_dir;
  std::string backend;
  std::string target_platform;
};

struct ManifestRunCounts {
  int pass_count = 0;
  int mismatch_count = 0;
  int input_failure_count = 0;
  int probe_failure_count = 0;
  int schema_failure_count = 0;
  int backend_unavailable_count = 0;
  int io_failure_count = 0;
};

void PrintUsage(std::ostream& out) {
  out << "skity-font - Skity font harness CLI\n"
      << "\n"
      << "Usage:\n"
      << "  skity-font --help\n"
      << "  skity-font <command> [options]\n"
      << "\n"
      << "Commands planned for the font harness:\n"
      << "  env-info      collect platform and backend information\n"
      << "  case-info     validate and normalize a case file\n"
      << "  match         run a Skity system font match probe\n"
      << "  probe         run a Skity font probe and write result JSON\n"
      << "  compare       compare Skia oracle and Skity result JSON\n"
      << "  run           run a manifest with existing Skia oracle data\n"
      << "  dump-path     dump one normalized glyph path\n"
      << "  list-fonts    list visible families and styles for a backend\n"
      << "\n"
      << "case-info options:\n"
      << "  --case <path>       input case JSON\n"
      << "  --report <path>     optional output report JSON\n"
      << "  --repo-root <path>  optional repository root override\n"
      << "\n"
      << "match options:\n"
      << "  --case <path>       input font_manager/family_style_set case JSON\n"
      << "  --backend <name>    backend to use\n"
      << "  --out <path>        optional output JSON\n"
      << "  --repo-root <path>  optional repository root override\n"
      << "\n"
      << "env-info/list-fonts options:\n"
      << "  --backend <name>    backend to inspect (coretext, directwrite)\n"
      << "  --report <path>     optional output JSON\n"
      << "  --repo-root <path>  optional repository root override\n"
      << "\n"
      << "probe/dump-path options:\n"
      << "  --case <path>       input case JSON\n"
      << "  --backend <name>    backend to use\n"
      << "  --out <path>        optional output JSON\n"
      << "  --repo-root <path>  optional repository root override\n"
      << "\n"
      << "compare options:\n"
      << "  --case <path>       input case JSON\n"
      << "  --expected <path>   Skia oracle JSON\n"
      << "  --actual <path>     Skity result JSON\n"
      << "  --report <path>     optional output report JSON\n"
      << "  --backend <name>    optional backend override\n"
      << "  --repo-root <path>  optional repository root override\n"
      << "\n"
      << "run options:\n"
      << "  --manifest <path>   input manifest JSON\n"
      << "  --backend <name>    backend to use\n"
      << "  --skia-dir <path>   existing Skia oracle directory\n"
      << "  --skity-dir <path>  Skity result directory\n"
      << "  --report <path>     optional output report JSON\n"
      << "  --repo-root <path>  optional repository root override\n";
}

bool IsHelpArgument(const std::string& arg) {
  return arg == "--help" || arg == "-h" || arg == "help";
}

std::string PathArg(const std::filesystem::path& path) {
  return path.generic_string();
}

std::string StemOrFallback(const std::filesystem::path& path,
                           const std::string& fallback) {
  std::string stem = path.stem().string();
  return stem.empty() ? fallback : stem;
}

std::string StablePathForRun(const std::filesystem::path& path,
                             const std::filesystem::path& repo_root) {
  std::error_code ec;
  std::filesystem::path relative =
      std::filesystem::relative(path, repo_root, ec);
  if (!ec && !relative.empty()) {
    bool escapes_repo = false;
    for (const auto& part : relative) {
      if (part == "..") {
        escapes_repo = true;
        break;
      }
    }
    if (!escapes_repo) {
      return relative.generic_string();
    }
  }
  return path.lexically_normal().generic_string();
}

std::filesystem::path ResolveRunPath(const std::filesystem::path& repo_root,
                                     const std::filesystem::path& path) {
  if (path.is_absolute()) {
    return path.lexically_normal();
  }
  return (repo_root / path).lexically_normal();
}

bool IsSafeFileChar(char value) {
  return std::isalnum(static_cast<unsigned char>(value)) || value == '-' ||
         value == '_' || value == '.';
}

std::string SanitizeFileComponent(std::string value,
                                  const std::string& fallback) {
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

bool ReadStringField(const Json::Value& root, const std::string& field,
                     std::string* value) {
  if (!root.isObject() || !root.isMember(field) || !root[field].isString()) {
    return false;
  }
  *value = root[field].asString();
  return true;
}

CaseMetadata ReadCaseMetadata(const std::filesystem::path& case_path) {
  CaseMetadata metadata;
  metadata.case_id = StemOrFallback(case_path, "case");

  Json::Value root;
  std::string error;
  if (!skity::font_harness::LoadJsonFile(case_path, &root, &error)) {
    return metadata;
  }
  ReadStringField(root, "id", &metadata.case_id);
  ReadStringField(root, "backend", &metadata.backend);
  return metadata;
}

ManifestMetadata ReadManifestMetadata(
    const std::filesystem::path& manifest_path) {
  ManifestMetadata metadata;
  metadata.manifest_id = StemOrFallback(manifest_path, "manifest");

  Json::Value root;
  std::string error;
  if (!skity::font_harness::LoadJsonFile(manifest_path, &root, &error)) {
    return metadata;
  }
  ReadStringField(root, "id", &metadata.manifest_id);
  ReadStringField(root, "backend", &metadata.backend);
  return metadata;
}

Json::Value BuildCaseLoadErrorReport(const std::string& message) {
  skity::font_harness::ValidationContext errors;
  errors.AddError("--case", message);

  Json::Value report(Json::objectValue);
  report["schema_version"] = 1;
  report["artifact_type"] = "font_case_info";
  report["valid"] = false;
  report["case_id"] = "";
  report["backend"] = "";
  report["reason_code"] = "schema_validation_failed";
  report["errors"] = errors.ToJson();
  return report;
}

int WriteCommandReport(
    Json::Value report, const std::filesystem::path& repo_root,
    const skity::font_harness::ArtifactDescriptor& descriptor, int exit_code) {
  skity::font_harness::ArtifactWriteResult write_result;
  std::string error;
  if (!skity::font_harness::WriteStableArtifact(repo_root, descriptor, &report,
                                                &write_result, &error)) {
    std::cerr << error << "\n";
    return kExitIOFailure;
  }

  std::cout << skity::font_harness::BuildHumanSummary(descriptor.command,
                                                      report, exit_code)
            << "\n";
  return exit_code;
}

std::vector<std::string> BackendReproArgs(const std::string& backend) {
  return {"--backend", backend};
}

bool IsMetricsProbeCategory(const std::string& category) {
  return category == "font_metrics" || category == "glyph_metrics" ||
         category == "scaler_context";
}

bool IsGlyphPathProbeCategory(const std::string& category) {
  return category == "glyph_path";
}

bool IsGlyphImageProbeCategory(const std::string& category) {
  return category == "glyph_image";
}

bool IsFontManagerProbeCategory(const std::string& category) {
  return category == "font_manager" || category == "family_style_set";
}

std::string ReadCaseCategory(const std::filesystem::path& case_path) {
  Json::Value root;
  std::string error;
  if (!skity::font_harness::LoadJsonFile(case_path, &root, &error)) {
    return "";
  }

  std::string category;
  ReadStringField(root, "category", &category);
  return category;
}

Json::Value BuildMatchSchemaErrorReport(const std::string& case_id,
                                        const std::string& backend,
                                        const std::string& path,
                                        const std::string& message) {
  Json::Value report = skity::font_harness::BuildProbeResultReport(
      case_id, backend, "schema_validation_failed");
  Json::Value error(Json::objectValue);
  error["path"] = path;
  error["message"] = message;
  report["validation_errors"] = Json::Value(Json::arrayValue);
  report["validation_errors"].append(std::move(error));
  return report;
}

int ExitCodeForProbeStatus(skity::font_harness::TypefaceProbeStatus status) {
  switch (status) {
    case skity::font_harness::TypefaceProbeStatus::kSuccess:
      return kExitSuccess;
    case skity::font_harness::TypefaceProbeStatus::kSchemaValidationFailed:
      return kExitSchemaValidationFailed;
    case skity::font_harness::TypefaceProbeStatus::kBackendUnavailable:
      return kExitBackendUnavailable;
    case skity::font_harness::TypefaceProbeStatus::kProbeFailed:
      return kExitSkityProbeFailure;
  }
  return kExitSkityProbeFailure;
}

int ExitCodeForProbeStatus(skity::font_harness::MetricsProbeStatus status) {
  switch (status) {
    case skity::font_harness::MetricsProbeStatus::kSuccess:
      return kExitSuccess;
    case skity::font_harness::MetricsProbeStatus::kSchemaValidationFailed:
      return kExitSchemaValidationFailed;
    case skity::font_harness::MetricsProbeStatus::kBackendUnavailable:
      return kExitBackendUnavailable;
    case skity::font_harness::MetricsProbeStatus::kProbeFailed:
      return kExitSkityProbeFailure;
  }
  return kExitSkityProbeFailure;
}

int ExitCodeForProbeStatus(skity::font_harness::GlyphPathProbeStatus status) {
  switch (status) {
    case skity::font_harness::GlyphPathProbeStatus::kSuccess:
      return kExitSuccess;
    case skity::font_harness::GlyphPathProbeStatus::kSchemaValidationFailed:
      return kExitSchemaValidationFailed;
    case skity::font_harness::GlyphPathProbeStatus::kBackendUnavailable:
      return kExitBackendUnavailable;
    case skity::font_harness::GlyphPathProbeStatus::kProbeFailed:
      return kExitSkityProbeFailure;
  }
  return kExitSkityProbeFailure;
}

int ExitCodeForProbeStatus(skity::font_harness::GlyphImageProbeStatus status) {
  switch (status) {
    case skity::font_harness::GlyphImageProbeStatus::kSuccess:
      return kExitSuccess;
    case skity::font_harness::GlyphImageProbeStatus::kSchemaValidationFailed:
      return kExitSchemaValidationFailed;
    case skity::font_harness::GlyphImageProbeStatus::kBackendUnavailable:
      return kExitBackendUnavailable;
    case skity::font_harness::GlyphImageProbeStatus::kProbeFailed:
      return kExitSkityProbeFailure;
  }
  return kExitSkityProbeFailure;
}

int ExitCodeForProbeStatus(skity::font_harness::FontManagerProbeStatus status) {
  switch (status) {
    case skity::font_harness::FontManagerProbeStatus::kSuccess:
      return kExitSuccess;
    case skity::font_harness::FontManagerProbeStatus::kSchemaValidationFailed:
      return kExitSchemaValidationFailed;
    case skity::font_harness::FontManagerProbeStatus::kBackendUnavailable:
      return kExitBackendUnavailable;
    case skity::font_harness::FontManagerProbeStatus::kProbeFailed:
      return kExitSkityProbeFailure;
  }
  return kExitSkityProbeFailure;
}

int ExitCodeForCompareStatus(skity::font_harness::CompareStatus status) {
  switch (status) {
    case skity::font_harness::CompareStatus::kPass:
      return kExitSuccess;
    case skity::font_harness::CompareStatus::kMismatch:
      return kExitCompareMismatch;
    case skity::font_harness::CompareStatus::kInputFailed:
      return kExitCompareInputFailure;
  }
  return kExitCompareInputFailure;
}

ProbeInvocationResult RunSkityProbeForCase(
    const std::filesystem::path& repo_root,
    const std::filesystem::path& case_path, const std::string& backend) {
  ConfigureFontManagerBackend(backend);

  ProbeInvocationResult invocation;
  const std::string category = ReadCaseCategory(case_path);
  if (IsMetricsProbeCategory(category)) {
    skity::font_harness::MetricsProbeRequest request;
    request.repo_root = repo_root;
    request.case_path = case_path;
    request.backend = backend;
    skity::font_harness::MetricsProbeResult result =
        skity::font_harness::RunMetricsProbe(request);
    invocation.report = std::move(result.report);
    invocation.exit_code = ExitCodeForProbeStatus(result.status);
    invocation.case_id = std::move(result.case_id);
    invocation.backend = std::move(result.backend);
  } else if (IsGlyphPathProbeCategory(category)) {
    skity::font_harness::GlyphPathProbeRequest request;
    request.repo_root = repo_root;
    request.case_path = case_path;
    request.backend = backend;
    skity::font_harness::GlyphPathProbeResult result =
        skity::font_harness::RunGlyphPathProbe(request);
    invocation.report = std::move(result.report);
    invocation.exit_code = ExitCodeForProbeStatus(result.status);
    invocation.case_id = std::move(result.case_id);
    invocation.backend = std::move(result.backend);
  } else if (IsGlyphImageProbeCategory(category)) {
    skity::font_harness::GlyphImageProbeRequest request;
    request.repo_root = repo_root;
    request.case_path = case_path;
    request.backend = backend;
    skity::font_harness::GlyphImageProbeResult result =
        skity::font_harness::RunGlyphImageProbe(request);
    invocation.report = std::move(result.report);
    invocation.exit_code = ExitCodeForProbeStatus(result.status);
    invocation.case_id = std::move(result.case_id);
    invocation.backend = std::move(result.backend);
  } else if (IsFontManagerProbeCategory(category)) {
    skity::font_harness::FontManagerProbeRequest request;
    request.repo_root = repo_root;
    request.case_path = case_path;
    request.backend = backend;
    skity::font_harness::FontManagerProbeResult result =
        skity::font_harness::RunFontManagerProbe(request);
    invocation.report = std::move(result.report);
    invocation.exit_code = ExitCodeForProbeStatus(result.status);
    invocation.case_id = std::move(result.case_id);
    invocation.backend = std::move(result.backend);
  } else {
    skity::font_harness::TypefaceProbeRequest request;
    request.repo_root = repo_root;
    request.case_path = case_path;
    request.backend = backend;
    skity::font_harness::TypefaceProbeResult result =
        skity::font_harness::RunTypefaceProbe(request);
    invocation.report = std::move(result.report);
    invocation.exit_code = ExitCodeForProbeStatus(result.status);
    invocation.case_id = std::move(result.case_id);
    invocation.backend = std::move(result.backend);
  }
  return invocation;
}

CompareInvocationResult RunCompareForArtifacts(
    const std::filesystem::path& repo_root,
    const std::filesystem::path& case_path,
    const std::filesystem::path& expected_path,
    const std::filesystem::path& actual_path, const std::string& backend) {
  CompareInvocationResult invocation;
  skity::font_harness::CompareRequest request;
  request.repo_root = repo_root;
  request.case_path = case_path;
  request.expected_path = expected_path;
  request.actual_path = actual_path;
  request.backend = backend;
  skity::font_harness::CompareResult result =
      skity::font_harness::RunCompare(request);
  invocation.report = std::move(result.report);
  invocation.exit_code = ExitCodeForCompareStatus(result.status);
  invocation.case_id = std::move(result.case_id);
  invocation.backend = std::move(result.backend);
  return invocation;
}

std::filesystem::path ReadManifestArtifactDir(const Json::Value& manifest,
                                              const std::string& field,
                                              const std::string& fallback) {
  if (manifest.isObject() && manifest.isMember("artifacts") &&
      manifest["artifacts"].isObject()) {
    std::string value;
    if (ReadStringField(manifest["artifacts"], field, &value) &&
        !value.empty()) {
      return value;
    }
  }
  return fallback;
}

std::filesystem::path ReadManifestArtifactRoot(const Json::Value& manifest) {
  return ReadManifestArtifactDir(manifest, "root", "");
}

std::filesystem::path ReadManifestReportRoot(const Json::Value& manifest) {
  return ReadManifestArtifactDir(manifest, "report_root", "");
}

std::filesystem::path ReadManifestArtifactRoleDir(
    const Json::Value& manifest, const std::filesystem::path& artifact_root,
    const std::string& role, const std::string& legacy_field,
    const std::string& legacy_fallback) {
  if (!artifact_root.empty()) {
    return artifact_root / role;
  }
  return ReadManifestArtifactDir(manifest, legacy_field, legacy_fallback);
}

std::string ReadManifestTargetPlatform(const Json::Value& manifest,
                                       const std::string& backend) {
  std::string target_platform;
  if (ReadStringField(manifest, "target_platform", &target_platform) &&
      !target_platform.empty()) {
    return skity::font_harness::CanonicalPlatformTarget(target_platform);
  }
  if (manifest.isObject() && manifest.isMember("platforms")) {
    return skity::font_harness::InferTargetPlatformFromArray(
        manifest["platforms"], backend);
  }
  return "";
}

bool StripPrefix(std::string* value, const std::string& prefix) {
  if (value->rfind(prefix, 0) != 0) {
    return false;
  }
  value->erase(0, prefix.size());
  return true;
}

std::string UnderscorePlatformPrefix(const std::string& target_platform) {
  std::string prefix = target_platform;
  for (char& c : prefix) {
    if (c == '-') {
      c = '_';
    }
  }
  return prefix + "_";
}

std::string ReportNameForManifestId(std::string manifest_id,
                                    const std::string& target_platform) {
  if (target_platform == "macos-coretext") {
    StripPrefix(&manifest_id, "pc_macos_coretext_");
  } else if (target_platform == "ios-sim-coretext") {
    StripPrefix(&manifest_id, "ios_sim_coretext_");
  } else if (target_platform == "ios-device-coretext") {
    StripPrefix(&manifest_id, "ios_device_coretext_");
  } else if (!target_platform.empty()) {
    StripPrefix(&manifest_id, UnderscorePlatformPrefix(target_platform));
  }

  for (char& c : manifest_id) {
    if (c == '_') {
      c = '-';
    } else if (!IsSafeFileChar(c)) {
      c = '-';
    }
  }
  return SanitizeFileComponent(manifest_id, "manifest") + ".latest.json";
}

std::filesystem::path ReadManifestReportPath(
    const Json::Value& manifest, const std::string& manifest_id,
    const std::string& target_platform) {
  std::filesystem::path report_root = ReadManifestReportRoot(manifest);
  if (report_root.empty()) {
    return {};
  }
  return report_root / ReportNameForManifestId(manifest_id, target_platform);
}

bool WriteArtifactSilently(
    const std::filesystem::path& repo_root,
    const skity::font_harness::ArtifactDescriptor& descriptor,
    Json::Value* report, std::string* stable_path, std::string* error) {
  skity::font_harness::ArtifactWriteResult write_result;
  if (!skity::font_harness::WriteStableArtifact(repo_root, descriptor, report,
                                                &write_result, error)) {
    return false;
  }
  if (stable_path != nullptr) {
    *stable_path = write_result.stable_path;
  }
  return true;
}

Json::Value RunManifestCase(Json::ArrayIndex index,
                            const std::string& case_relative_path,
                            const ManifestRunPaths& paths,
                            skity::font_harness::RepoUriResolver* resolver,
                            ManifestRunCounts* counts) {
  const std::filesystem::path case_path =
      (paths.case_root / case_relative_path).lexically_normal();

  Json::Value item(Json::objectValue);
  item["index"] = index;
  item["case"] = case_relative_path;
  item["case_path"] = StablePathForRun(case_path, paths.repo_root);
  item["backend"] = paths.backend;
  if (!paths.target_platform.empty()) {
    item["target_platform"] = paths.target_platform;
  }
  item["status"] = "running";
  item["artifacts"] = Json::Value(Json::objectValue);

  Json::Value case_root_json;
  std::string case_error;
  if (!skity::font_harness::LoadJsonFile(case_path, &case_root_json,
                                         &case_error)) {
    item["status"] = "schema_validation_failed";
    item["reason_code"] = "schema_validation_failed";
    item["message"] = case_error;
    counts->schema_failure_count += 1;
    return item;
  }

  skity::font_harness::CaseValidationResult case_validation =
      skity::font_harness::ValidateCaseDocument(case_root_json, *resolver);
  const std::string case_id = case_validation.case_id.empty()
                                  ? StemOrFallback(case_path, "case")
                                  : case_validation.case_id;
  item["case_id"] = case_id;
  std::string category;
  std::string status;
  ReadStringField(case_root_json, "category", &category);
  ReadStringField(case_root_json, "status", &status);
  item["category"] = category;
  item["case_status"] = status;

  if (!case_validation.valid) {
    item["status"] = "schema_validation_failed";
    item["reason_code"] = "schema_validation_failed";
    item["validation_errors"] = case_validation.errors.ToJson();
    counts->schema_failure_count += 1;
    return item;
  }
  if (case_validation.backend != paths.backend) {
    item["status"] = "schema_validation_failed";
    item["reason_code"] = "backend_mismatch";
    item["message"] = "case backend does not match manifest run backend";
    counts->schema_failure_count += 1;
    return item;
  }
  if (!paths.target_platform.empty() &&
      !skity::font_harness::PlatformArrayContainsTarget(
          case_root_json["platforms"], paths.target_platform)) {
    item["status"] = "schema_validation_failed";
    item["reason_code"] = "platform_mismatch";
    item["message"] = "case platforms do not include manifest target_platform";
    counts->schema_failure_count += 1;
    return item;
  }
  if (status != "active") {
    item["status"] = "schema_validation_failed";
    item["reason_code"] = "non_active_case_in_smoke";
    item["message"] = "smoke manifest entries must be active cases";
    counts->schema_failure_count += 1;
    return item;
  }

  const std::string safe_case_id = SanitizeFileComponent(case_id, "case");
  const std::string safe_backend =
      SanitizeFileComponent(paths.backend, "backend");
  const std::filesystem::path expected_path =
      paths.resolved_skia_dir / (safe_case_id + ".skia.json");
  const std::filesystem::path actual_path =
      paths.resolved_skity_dir / (safe_case_id + "." + safe_backend + ".json");
  const std::filesystem::path compare_path =
      paths.resolved_compare_dir /
      (safe_case_id + "." + safe_backend + ".compare.json");

  item["artifacts"]["expected"] =
      StablePathForRun(expected_path, paths.repo_root);
  item["artifacts"]["actual"] = StablePathForRun(actual_path, paths.repo_root);
  item["artifacts"]["compare"] =
      StablePathForRun(compare_path, paths.repo_root);

  std::error_code exists_error;
  if (!std::filesystem::is_regular_file(expected_path, exists_error)) {
    item["status"] = "input_failed";
    item["reason_code"] = "missing_oracle";
    item["message"] = "Skia oracle is missing for manifest case";
    item["compare_exit_code"] = kExitCompareInputFailure;
    counts->input_failure_count += 1;
    return item;
  }

  ProbeInvocationResult probe_invocation =
      RunSkityProbeForCase(paths.repo_root, case_path, paths.backend);
  item["probe_exit_code"] = probe_invocation.exit_code;
  skity::font_harness::ArtifactDescriptor probe_descriptor;
  probe_descriptor.kind = skity::font_harness::ArtifactKind::kSkityResult;
  probe_descriptor.command = "probe";
  probe_descriptor.output_flag = "--out";
  probe_descriptor.case_id =
      probe_invocation.case_id.empty() ? case_id : probe_invocation.case_id;
  probe_descriptor.backend = probe_invocation.backend.empty()
                                 ? paths.backend
                                 : probe_invocation.backend;
  probe_descriptor.input_path = case_path;
  probe_descriptor.explicit_output_path = actual_path;
  probe_descriptor.repro_args = {"--case",
                                 StablePathForRun(case_path, paths.repo_root),
                                 "--backend", paths.backend};

  std::string write_error;
  std::string stable_probe_path;
  if (!WriteArtifactSilently(paths.repo_root, probe_descriptor,
                             &probe_invocation.report, &stable_probe_path,
                             &write_error)) {
    item["status"] = "io_failure";
    item["reason_code"] = "io_failure";
    item["message"] = write_error;
    counts->io_failure_count += 1;
    return item;
  }
  item["artifacts"]["actual"] = stable_probe_path;

  if (probe_invocation.exit_code != kExitSuccess) {
    item["status"] = probe_invocation.exit_code == kExitBackendUnavailable
                         ? "backend_unavailable"
                         : "probe_failed";
    item["reason_code"] =
        probe_invocation.report.isMember("reason_code")
            ? probe_invocation.report["reason_code"].asString()
            : "probe_failed";
    if (probe_invocation.exit_code == kExitBackendUnavailable) {
      counts->backend_unavailable_count += 1;
    } else {
      counts->probe_failure_count += 1;
    }
    return item;
  }

  CompareInvocationResult compare_invocation = RunCompareForArtifacts(
      paths.repo_root, case_path, expected_path, actual_path, paths.backend);
  item["compare_exit_code"] = compare_invocation.exit_code;

  skity::font_harness::ArtifactDescriptor compare_descriptor;
  compare_descriptor.kind = skity::font_harness::ArtifactKind::kCompareReport;
  compare_descriptor.command = "compare";
  compare_descriptor.case_id =
      compare_invocation.case_id.empty() ? case_id : compare_invocation.case_id;
  compare_descriptor.backend = compare_invocation.backend.empty()
                                   ? paths.backend
                                   : compare_invocation.backend;
  compare_descriptor.input_path = case_path;
  compare_descriptor.explicit_output_path = compare_path;
  compare_descriptor.repro_args = {
      "--case",     StablePathForRun(case_path, paths.repo_root),
      "--expected", StablePathForRun(expected_path, paths.repo_root),
      "--actual",   StablePathForRun(actual_path, paths.repo_root),
      "--backend",  paths.backend};

  std::string stable_compare_path;
  if (!WriteArtifactSilently(paths.repo_root, compare_descriptor,
                             &compare_invocation.report, &stable_compare_path,
                             &write_error)) {
    item["status"] = "io_failure";
    item["reason_code"] = "io_failure";
    item["message"] = write_error;
    counts->io_failure_count += 1;
    return item;
  }
  item["artifacts"]["compare"] = stable_compare_path;
  item["diff_count"] = compare_invocation.report.isMember("diff_count")
                           ? compare_invocation.report["diff_count"].asInt()
                           : 0;

  if (compare_invocation.exit_code == kExitSuccess) {
    item["status"] = "pass";
    item["reason_code"] = "pass";
    item["passed"] = true;
    counts->pass_count += 1;
  } else if (compare_invocation.exit_code == kExitCompareMismatch) {
    item["status"] = "mismatch";
    item["reason_code"] =
        compare_invocation.report.isMember("reason_code")
            ? compare_invocation.report["reason_code"].asString()
            : "compare_mismatch";
    item["passed"] = false;
    counts->mismatch_count += 1;
  } else {
    item["status"] = "input_failed";
    item["reason_code"] =
        compare_invocation.report.isMember("reason_code")
            ? compare_invocation.report["reason_code"].asString()
            : "compare_input_failed";
    item["passed"] = false;
    counts->input_failure_count += 1;
  }
  return item;
}

int FinalizeManifestRunReport(Json::Value* report,
                              const ManifestRunCounts& counts) {
  (*report)["pass_count"] = counts.pass_count;
  (*report)["mismatch_count"] = counts.mismatch_count;
  (*report)["input_failure_count"] = counts.input_failure_count;
  (*report)["probe_failure_count"] = counts.probe_failure_count;
  (*report)["schema_failure_count"] = counts.schema_failure_count;
  (*report)["backend_unavailable_count"] = counts.backend_unavailable_count;
  (*report)["io_failure_count"] = counts.io_failure_count;

  int exit_code = kExitSuccess;
  std::string run_status = "pass";
  std::string reason_code = "pass";
  if (counts.schema_failure_count > 0) {
    exit_code = kExitSchemaValidationFailed;
    run_status = "failed";
    reason_code = "schema_validation_failed";
  } else if (counts.io_failure_count > 0) {
    exit_code = kExitIOFailure;
    run_status = "failed";
    reason_code = "io_failure";
  } else if (counts.backend_unavailable_count > 0) {
    exit_code = kExitBackendUnavailable;
    run_status = "failed";
    reason_code = "backend_unavailable";
  } else if (counts.input_failure_count > 0) {
    exit_code = kExitCompareInputFailure;
    run_status = "failed";
    reason_code = "compare_input_failed";
  } else if (counts.probe_failure_count > 0) {
    exit_code = kExitSkityProbeFailure;
    run_status = "failed";
    reason_code = "probe_failed";
  } else if (counts.mismatch_count > 0) {
    exit_code = kExitCompareMismatch;
    run_status = "mismatch";
    reason_code = "compare_mismatch";
  }
  (*report)["status"] = run_status;
  (*report)["reason_code"] = reason_code;
  (*report)["passed"] = exit_code == kExitSuccess;
  return exit_code;
}

int RunCaseInfo(int argc, char** argv) {
  std::filesystem::path case_path;
  std::filesystem::path report_path;
  std::filesystem::path repo_root(SKITY_FONT_HARNESS_REPO_ROOT);

  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--case") {
      if (i + 1 >= argc) {
        std::cerr << "--case requires a path\n";
        return kExitUsageError;
      }
      case_path = argv[++i];
    } else if (arg == "--report") {
      if (i + 1 >= argc) {
        std::cerr << "--report requires a path\n";
        return kExitUsageError;
      }
      report_path = argv[++i];
    } else if (arg == "--repo-root") {
      if (i + 1 >= argc) {
        std::cerr << "--repo-root requires a path\n";
        return kExitUsageError;
      }
      repo_root = argv[++i];
    } else if (IsHelpArgument(arg)) {
      PrintUsage(std::cout);
      return kExitSuccess;
    } else {
      std::cerr << "Unknown case-info option: " << arg << "\n";
      return kExitUsageError;
    }
  }

  if (case_path.empty()) {
    std::cerr << "case-info requires --case <path>\n";
    return kExitUsageError;
  }

  Json::Value root;
  std::string error;
  Json::Value report;
  int exit_code = kExitSuccess;
  if (!skity::font_harness::LoadJsonFile(case_path, &root, &error)) {
    report = BuildCaseLoadErrorReport(error);
    exit_code = kExitSchemaValidationFailed;
  } else {
    skity::font_harness::RepoUriResolver resolver(repo_root);
    auto result = skity::font_harness::ValidateCaseDocument(root, resolver);
    report = skity::font_harness::BuildCaseInfoReport(result);
    exit_code = result.valid ? kExitSuccess : kExitSchemaValidationFailed;
  }

  skity::font_harness::ArtifactDescriptor descriptor;
  descriptor.kind = skity::font_harness::ArtifactKind::kCaseInfo;
  descriptor.command = "case-info";
  descriptor.case_id = report["case_id"].asString();
  descriptor.backend = report["backend"].asString();
  descriptor.input_path = case_path;
  descriptor.explicit_output_path = report_path;
  descriptor.repro_args = {"--case", PathArg(case_path)};
  return WriteCommandReport(std::move(report), repo_root, descriptor,
                            exit_code);
}

Json::Value BuildUnsupportedBackendReport(const std::string& backend,
                                          const std::string& artifact_type) {
  Json::Value report(Json::objectValue);
  report["schema_version"] = 1;
  report["artifact_type"] = artifact_type;
  report["backend"] = backend;
  report["backend_available"] = false;
  report["reason_code"] = "backend_unavailable";
  report["message"] = "backend is not supported by this harness command";
  return report;
}

int RunPlatformInfoCommand(int argc, char** argv, const std::string& command) {
  std::string backend;
  std::filesystem::path report_path;
  std::filesystem::path repo_root(SKITY_FONT_HARNESS_REPO_ROOT);

  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--backend") {
      if (i + 1 >= argc) {
        std::cerr << "--backend requires a value\n";
        return kExitUsageError;
      }
      backend = argv[++i];
    } else if (arg == "--report") {
      if (i + 1 >= argc) {
        std::cerr << "--report requires a path\n";
        return kExitUsageError;
      }
      report_path = argv[++i];
    } else if (arg == "--repo-root") {
      if (i + 1 >= argc) {
        std::cerr << "--repo-root requires a path\n";
        return kExitUsageError;
      }
      repo_root = argv[++i];
    } else if (IsHelpArgument(arg)) {
      PrintUsage(std::cout);
      return kExitSuccess;
    } else {
      std::cerr << "Unknown " << command << " option: " << arg << "\n";
      return kExitUsageError;
    }
  }

  if (backend.empty()) {
    std::cerr << command << " requires --backend <backend>\n";
    return kExitUsageError;
  }
  ConfigureFontManagerBackend(backend);

  Json::Value report;
  if (backend == "coretext") {
    skity::font_harness::CoreTextEnvRequest request;
    request.repo_root = repo_root;
    report = command == "list-fonts"
                 ? skity::font_harness::BuildCoreTextFontList(request)
                 : skity::font_harness::BuildCoreTextEnvInfo(request);
  } else if (backend == "directwrite") {
    skity::font_harness::DirectWriteEnvRequest request;
    request.repo_root = repo_root;
    report = command == "list-fonts"
                 ? skity::font_harness::BuildDirectWriteFontList(request)
                 : skity::font_harness::BuildDirectWriteEnvInfo(request);
  } else {
    report = BuildUnsupportedBackendReport(
        backend, command == "list-fonts" ? "font_list_fonts" : "font_env_info");
  }

  skity::font_harness::ArtifactDescriptor descriptor;
  descriptor.kind = command == "list-fonts"
                        ? skity::font_harness::ArtifactKind::kFontList
                        : skity::font_harness::ArtifactKind::kEnvInfo;
  descriptor.command = command;
  descriptor.backend = backend;
  descriptor.explicit_output_path = report_path;
  descriptor.repro_args = BackendReproArgs(backend);

  int exit_code = report["backend_available"].asBool()
                      ? kExitSuccess
                      : kExitBackendUnavailable;
  return WriteCommandReport(std::move(report), repo_root, descriptor,
                            exit_code);
}

int RunDumpPathCommand(int argc, char** argv) {
  std::filesystem::path case_path;
  std::filesystem::path output_path;
  std::filesystem::path repo_root(SKITY_FONT_HARNESS_REPO_ROOT);
  std::string backend;

  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--case") {
      if (i + 1 >= argc) {
        std::cerr << "--case requires a path\n";
        return kExitUsageError;
      }
      case_path = argv[++i];
    } else if (arg == "--backend") {
      if (i + 1 >= argc) {
        std::cerr << "--backend requires a value\n";
        return kExitUsageError;
      }
      backend = argv[++i];
    } else if (arg == "--out" || arg == "--report") {
      if (i + 1 >= argc) {
        std::cerr << arg << " requires a path\n";
        return kExitUsageError;
      }
      output_path = argv[++i];
    } else if (arg == "--repo-root") {
      if (i + 1 >= argc) {
        std::cerr << "--repo-root requires a path\n";
        return kExitUsageError;
      }
      repo_root = argv[++i];
    } else if (IsHelpArgument(arg)) {
      PrintUsage(std::cout);
      return kExitSuccess;
    } else {
      std::cerr << "Unknown dump-path option: " << arg << "\n";
      return kExitUsageError;
    }
  }

  if (case_path.empty()) {
    std::cerr << "dump-path requires --case <path>\n";
    return kExitUsageError;
  }
  if (backend.empty()) {
    std::cerr << "dump-path requires --backend <backend>\n";
    return kExitUsageError;
  }

  skity::font_harness::GlyphPathProbeRequest request;
  request.repo_root = repo_root;
  request.case_path = case_path;
  request.backend = backend;
  ConfigureFontManagerBackend(backend);
  skity::font_harness::GlyphPathProbeResult result =
      skity::font_harness::RunGlyphPathDump(request);
  Json::Value report = std::move(result.report);
  const int exit_code = ExitCodeForProbeStatus(result.status);

  skity::font_harness::ArtifactDescriptor descriptor;
  descriptor.kind = skity::font_harness::ArtifactKind::kPathDump;
  descriptor.command = "dump-path";
  descriptor.output_flag = "--out";
  descriptor.case_id = result.case_id.empty()
                           ? StemOrFallback(case_path, "case")
                           : result.case_id;
  descriptor.backend = result.backend.empty() ? backend : result.backend;
  descriptor.input_path = case_path;
  descriptor.explicit_output_path = output_path;
  descriptor.repro_args = {"--case", PathArg(case_path), "--backend", backend};

  return WriteCommandReport(std::move(report), repo_root, descriptor,
                            exit_code);
}

int RunProbeCommand(int argc, char** argv) {
  std::filesystem::path case_path;
  std::filesystem::path output_path;
  std::filesystem::path repo_root(SKITY_FONT_HARNESS_REPO_ROOT);
  std::string backend;

  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--case") {
      if (i + 1 >= argc) {
        std::cerr << "--case requires a path\n";
        return kExitUsageError;
      }
      case_path = argv[++i];
    } else if (arg == "--backend") {
      if (i + 1 >= argc) {
        std::cerr << "--backend requires a value\n";
        return kExitUsageError;
      }
      backend = argv[++i];
    } else if (arg == "--out" || arg == "--report") {
      if (i + 1 >= argc) {
        std::cerr << arg << " requires a path\n";
        return kExitUsageError;
      }
      output_path = argv[++i];
    } else if (arg == "--repo-root") {
      if (i + 1 >= argc) {
        std::cerr << "--repo-root requires a path\n";
        return kExitUsageError;
      }
      repo_root = argv[++i];
    } else if (IsHelpArgument(arg)) {
      PrintUsage(std::cout);
      return kExitSuccess;
    } else {
      std::cerr << "Unknown probe option: " << arg << "\n";
      return kExitUsageError;
    }
  }

  if (case_path.empty()) {
    std::cerr << "probe requires --case <path>\n";
    return kExitUsageError;
  }
  if (backend.empty()) {
    std::cerr << "probe requires --backend <backend>\n";
    return kExitUsageError;
  }

  ProbeInvocationResult invocation =
      RunSkityProbeForCase(repo_root, case_path, backend);

  skity::font_harness::ArtifactDescriptor descriptor;
  descriptor.kind = skity::font_harness::ArtifactKind::kSkityResult;
  descriptor.command = "probe";
  descriptor.output_flag = "--out";
  descriptor.case_id = invocation.case_id.empty()
                           ? StemOrFallback(case_path, "case")
                           : invocation.case_id;
  descriptor.backend =
      invocation.backend.empty() ? backend : invocation.backend;
  descriptor.input_path = case_path;
  descriptor.explicit_output_path = output_path;
  descriptor.repro_args = {"--case", PathArg(case_path), "--backend", backend};

  return WriteCommandReport(std::move(invocation.report), repo_root, descriptor,
                            invocation.exit_code);
}

int RunMatchCommand(int argc, char** argv) {
  std::filesystem::path case_path;
  std::filesystem::path output_path;
  std::filesystem::path repo_root(SKITY_FONT_HARNESS_REPO_ROOT);
  std::string backend;

  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--case") {
      if (i + 1 >= argc) {
        std::cerr << "--case requires a path\n";
        return kExitUsageError;
      }
      case_path = argv[++i];
    } else if (arg == "--backend") {
      if (i + 1 >= argc) {
        std::cerr << "--backend requires a value\n";
        return kExitUsageError;
      }
      backend = argv[++i];
    } else if (arg == "--out" || arg == "--report") {
      if (i + 1 >= argc) {
        std::cerr << arg << " requires a path\n";
        return kExitUsageError;
      }
      output_path = argv[++i];
    } else if (arg == "--repo-root") {
      if (i + 1 >= argc) {
        std::cerr << "--repo-root requires a path\n";
        return kExitUsageError;
      }
      repo_root = argv[++i];
    } else if (IsHelpArgument(arg)) {
      PrintUsage(std::cout);
      return kExitSuccess;
    } else {
      std::cerr << "Unknown match option: " << arg << "\n";
      return kExitUsageError;
    }
  }

  if (case_path.empty()) {
    std::cerr << "match requires --case <path>\n";
    return kExitUsageError;
  }
  if (backend.empty()) {
    std::cerr << "match requires --backend <backend>\n";
    return kExitUsageError;
  }

  Json::Value case_root;
  std::string error;
  std::string case_id = StemOrFallback(case_path, "case");
  if (!skity::font_harness::LoadJsonFile(case_path, &case_root, &error)) {
    Json::Value report =
        BuildMatchSchemaErrorReport(case_id, backend, "--case", error);

    skity::font_harness::ArtifactDescriptor descriptor;
    descriptor.kind = skity::font_harness::ArtifactKind::kMatchResult;
    descriptor.command = "match";
    descriptor.output_flag = "--out";
    descriptor.case_id = case_id;
    descriptor.backend = backend;
    descriptor.input_path = case_path;
    descriptor.explicit_output_path = output_path;
    descriptor.repro_args = {"--case", PathArg(case_path), "--backend",
                             backend};
    return WriteCommandReport(std::move(report), repo_root, descriptor,
                              kExitSchemaValidationFailed);
  }

  ReadStringField(case_root, "id", &case_id);
  std::string category;
  ReadStringField(case_root, "category", &category);
  if (!IsFontManagerProbeCategory(category)) {
    Json::Value report = BuildMatchSchemaErrorReport(
        case_id, backend, "$.category",
        "match supports only font_manager and family_style_set cases");

    skity::font_harness::ArtifactDescriptor descriptor;
    descriptor.kind = skity::font_harness::ArtifactKind::kMatchResult;
    descriptor.command = "match";
    descriptor.output_flag = "--out";
    descriptor.case_id = case_id;
    descriptor.backend = backend;
    descriptor.input_path = case_path;
    descriptor.explicit_output_path = output_path;
    descriptor.repro_args = {"--case", PathArg(case_path), "--backend",
                             backend};
    return WriteCommandReport(std::move(report), repo_root, descriptor,
                              kExitSchemaValidationFailed);
  }

  skity::font_harness::FontManagerProbeRequest request;
  request.repo_root = repo_root;
  request.case_path = case_path;
  request.backend = backend;
  ConfigureFontManagerBackend(backend);
  skity::font_harness::FontManagerProbeResult result =
      skity::font_harness::RunFontManagerProbe(request);
  const int exit_code = ExitCodeForProbeStatus(result.status);

  skity::font_harness::ArtifactDescriptor descriptor;
  descriptor.kind = skity::font_harness::ArtifactKind::kMatchResult;
  descriptor.command = "match";
  descriptor.output_flag = "--out";
  descriptor.case_id = result.case_id.empty() ? case_id : result.case_id;
  descriptor.backend = result.backend.empty() ? backend : result.backend;
  descriptor.input_path = case_path;
  descriptor.explicit_output_path = output_path;
  descriptor.repro_args = {"--case", PathArg(case_path), "--backend", backend};

  return WriteCommandReport(std::move(result.report), repo_root, descriptor,
                            exit_code);
}

int RunCompareCommand(int argc, char** argv) {
  std::filesystem::path case_path;
  std::filesystem::path expected_path;
  std::filesystem::path actual_path;
  std::filesystem::path report_path;
  std::filesystem::path repo_root(SKITY_FONT_HARNESS_REPO_ROOT);
  std::string backend;

  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--case") {
      if (i + 1 >= argc) {
        std::cerr << "--case requires a path\n";
        return kExitUsageError;
      }
      case_path = argv[++i];
    } else if (arg == "--expected") {
      if (i + 1 >= argc) {
        std::cerr << "--expected requires a path\n";
        return kExitUsageError;
      }
      expected_path = argv[++i];
    } else if (arg == "--actual") {
      if (i + 1 >= argc) {
        std::cerr << "--actual requires a path\n";
        return kExitUsageError;
      }
      actual_path = argv[++i];
    } else if (arg == "--report") {
      if (i + 1 >= argc) {
        std::cerr << "--report requires a path\n";
        return kExitUsageError;
      }
      report_path = argv[++i];
    } else if (arg == "--backend") {
      if (i + 1 >= argc) {
        std::cerr << "--backend requires a value\n";
        return kExitUsageError;
      }
      backend = argv[++i];
    } else if (arg == "--repo-root") {
      if (i + 1 >= argc) {
        std::cerr << "--repo-root requires a path\n";
        return kExitUsageError;
      }
      repo_root = argv[++i];
    } else if (IsHelpArgument(arg)) {
      PrintUsage(std::cout);
      return kExitSuccess;
    } else {
      std::cerr << "Unknown compare option: " << arg << "\n";
      return kExitUsageError;
    }
  }

  if (case_path.empty() || expected_path.empty() || actual_path.empty()) {
    std::cerr << "compare requires --case, --expected, and --actual\n";
    return kExitUsageError;
  }

  const CaseMetadata metadata = ReadCaseMetadata(case_path);
  const std::string case_id = metadata.case_id.empty()
                                  ? StemOrFallback(case_path, "case")
                                  : metadata.case_id;
  if (backend.empty()) {
    backend = metadata.backend.empty() ? "unknown" : metadata.backend;
  }

  CompareInvocationResult invocation =
      RunCompareForArtifacts(repo_root, case_path, expected_path, actual_path,
                             backend == "unknown" ? "" : backend);

  skity::font_harness::ArtifactDescriptor descriptor;
  descriptor.kind = skity::font_harness::ArtifactKind::kCompareReport;
  descriptor.command = "compare";
  descriptor.case_id =
      invocation.case_id.empty() ? case_id : invocation.case_id;
  descriptor.backend =
      invocation.backend.empty() ? backend : invocation.backend;
  descriptor.input_path = case_path;
  descriptor.explicit_output_path = report_path;
  descriptor.repro_args = {"--case",     PathArg(case_path),
                           "--expected", PathArg(expected_path),
                           "--actual",   PathArg(actual_path)};
  if (descriptor.backend != "unknown") {
    descriptor.repro_args.push_back("--backend");
    descriptor.repro_args.push_back(descriptor.backend);
  }

  return WriteCommandReport(std::move(invocation.report), repo_root, descriptor,
                            invocation.exit_code);
}

int RunManifestCommand(int argc, char** argv) {
  std::filesystem::path manifest_path;
  std::filesystem::path skia_dir;
  std::filesystem::path skity_dir;
  std::filesystem::path report_path;
  std::filesystem::path repo_root(SKITY_FONT_HARNESS_REPO_ROOT);
  std::string backend;
  bool skia_dir_overridden = false;
  bool skity_dir_overridden = false;

  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--manifest") {
      if (i + 1 >= argc) {
        std::cerr << "--manifest requires a path\n";
        return kExitUsageError;
      }
      manifest_path = argv[++i];
    } else if (arg == "--backend") {
      if (i + 1 >= argc) {
        std::cerr << "--backend requires a value\n";
        return kExitUsageError;
      }
      backend = argv[++i];
    } else if (arg == "--skia-dir") {
      if (i + 1 >= argc) {
        std::cerr << "--skia-dir requires a path\n";
        return kExitUsageError;
      }
      skia_dir = argv[++i];
      skia_dir_overridden = true;
    } else if (arg == "--skity-dir") {
      if (i + 1 >= argc) {
        std::cerr << "--skity-dir requires a path\n";
        return kExitUsageError;
      }
      skity_dir = argv[++i];
      skity_dir_overridden = true;
    } else if (arg == "--report") {
      if (i + 1 >= argc) {
        std::cerr << "--report requires a path\n";
        return kExitUsageError;
      }
      report_path = argv[++i];
    } else if (arg == "--repo-root") {
      if (i + 1 >= argc) {
        std::cerr << "--repo-root requires a path\n";
        return kExitUsageError;
      }
      repo_root = argv[++i];
    } else if (IsHelpArgument(arg)) {
      PrintUsage(std::cout);
      return kExitSuccess;
    } else {
      std::cerr << "Unknown run option: " << arg << "\n";
      return kExitUsageError;
    }
  }

  if (manifest_path.empty() || backend.empty()) {
    std::cerr << "run requires --manifest and --backend\n";
    return kExitUsageError;
  }

  const ManifestMetadata metadata = ReadManifestMetadata(manifest_path);
  std::string manifest_id = metadata.manifest_id.empty()
                                ? StemOrFallback(manifest_path, "manifest")
                                : metadata.manifest_id;

  Json::Value manifest_root;
  std::string load_error;
  if (!skity::font_harness::LoadJsonFile(manifest_path, &manifest_root,
                                         &load_error)) {
    Json::Value report = skity::font_harness::BuildRunSummaryReport(
        manifest_id, backend, "schema_validation_failed");
    report["status"] = "invalid";
    report["passed"] = false;
    report["message"] = load_error;

    skity::font_harness::ArtifactDescriptor descriptor;
    descriptor.kind = skity::font_harness::ArtifactKind::kRunSummary;
    descriptor.command = "run";
    descriptor.case_id = "";
    descriptor.manifest_id = manifest_id;
    descriptor.backend = backend;
    descriptor.input_path = manifest_path;
    descriptor.explicit_output_path = report_path;
    descriptor.repro_args = {"--manifest", PathArg(manifest_path), "--backend",
                             backend};
    return WriteCommandReport(std::move(report), repo_root, descriptor,
                              kExitSchemaValidationFailed);
  }

  std::string target_platform =
      ReadManifestTargetPlatform(manifest_root, backend);
  if (report_path.empty()) {
    report_path =
        ReadManifestReportPath(manifest_root, manifest_id, target_platform);
  }

  skity::font_harness::RepoUriResolver resolver(repo_root);
  skity::font_harness::ManifestValidationResult manifest_validation =
      skity::font_harness::ValidateManifestDocument(manifest_root, resolver);
  if (!manifest_validation.manifest_id.empty()) {
    manifest_id = manifest_validation.manifest_id;
  }
  if (!manifest_validation.target_platform.empty()) {
    target_platform = manifest_validation.target_platform;
  }
  if (report_path.empty()) {
    report_path =
        ReadManifestReportPath(manifest_root, manifest_id, target_platform);
  }

  if (!manifest_validation.valid || manifest_validation.backend != backend) {
    Json::Value report = skity::font_harness::BuildRunSummaryReport(
        manifest_id, backend, "schema_validation_failed");
    report["status"] = "invalid";
    report["passed"] = false;
    if (!target_platform.empty()) {
      report["target_platform"] = target_platform;
    }
    report["manifest_path"] =
        StablePathForRun(ResolveRunPath(repo_root, manifest_path), repo_root);
    report["normalized_manifest"] = manifest_validation.normalized_manifest;
    report["validation_errors"] = manifest_validation.errors.ToJson();
    if (manifest_validation.valid && manifest_validation.backend != backend) {
      report["reason_code"] = "backend_mismatch";
      report["message"] = "manifest backend does not match --backend";
    }

    skity::font_harness::ArtifactDescriptor descriptor;
    descriptor.kind = skity::font_harness::ArtifactKind::kRunSummary;
    descriptor.command = "run";
    descriptor.case_id = "";
    descriptor.manifest_id = manifest_id;
    descriptor.backend = backend;
    descriptor.input_path = manifest_path;
    descriptor.explicit_output_path = report_path;
    descriptor.repro_args = {"--manifest", PathArg(manifest_path), "--backend",
                             backend};
    return WriteCommandReport(std::move(report), repo_root, descriptor,
                              kExitSchemaValidationFailed);
  }

  const std::filesystem::path artifact_root =
      ReadManifestArtifactRoot(manifest_root);
  const std::filesystem::path report_root =
      ReadManifestReportRoot(manifest_root);

  if (skia_dir.empty()) {
    skia_dir = ReadManifestArtifactRoleDir(manifest_root, artifact_root, "skia",
                                           "skia_dir",
                                           "local/font-harness/artifacts/skia");
  }
  if (skity_dir.empty()) {
    skity_dir = ReadManifestArtifactRoleDir(
        manifest_root, artifact_root, "skity", "skity_dir",
        "local/font-harness/artifacts/skity");
  }
  std::filesystem::path compare_dir = ReadManifestArtifactRoleDir(
      manifest_root, artifact_root, "compare", "compare_dir",
      "local/font-harness/artifacts/compare");

  const std::filesystem::path resolved_skia_dir =
      ResolveRunPath(repo_root, skia_dir);
  const std::filesystem::path resolved_skity_dir =
      ResolveRunPath(repo_root, skity_dir);
  const std::filesystem::path resolved_compare_dir =
      ResolveRunPath(repo_root, compare_dir);
  const std::filesystem::path resolved_artifact_root =
      artifact_root.empty() ? std::filesystem::path()
                            : ResolveRunPath(repo_root, artifact_root);
  const std::filesystem::path resolved_report_root =
      report_root.empty() ? std::filesystem::path()
                          : ResolveRunPath(repo_root, report_root);

  Json::Value report =
      skity::font_harness::BuildRunSummaryReport(manifest_id, backend, "pass");
  report["status"] = "running";
  report["passed"] = false;
  report["runner"] = "host-cli";
  report["expected_runner"] = "host-cli";
  report["actual_runner"] = "host-cli";
  report["manifest_path"] =
      StablePathForRun(ResolveRunPath(repo_root, manifest_path), repo_root);
  if (!target_platform.empty()) {
    report["target_platform"] = target_platform;
  }
  report["artifacts"] = Json::Value(Json::objectValue);
  report["artifacts"]["manifest"] =
      StablePathForRun(ResolveRunPath(repo_root, manifest_path), repo_root);
  if (!resolved_artifact_root.empty()) {
    report["artifacts"]["root"] =
        StablePathForRun(resolved_artifact_root, repo_root);
  }
  if (!resolved_report_root.empty()) {
    report["artifacts"]["report_root"] =
        StablePathForRun(resolved_report_root, repo_root);
  }
  report["artifacts"]["skia_dir"] =
      StablePathForRun(resolved_skia_dir, repo_root);
  report["artifacts"]["skity_dir"] =
      StablePathForRun(resolved_skity_dir, repo_root);
  report["artifacts"]["compare_dir"] =
      StablePathForRun(resolved_compare_dir, repo_root);

  const std::string case_root_value = manifest_root["case_root"].asString();
  const Json::Value& cases = manifest_root["cases"];
  report["case_root"] = case_root_value;

  ManifestRunPaths paths;
  paths.case_root = ResolveRunPath(repo_root, case_root_value);
  paths.repo_root = repo_root;
  paths.resolved_skia_dir = resolved_skia_dir;
  paths.resolved_skity_dir = resolved_skity_dir;
  paths.resolved_compare_dir = resolved_compare_dir;
  paths.backend = backend;
  paths.target_platform = target_platform;

  ManifestRunCounts counts;
  Json::Value case_reports(Json::arrayValue);
  for (Json::ArrayIndex i = 0; i < cases.size(); ++i) {
    const std::string case_relative_path = cases[i].asString();
    case_reports.append(
        RunManifestCase(i, case_relative_path, paths, &resolver, &counts));
  }

  report["cases"] = std::move(case_reports);
  report["case_count"] = static_cast<int>(cases.size());
  const int exit_code = FinalizeManifestRunReport(&report, counts);

  skity::font_harness::ArtifactDescriptor descriptor;
  descriptor.kind = skity::font_harness::ArtifactKind::kRunSummary;
  descriptor.command = "run";
  descriptor.case_id = "";
  descriptor.manifest_id = manifest_id;
  descriptor.backend = backend;
  descriptor.input_path = manifest_path;
  descriptor.explicit_output_path = report_path;
  descriptor.repro_args = {"--manifest", PathArg(manifest_path), "--backend",
                           backend};
  if (skia_dir_overridden && !skia_dir.empty()) {
    descriptor.repro_args.push_back("--skia-dir");
    descriptor.repro_args.push_back(PathArg(skia_dir));
  }
  if (skity_dir_overridden && !skity_dir.empty()) {
    descriptor.repro_args.push_back("--skity-dir");
    descriptor.repro_args.push_back(PathArg(skity_dir));
  }

  return WriteCommandReport(std::move(report), repo_root, descriptor,
                            exit_code);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc <= 1 || IsHelpArgument(argv[1])) {
    PrintUsage(std::cout);
    return kExitSuccess;
  }

  const std::string command = argv[1];
  if (!command.empty() && command[0] == '-') {
    std::cerr << "Unknown option: " << command << "\n\n";
    PrintUsage(std::cerr);
    return kExitUsageError;
  }

  if (command == "case-info") {
    return RunCaseInfo(argc, argv);
  }
  if (command == "env-info" || command == "list-fonts") {
    return RunPlatformInfoCommand(argc, argv, command);
  }
  if (command == "probe") {
    return RunProbeCommand(argc, argv);
  }
  if (command == "match") {
    return RunMatchCommand(argc, argv);
  }
  if (command == "dump-path") {
    return RunDumpPathCommand(argc, argv);
  }
  if (command == "compare") {
    return RunCompareCommand(argc, argv);
  }
  if (command == "run") {
    return RunManifestCommand(argc, argv);
  }

  std::cerr << "Unknown command: " << command << "\n";
  return kExitUsageError;
}
