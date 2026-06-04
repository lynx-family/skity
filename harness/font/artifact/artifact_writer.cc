// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/artifact/artifact_writer.hpp"

#include <cctype>
#include <sstream>
#include <system_error>

#include "harness/font/artifact/json_io.hpp"

namespace skity {
namespace font_harness {

namespace {

std::filesystem::path NormalizeRepoRoot(const std::filesystem::path& repo_root,
                                        std::string* error) {
  std::error_code ec;
  std::filesystem::path normalized =
      std::filesystem::weakly_canonical(repo_root, ec);
  if (!ec) {
    return normalized;
  }
  if (error != nullptr) {
    *error = "failed to resolve repository root: " + ec.message();
  }
  return {};
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
  if (value.empty()) {
    return fallback;
  }
  return value;
}

std::string StemFromPath(const std::filesystem::path& path,
                         const std::string& fallback) {
  std::string stem = path.stem().string();
  if (stem.empty()) {
    return fallback;
  }
  return SanitizeFileComponent(stem, fallback);
}

std::string ArtifactBaseName(const ArtifactDescriptor& descriptor) {
  if (!descriptor.case_id.empty()) {
    return SanitizeFileComponent(descriptor.case_id, "case");
  }
  if (!descriptor.manifest_id.empty()) {
    return SanitizeFileComponent(descriptor.manifest_id, "manifest");
  }
  return StemFromPath(descriptor.input_path, "artifact");
}

std::string BackendSuffix(const ArtifactDescriptor& descriptor) {
  if (descriptor.backend.empty()) {
    return "";
  }
  return "." + SanitizeFileComponent(descriptor.backend, "backend");
}

std::filesystem::path DefaultArtifactPath(
    const std::filesystem::path& repo_root,
    const ArtifactDescriptor& descriptor) {
  const std::filesystem::path local_root = repo_root / "local" / "font-harness";
  const std::string name = ArtifactBaseName(descriptor);
  const std::string backend = BackendSuffix(descriptor);

  switch (descriptor.kind) {
    case ArtifactKind::kCaseInfo:
      return local_root / "artifacts" / "compare" /
             (name + backend + ".case-info.json");
    case ArtifactKind::kEnvInfo:
      return local_root / "artifacts" / "env" /
             (SanitizeFileComponent(descriptor.backend, "backend") +
              ".env-info.json");
    case ArtifactKind::kFontList:
      return local_root / "artifacts" / "env" /
             (SanitizeFileComponent(descriptor.backend, "backend") +
              ".fonts.json");
    case ArtifactKind::kSkityResult:
      return local_root / "artifacts" / "skity" / (name + backend + ".json");
    case ArtifactKind::kCompareReport:
      return local_root / "artifacts" / "compare" /
             (name + backend + ".compare.json");
    case ArtifactKind::kRunSummary:
      return local_root / "reports" / (name + backend + ".run.json");
    case ArtifactKind::kPathDump:
      return local_root / "artifacts" / "path" /
             (name + backend + ".path.json");
  }
  return local_root / "artifacts" / (name + backend + ".json");
}

std::string StablePathFor(const std::filesystem::path& absolute_path,
                          const std::filesystem::path& repo_root) {
  std::error_code ec;
  auto relative = std::filesystem::relative(absolute_path, repo_root, ec);
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
  return absolute_path.lexically_normal().string();
}

std::string QuoteArg(const std::string& arg) {
  if (arg.empty()) {
    return "''";
  }

  bool needs_quote = false;
  for (char c : arg) {
    if (std::isspace(static_cast<unsigned char>(c)) || c == '\'' || c == '"' ||
        c == '\\') {
      needs_quote = true;
      break;
    }
  }
  if (!needs_quote) {
    return arg;
  }

  std::string quoted = "'";
  for (char c : arg) {
    if (c == '\'') {
      quoted += "'\\''";
    } else {
      quoted += c;
    }
  }
  quoted += "'";
  return quoted;
}

std::string BuildMinimalRepro(const ArtifactDescriptor& descriptor,
                              const std::string& stable_path) {
  std::vector<std::string> args = {"skity-font", descriptor.command};
  args.insert(args.end(), descriptor.repro_args.begin(),
              descriptor.repro_args.end());
  if (!descriptor.output_flag.empty()) {
    args.push_back(descriptor.output_flag);
    args.push_back(stable_path);
  }

  std::ostringstream stream;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) {
      stream << ' ';
    }
    stream << QuoteArg(args[i]);
  }
  return stream.str();
}

void FillCommonFields(const ArtifactDescriptor& descriptor,
                      const ArtifactWriteResult& write_result,
                      Json::Value* report) {
  if (!descriptor.case_id.empty() && !report->isMember("case_id")) {
    (*report)["case_id"] = descriptor.case_id;
  }
  if (!descriptor.manifest_id.empty() && !report->isMember("manifest_id")) {
    (*report)["manifest_id"] = descriptor.manifest_id;
  }
  if (!descriptor.backend.empty() && !report->isMember("backend")) {
    (*report)["backend"] = descriptor.backend;
  }
  (*report)["artifact_path"] = write_result.stable_path;
  (*report)["minimal_repro_command"] = write_result.minimal_repro_command;
}

std::string ReportStatus(const Json::Value& report, int exit_code) {
  if (exit_code == 0) {
    if (report.isMember("valid") && !report["valid"].asBool()) {
      return "invalid";
    }
    return "ok";
  }
  if (report.isMember("reason_code")) {
    return report["reason_code"].asString();
  }
  if (report.isMember("valid") && !report["valid"].asBool()) {
    return "schema_validation_failed";
  }
  return "failed";
}

void AppendIfPresent(std::ostringstream* stream, const Json::Value& report,
                     const std::string& field) {
  if (report.isMember(field) && report[field].isString() &&
      !report[field].asString().empty()) {
    *stream << ' ' << field << '=' << report[field].asString();
  }
}

}  // namespace

std::filesystem::path ResolveArtifactPath(
    const std::filesystem::path& repo_root,
    const ArtifactDescriptor& descriptor) {
  if (!descriptor.explicit_output_path.empty()) {
    if (descriptor.explicit_output_path.is_absolute()) {
      return descriptor.explicit_output_path.lexically_normal();
    }
    return (repo_root / descriptor.explicit_output_path).lexically_normal();
  }
  return DefaultArtifactPath(repo_root, descriptor).lexically_normal();
}

bool WriteStableArtifact(const std::filesystem::path& repo_root,
                         const ArtifactDescriptor& descriptor,
                         Json::Value* report, ArtifactWriteResult* result,
                         std::string* error) {
  if (report == nullptr || result == nullptr) {
    if (error != nullptr) {
      *error = "artifact writer requires report and result";
    }
    return false;
  }

  const std::filesystem::path normalized_repo =
      NormalizeRepoRoot(repo_root, error);
  if (normalized_repo.empty()) {
    return false;
  }

  result->absolute_path = ResolveArtifactPath(normalized_repo, descriptor);
  result->stable_path = StablePathFor(result->absolute_path, normalized_repo);
  result->minimal_repro_command =
      BuildMinimalRepro(descriptor, result->stable_path);
  FillCommonFields(descriptor, *result, report);

  return WriteJsonFile(result->absolute_path, *report, error);
}

std::string BuildHumanSummary(const std::string& command,
                              const Json::Value& report, int exit_code) {
  std::ostringstream stream;
  stream << "font-harness command=" << command << " exit_code=" << exit_code
         << " status=" << ReportStatus(report, exit_code);
  AppendIfPresent(&stream, report, "case_id");
  AppendIfPresent(&stream, report, "manifest_id");
  AppendIfPresent(&stream, report, "backend");
  AppendIfPresent(&stream, report, "artifact_path");
  return stream.str();
}

Json::Value BuildProbeResultReport(const std::string& case_id,
                                   const std::string& backend,
                                   const std::string& reason_code) {
  Json::Value report(Json::objectValue);
  report["schema_version"] = 1;
  report["artifact_type"] = "font_probe_result";
  report["runner"] = "skity";
  report["case_id"] = case_id;
  report["backend"] = backend;
  report["ok"] = false;
  report["reason_code"] = reason_code;
  return report;
}

Json::Value BuildCompareReport(const std::string& case_id,
                               const std::string& backend,
                               const std::string& stage,
                               const std::string& reason_code) {
  Json::Value report(Json::objectValue);
  report["schema_version"] = 1;
  report["artifact_type"] = "font_compare_report";
  report["case_id"] = case_id;
  report["backend"] = backend;
  report["stage"] = stage;
  report["reason_code"] = reason_code;
  report["passed"] = false;
  report["diff_path"] = "";
  report["artifacts"] = Json::Value(Json::objectValue);
  return report;
}

Json::Value BuildRunSummaryReport(const std::string& manifest_id,
                                  const std::string& backend,
                                  const std::string& reason_code) {
  Json::Value report(Json::objectValue);
  report["schema_version"] = 1;
  report["artifact_type"] = "font_run_summary";
  report["manifest_id"] = manifest_id;
  report["backend"] = backend;
  report["status"] = "not_run";
  report["reason_code"] = reason_code;
  report["cases"] = Json::Value(Json::arrayValue);
  return report;
}

Json::Value BuildPathDumpReport(const std::string& case_id,
                                const std::string& backend,
                                const std::string& reason_code) {
  Json::Value report(Json::objectValue);
  report["schema_version"] = 1;
  report["artifact_type"] = "font_path_dump";
  report["case_id"] = case_id;
  report["backend"] = backend;
  report["ok"] = false;
  report["reason_code"] = reason_code;
  return report;
}

}  // namespace font_harness
}  // namespace skity
