// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <skity/graphic/path.hpp>
#include <string>
#include <string_view>

#include "harness/font/artifact/json_io.hpp"
#include "harness/font/case/case_document.hpp"
#include "harness/font/case/manifest_document.hpp"
#include "harness/font/case/repo_uri.hpp"
#include "harness/font/compare/compare_engine.hpp"
#include "harness/font/compare/path/path_normalizer.hpp"

namespace skity {
namespace font_harness {
namespace {

constexpr const char* kCaseId = "font.synthetic.typeface";
constexpr const char* kMetricsCaseId = "font.synthetic.metrics";
constexpr const char* kBackend = "coretext";

class TempDir {
 public:
  explicit TempDir(const std::string& name) {
    static std::atomic<int> counter{0};
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = std::filesystem::temp_directory_path() /
            ("skity_font_harness_" + name + "_" + std::to_string(stamp) + "_" +
             std::to_string(counter.fetch_add(1)));
    std::filesystem::remove_all(root_);
    std::filesystem::create_directories(root_);
  }

  ~TempDir() {
    std::error_code error;
    std::filesystem::remove_all(root_, error);
  }

  const std::filesystem::path& root() const { return root_; }

 private:
  std::filesystem::path root_;
};

void WriteTextFile(const std::filesystem::path& path,
                   std::string_view content) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary);
  ASSERT_TRUE(output.is_open()) << path;
  output << content;
  ASSERT_TRUE(output.good()) << path;
}

void WriteJson(const std::filesystem::path& path, const Json::Value& root) {
  std::string error;
  ASSERT_TRUE(WriteJsonFile(path, root, &error)) << error;
}

Json::Value MakeTypefaceCase() {
  Json::Value root(Json::objectValue);
  root["schema_version"] = 1;
  root["id"] = kCaseId;
  root["category"] = "typeface_probe";
  root["status"] = "active";
  root["backend"] = kBackend;
  root["platforms"] = Json::Value(Json::arrayValue);
  root["platforms"].append("darwin-ct");

  Json::Value font_file(Json::objectValue);
  font_file["id"] = "synthetic";
  font_file["uri"] = "repo://fonts/synthetic.ttf";
  font_file["collection_index"] = 0;
  root["font_files"] = Json::Value(Json::arrayValue);
  root["font_files"].append(std::move(font_file));

  root["typeface_request"] = Json::Value(Json::objectValue);
  root["typeface_request"]["entry"] = "MakeFromFile";
  root["typeface_request"]["font_file"] = "synthetic";

  root["glyphs"] = Json::Value(Json::objectValue);
  root["glyphs"]["chars"] = Json::Value(Json::arrayValue);
  root["glyphs"]["chars"].append("U+0041");

  root["compare"] = Json::Value(Json::objectValue);
  root["compare"]["typeface_identity"] = "normalized_descriptor";
  root["compare"]["font_style"] = "exact";
  return root;
}

Json::Value MakeManifest() {
  Json::Value root(Json::objectValue);
  root["schema_version"] = 1;
  root["id"] = "font.synthetic.smoke";
  root["backend"] = kBackend;
  root["platforms"] = Json::Value(Json::arrayValue);
  root["platforms"].append("darwin-ct");
  root["case_root"] = "cases";
  root["cases"] = Json::Value(Json::arrayValue);
  root["cases"].append("typeface.json");
  root["artifacts"] = Json::Value(Json::objectValue);
  root["artifacts"]["skia_dir"] = "artifacts/skia";
  root["artifacts"]["skity_dir"] = "artifacts/skity";
  root["artifacts"]["compare_dir"] = "artifacts/compare";
  return root;
}

Json::Value MakeMetricsCase() {
  Json::Value root = MakeTypefaceCase();
  root["id"] = kMetricsCaseId;
  root["category"] = "font_metrics";
  root["font_request"] = Json::Value(Json::objectValue);
  root["font_request"]["size"] = 16.0;
  root["compare"] = Json::Value(Json::objectValue);
  root["compare"]["typeface_identity"] = "none";
  root["compare"]["font_metrics"] = Json::Value(Json::objectValue);
  root["compare"]["font_metrics"]["mode"] = "epsilon";
  root["compare"]["font_metrics"]["epsilon"] = 0.01;
  root["compare"]["glyph_metrics"] = Json::Value(Json::objectValue);
  root["compare"]["glyph_metrics"]["mode"] = "ignore";
  return root;
}

Json::Value MakeProbeArtifact(int glyph_id) {
  Json::Value root(Json::objectValue);
  root["schema_version"] = 1;
  root["artifact_type"] = "font_probe_result";
  root["case_id"] = kCaseId;
  root["backend"] = kBackend;
  root["ok"] = true;

  Json::Value typeface(Json::objectValue);
  typeface["family_name"] = "Synthetic";
  typeface["post_script_name"] = "Synthetic-Regular";
  typeface["collection_index"] = 0;
  typeface["style"] = Json::Value(Json::objectValue);
  typeface["style"]["weight"] = 400;
  typeface["style"]["width"] = 5;
  typeface["style"]["slant"] = "upright";
  root["typeface_result"] = Json::Value(Json::objectValue);
  root["typeface_result"]["typeface"] = std::move(typeface);

  Json::Value glyph(Json::objectValue);
  glyph["label"] = "U+0041";
  glyph["code_point"] = 65;
  glyph["glyph_id"] = glyph_id;
  root["typeface_probe"] = Json::Value(Json::objectValue);
  root["typeface_probe"]["units_per_em"] = 1000;
  root["typeface_probe"]["table_count"] = 1;
  root["typeface_probe"]["glyphs"] = Json::Value(Json::arrayValue);
  root["typeface_probe"]["glyphs"].append(std::move(glyph));

  Json::Value table(Json::objectValue);
  table["tag"] = "head";
  table["tag_value"] = 1751474532;
  table["size"] = 54;
  root["typeface_probe"]["tables"] = Json::Value(Json::arrayValue);
  root["typeface_probe"]["tables"].append(std::move(table));
  return root;
}

Json::Value MakeMetricsArtifact(double ascent) {
  Json::Value root(Json::objectValue);
  root["schema_version"] = 1;
  root["artifact_type"] = "font_probe_result";
  root["case_id"] = kMetricsCaseId;
  root["backend"] = kBackend;
  root["ok"] = true;

  Json::Value font_metrics(Json::objectValue);
  font_metrics["ascent"] = ascent;
  font_metrics["descent"] = 3.0;
  font_metrics["leading"] = 1.0;

  root["metrics_probe"] = Json::Value(Json::objectValue);
  root["metrics_probe"]["font_result"] = Json::Value(Json::objectValue);
  root["metrics_probe"]["font_result"]["font_metrics"] = font_metrics;
  root["metrics_probe"]["scaler_context_result"] =
      Json::Value(Json::objectValue);
  root["metrics_probe"]["scaler_context_result"]["font_metrics"] =
      std::move(font_metrics);
  return root;
}

TEST(FontHarnessCaseDocumentTest, ValidatesSyntheticTypefaceCase) {
  TempDir temp("case_valid");
  WriteTextFile(temp.root() / "fonts/synthetic.ttf", "synthetic font bytes");

  RepoUriResolver resolver(temp.root());
  CaseValidationResult result =
      ValidateCaseDocument(MakeTypefaceCase(), resolver);

  EXPECT_TRUE(result.valid) << WriteJsonString(result.errors.ToJson());
  EXPECT_EQ(kCaseId, result.case_id);
  EXPECT_EQ(kBackend, result.backend);
  ASSERT_EQ(1u, result.resolved_font_files.size());
  EXPECT_EQ("synthetic", result.resolved_font_files[0].id);
  EXPECT_EQ(
      std::filesystem::weakly_canonical(temp.root() / "fonts/synthetic.ttf"),
      result.resolved_font_files[0].absolute_path);
}

TEST(FontHarnessCaseDocumentTest, RejectsRepoUriTraversal) {
  TempDir temp("case_traversal");
  WriteTextFile(temp.root() / "fonts/synthetic.ttf", "synthetic font bytes");

  Json::Value root = MakeTypefaceCase();
  root["font_files"][0]["uri"] = "repo://../outside.ttf";

  RepoUriResolver resolver(temp.root());
  CaseValidationResult result = ValidateCaseDocument(root, resolver);

  EXPECT_FALSE(result.valid);
  EXPECT_FALSE(result.errors.ToJson().empty());
}

TEST(FontHarnessManifestDocumentTest, ValidatesSyntheticManifest) {
  TempDir temp("manifest_valid");

  RepoUriResolver resolver(temp.root());
  ManifestValidationResult result =
      ValidateManifestDocument(MakeManifest(), resolver);

  EXPECT_TRUE(result.valid) << WriteJsonString(result.errors.ToJson());
  EXPECT_EQ("font.synthetic.smoke", result.manifest_id);
  EXPECT_EQ(kBackend, result.backend);
}

TEST(FontHarnessManifestDocumentTest, RejectsUnsafeCasePath) {
  TempDir temp("manifest_unsafe_case");
  Json::Value root = MakeManifest();
  root["cases"][0] = "../typeface.json";

  RepoUriResolver resolver(temp.root());
  ManifestValidationResult result = ValidateManifestDocument(root, resolver);

  EXPECT_FALSE(result.valid);
  EXPECT_FALSE(result.errors.ToJson().empty());
}

TEST(FontHarnessCompareEngineTest, ReportsPassMismatchAndInputFailure) {
  TempDir temp("compare");
  WriteTextFile(temp.root() / "fonts/synthetic.ttf", "synthetic font bytes");

  const std::filesystem::path case_path = temp.root() / "cases/typeface.json";
  const std::filesystem::path expected_path =
      temp.root() / "artifacts/expected.json";
  const std::filesystem::path actual_path =
      temp.root() / "artifacts/actual.json";
  WriteJson(case_path, MakeTypefaceCase());
  WriteJson(expected_path, MakeProbeArtifact(1));
  WriteJson(actual_path, MakeProbeArtifact(1));

  CompareRequest request;
  request.repo_root = temp.root();
  request.case_path = case_path;
  request.expected_path = expected_path;
  request.actual_path = actual_path;
  request.backend = kBackend;

  CompareResult pass = RunCompare(request);
  EXPECT_EQ(CompareStatus::kPass, pass.status);
  EXPECT_TRUE(pass.report["passed"].asBool());
  EXPECT_EQ("pass", pass.report["reason_code"].asString());

  WriteJson(actual_path, MakeProbeArtifact(2));
  CompareResult mismatch = RunCompare(request);
  EXPECT_EQ(CompareStatus::kMismatch, mismatch.status);
  EXPECT_FALSE(mismatch.report["passed"].asBool());
  EXPECT_EQ("glyph_id_mismatch", mismatch.report["reason_code"].asString());
  EXPECT_EQ("typeface_probe.glyphs[0].glyph_id",
            mismatch.report["diff_path"].asString());

  CompareRequest missing = request;
  missing.expected_path = temp.root() / "artifacts/missing.skia.json";
  CompareResult input_failure = RunCompare(missing);
  EXPECT_EQ(CompareStatus::kInputFailed, input_failure.status);
  EXPECT_EQ("expected_load", input_failure.report["stage"].asString());
  EXPECT_EQ("oracle_unavailable",
            input_failure.report["reason_code"].asString());
}

TEST(FontHarnessCompareEngineTest, ComparesMetricsWithEpsilon) {
  TempDir temp("metrics_compare");
  WriteTextFile(temp.root() / "fonts/synthetic.ttf", "synthetic font bytes");

  const std::filesystem::path case_path = temp.root() / "cases/metrics.json";
  const std::filesystem::path expected_path =
      temp.root() / "artifacts/expected.json";
  const std::filesystem::path actual_path =
      temp.root() / "artifacts/actual.json";
  WriteJson(case_path, MakeMetricsCase());
  WriteJson(expected_path, MakeMetricsArtifact(10.0));
  WriteJson(actual_path, MakeMetricsArtifact(10.005));

  CompareRequest request;
  request.repo_root = temp.root();
  request.case_path = case_path;
  request.expected_path = expected_path;
  request.actual_path = actual_path;
  request.backend = kBackend;

  CompareResult pass = RunCompare(request);
  EXPECT_EQ(CompareStatus::kPass, pass.status);
  EXPECT_TRUE(pass.report["passed"].asBool());

  WriteJson(actual_path, MakeMetricsArtifact(10.5));
  CompareResult mismatch = RunCompare(request);
  EXPECT_EQ(CompareStatus::kMismatch, mismatch.status);
  EXPECT_FALSE(mismatch.report["passed"].asBool());
  EXPECT_EQ("metrics_mismatch", mismatch.report["reason_code"].asString());
  EXPECT_EQ("metrics_probe.font_result.font_metrics.ascent",
            mismatch.report["diff_path"].asString());
}

TEST(FontHarnessPathNormalizerTest, DropsZeroLengthSegmentsAndRoundsPoints) {
  skity::Path path;
  path.MoveTo(0.00004f, -0.00004f);
  path.LineTo(0.00004f, -0.00004f);
  path.LineTo(1.00004f, 0.00004f);
  path.LineTo(1.00004f, 1.00004f);
  path.Close();

  PathNormalizeOptions options;
  options.epsilon = 0.001;
  Json::Value normalized = BuildNormalizedPathJson(path, options);

  EXPECT_EQ("path_normalized", normalized["mode"].asString());
  EXPECT_EQ(
      1, normalized["normalization"]["dropped_zero_length_segments"].asInt());
  EXPECT_EQ(4, normalized["verb_count"].asInt());
  ASSERT_EQ(4u, normalized["verb_sequence"].size());
  EXPECT_EQ("move", normalized["verb_sequence"][0].asString());
  EXPECT_EQ("line", normalized["verb_sequence"][1].asString());
  EXPECT_EQ("line", normalized["verb_sequence"][2].asString());
  EXPECT_EQ("close", normalized["verb_sequence"][3].asString());

  ASSERT_EQ(1, normalized["contour_count"].asInt());
  EXPECT_TRUE(normalized["contours"][0]["closed"].asBool());
  EXPECT_DOUBLE_EQ(0.0, normalized["verbs"][0]["points"][0]["x"].asDouble());
  EXPECT_DOUBLE_EQ(0.0, normalized["verbs"][0]["points"][0]["y"].asDouble());
}

}  // namespace
}  // namespace font_harness
}  // namespace skity
