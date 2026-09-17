// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/artifact/provenance.hpp"

#include <set>

#include "harness/font/artifact/json_io.hpp"

namespace skity {
namespace font_harness {
namespace {

bool ReadEnvironment(const std::filesystem::path& path, Json::Value* value,
                     ValidationContext* errors) {
  std::string message;
  if (path.empty() || !LoadJsonFile(path, value, &message) ||
      !value->isObject() || (*value)["schema_version"] != 1 ||
      !(*value)["fonts"].isArray() || !(*value)["platform"].isString()) {
    errors->AddError("$.environment",
                     "system/controlled probes require a current environment "
                     "snapshot via --environment: " +
                         message);
    return false;
  }
  return true;
}

bool CheckHash(const Json::Value& object, const std::string& field,
               const std::string& path, ValidationContext* errors) {
  std::string value;
  return RequireObject(object, path, errors) &&
         RequireStringField(object, field, path, errors, &value) &&
         ValidateSha256String(value, ChildPath(path, field), errors);
}

void CheckAttachments(const Json::Value& attachments,
                      const std::filesystem::path& artifact,
                      ValidationContext* errors) {
  if (!attachments.isArray() || attachments.empty()) {
    errors->AddError("$.provenance.attachments", "missing bundle attachments");
    return;
  }
  std::error_code root_error;
  const auto root =
      std::filesystem::weakly_canonical(artifact.parent_path(), root_error);
  if (root_error) {
    errors->AddError("$.provenance.attachments", root_error.message());
    return;
  }
  std::set<std::string> paths;
  for (Json::ArrayIndex i = 0; i < attachments.size(); ++i) {
    const auto& item = attachments[i];
    const auto label = IndexPath("$.provenance.attachments", i);
    std::string path;
    if (!RequireObject(item, label, errors) ||
        !RequireStringField(item, "path", label, errors, &path) ||
        !CheckHash(item, "sha256", label, errors)) {
      continue;
    }
    const auto relative = std::filesystem::u8path(path);
    std::error_code ec;
    const auto resolved =
        std::filesystem::weakly_canonical(root / relative, ec);
    const auto inside = resolved.lexically_relative(root);
    if (path.empty() || relative.is_absolute() || relative.has_root_name() ||
        ec || inside.empty() || *inside.begin() == ".." ||
        !paths.insert(path).second) {
      errors->AddError(label + ".path",
                       "attachment must stay inside its bundle");
      continue;
    }
    std::string digest, error;
    if (!FileSha256(resolved, &digest, &error) || item["sha256"] != digest) {
      errors->AddError(label + ".sha256",
                       "missing or changed attachment: " + error);
    }
  }
}

void CheckControlledFonts(const CaseValidationResult& input,
                          const Json::Value& artifact,
                          ValidationContext* errors) {
  const auto& c = input.normalized_case;
  const auto& inventory = artifact["fontconfig_inventory"];
  if (!inventory.isObject() || !inventory["files"].isArray()) {
    errors->AddError("$.fontconfig_inventory",
                     "missing controlled font inventory");
    return;
  }
  Json::Value fixture;
  std::string error;
  const auto& declared = artifact["provenance"]["controlled_fonts"];
  if (!declared.isArray() || inventory["files"] != declared) {
    errors->AddError("$.provenance.controlled_fonts",
                     "inventory must equal the captured controlled font set");
  }
  std::set<std::string> unique;
  for (const auto& font : inventory["files"]) {
    if (!font.isString() || font.asString().rfind("repo://", 0) != 0 ||
        !unique.insert(font.asString()).second) {
      errors->AddError("$.fontconfig_inventory.files",
                       "expected unique repository font URIs");
    }
  }
  const bool empty = c["fontconfig_fixture"] == "empty";
  std::set<std::string> allowed;
  if (!empty) {
    if (!LoadJsonFile(input.repo_root /
                          "harness/font/platform/linux/fontconfig/fonts.json",
                      &fixture, &error) ||
        !fixture["font_files"].isArray()) {
      errors->AddError("$.fontconfig_inventory",
                       "cannot read fixture: " + error);
      return;
    }
    for (const auto& uri : fixture["font_files"]) {
      allowed.insert(uri.asString());
    }
  }
  if (unique != allowed) {
    errors->AddError("$.fontconfig_inventory.files",
                     "inventory differs from repository controlled fixture");
  }
  if (empty != inventory["files"].empty()) {
    errors->AddError("$.fontconfig_inventory.files",
                     "controlled/empty fixture does not match case");
  }
}

}  // namespace

std::string CaseProfile(const Json::Value& input) {
  if (input["font_files"].isArray() && !input["font_files"].empty()) {
    return "explicit";
  }
  if (input["backend"] == "fontconfig" &&
      (input.isMember("fontconfig_fixture") ||
       input["font_manager_expectation"].isMember("inventory_count"))) {
    return "controlled";
  }
  return "system";
}

void AttachProbeInputs(const CaseValidationResult& input,
                       const std::filesystem::path& environment,
                       Json::Value* artifact, ValidationContext* errors) {
  (*artifact)["contract_version"] = 2;
  (*artifact)["producer"] = "skity";
  (*artifact)["input_fingerprint"] = BuildInputFingerprint(input, errors);
  if (CaseProfile(input.normalized_case) != "explicit") {
    Json::Value snapshot;
    if (ReadEnvironment(environment, &snapshot, errors)) {
      (*artifact)["input_fingerprint"]["environment_sha256"] =
          JsonFingerprint(snapshot);
    }
  }
}

void ValidateArtifactInputs(const CaseValidationResult& input,
                            const Json::Value& artifact,
                            const std::filesystem::path& artifact_path,
                            const std::filesystem::path& environment,
                            bool oracle, ValidationContext* errors) {
  Json::Value expected(Json::objectValue);
  AttachProbeInputs(input, environment, &expected, errors);
  if (artifact["input_fingerprint"] != expected["input_fingerprint"]) {
    errors->AddError("$.input_fingerprint",
                     "stale artifact: case, fonts or environment changed");
  }
  if (!oracle) {
    if (artifact["producer"] != "skity") {
      errors->AddError("$.producer", "expected a native Skity capture");
    }
    return;
  }
  if (artifact["runner"] != "skia") {
    errors->AddError("$.runner", "expected independent Skia oracle");
  }
  const auto& p = artifact["provenance"];
  if (!p.isObject() || p["version"] != 2) {
    errors->AddError("$.provenance.version",
                     "expected provenance version 2; regenerate oracle");
    return;
  }
  if (p["profile"] != CaseProfile(input.normalized_case)) {
    errors->AddError("$.provenance.profile",
                     "profile does not match case inputs");
  }
  std::string revision;
  if (RequireStringField(p, "skia_commit", "$.provenance", errors, &revision)) {
    if (revision.size() != 40 ||
        revision.find_first_not_of("0123456789abcdefABCDEF") !=
            std::string::npos) {
      errors->AddError("$.provenance.skia_commit",
                       "expected pinned Git revision");
    }
  }
  const auto& audit = p["audit"];
  for (const char* key :
       {"runner_source", "runner_binary", "skia_library", "gn_args"}) {
    CheckHash(audit, key, "$.provenance.audit", errors);
  }
  CheckAttachments(p["attachments"], artifact_path, errors);
  if (p["profile"] == "controlled") {
    CheckControlledFonts(input, artifact, errors);
  }
}

}  // namespace font_harness
}  // namespace skity
