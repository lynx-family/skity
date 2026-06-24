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
#include "harness/font/case/platform_target.hpp"
#include "harness/font/case/repo_uri.hpp"
#include "harness/font/compare/compare_engine.hpp"
#include "harness/font/compare/path/path_normalizer.hpp"
#if defined(_WIN32) && SKITY_FONT_HARNESS_HAS_DIRECTWRITE
#include "harness/font/platform/directwrite/env_info.hpp"
#endif

namespace skity {
namespace font_harness {
namespace {

constexpr const char* kCaseId = "font.synthetic.typeface";
constexpr const char* kMetricsCaseId = "font.synthetic.metrics";
constexpr const char* kGlyphPathCaseId = "font.synthetic.glyph_path";
constexpr const char* kGlyphImageCaseId = "font.synthetic.glyph_image";
constexpr const char* kFontManagerCaseId = "font.synthetic.font_manager";
constexpr const char* kBackend = "coretext";
constexpr const char* kTargetPlatform = "macos-coretext";

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
  root["platforms"].append(kTargetPlatform);
  root["target_platform"] = kTargetPlatform;
  root["case_root"] = "cases";
  root["cases"] = Json::Value(Json::arrayValue);
  root["cases"].append("typeface.json");
  root["artifacts"] = Json::Value(Json::objectValue);
  root["artifacts"]["root"] = "artifacts/macos-coretext";
  root["artifacts"]["report_root"] = "reports/macos-coretext";
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

Json::Value MakeFontManagerCase() {
  Json::Value root(Json::objectValue);
  root["schema_version"] = 1;
  root["id"] = kFontManagerCaseId;
  root["category"] = "font_manager";
  root["status"] = "active";
  root["backend"] = kBackend;
  root["platforms"] = Json::Value(Json::arrayValue);
  root["platforms"].append("darwin-ct");

  root["font_manager_request"] = Json::Value(Json::objectValue);
  root["font_manager_request"]["entry"] = "MatchFamilyStyle";
  root["font_manager_request"]["family_name"] = "Synthetic";
  root["font_manager_request"]["style"] = Json::Value(Json::objectValue);
  root["font_manager_request"]["style"]["weight"] = 400;
  root["font_manager_request"]["style"]["width"] = 5;
  root["font_manager_request"]["style"]["slant"] = "upright";

  root["glyphs"] = Json::Value(Json::objectValue);
  root["glyphs"]["chars"] = Json::Value(Json::arrayValue);
  root["glyphs"]["chars"].append("U+0041");

  root["compare"] = Json::Value(Json::objectValue);
  root["compare"]["typeface_identity"] = "normalized_descriptor";
  root["compare"]["font_style"] = "exact";
  root["compare"]["font_metrics"] = Json::Value(Json::objectValue);
  root["compare"]["font_metrics"]["mode"] = "epsilon";
  root["compare"]["font_metrics"]["epsilon"] = 0.001;
  return root;
}

Json::Value MakeGlyphPathCase() {
  Json::Value root = MakeTypefaceCase();
  root["id"] = kGlyphPathCaseId;
  root["category"] = "glyph_path";
  root["font_request"] = Json::Value(Json::objectValue);
  root["font_request"]["size"] = 16.0;
  root["compare"] = Json::Value(Json::objectValue);
  root["compare"]["typeface_identity"] = "none";
  root["compare"]["glyph_path"] = Json::Value(Json::objectValue);
  root["compare"]["glyph_path"]["mode"] = "path_normalized";
  root["compare"]["glyph_path"]["epsilon"] = 0.001;
  return root;
}

Json::Value MakeGlyphImageCase() {
  Json::Value root = MakeGlyphPathCase();
  root["id"] = kGlyphImageCaseId;
  root["category"] = "glyph_image";
  root["image_request"] = Json::Value(Json::objectValue);
  root["image_request"]["context_scale"] = 1.0;
  root["paint_request"] = Json::Value(Json::objectValue);
  root["paint_request"]["color"] = "#000000FF";
  root["compare"] = Json::Value(Json::objectValue);
  root["compare"]["glyph_image"] = Json::Value(Json::objectValue);
  root["compare"]["glyph_image"]["mode"] = "metadata";
  return root;
}

Json::Value MakeMetricSummary(double ascent) {
  Json::Value metrics(Json::objectValue);
  metrics["ascent"] = ascent;
  metrics["descent"] = 3.0;
  metrics["leading"] = 1.0;
  return metrics;
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

Json::Value MakeFontManagerArtifact(bool available,
                                    const std::string& post_script_name,
                                    int glyph_id, double font_ascent,
                                    double scaler_ascent) {
  Json::Value root(Json::objectValue);
  root["schema_version"] = 1;
  root["artifact_type"] = "font_probe_result";
  root["case_id"] = kFontManagerCaseId;
  root["backend"] = kBackend;
  root["ok"] = true;

  root["font_manager_probe"] = Json::Value(Json::objectValue);
  root["font_manager_probe"]["operation"] = Json::Value(Json::objectValue);
  root["font_manager_probe"]["operation"]["entry"] = "MatchFamilyStyle";
  root["font_manager_probe"]["matched_typefaces"] =
      Json::Value(Json::arrayValue);

  Json::Value typeface(Json::objectValue);
  typeface["available"] = available;
  if (available) {
    typeface["descriptor"] = Json::Value(Json::objectValue);
    typeface["descriptor"]["family_name"] = "Synthetic";
    typeface["descriptor"]["post_script_name"] = post_script_name;
    typeface["descriptor"]["collection_index"] = 0;
    typeface["descriptor"]["style"] = Json::Value(Json::objectValue);
    typeface["descriptor"]["style"]["weight"] = 400;
    typeface["descriptor"]["style"]["width"] = 5;
    typeface["descriptor"]["style"]["slant"] = "upright";

    typeface["font_style"] = Json::Value(Json::objectValue);
    typeface["font_style"]["weight"] = 400;
    typeface["font_style"]["width"] = 5;
    typeface["font_style"]["slant"] = "upright";
    typeface["units_per_em"] = 1000;
  }

  typeface["probe_summary"] = Json::Value(Json::objectValue);
  typeface["probe_summary"]["font_result"] = Json::Value(Json::objectValue);
  typeface["probe_summary"]["font_result"]["font_metrics"] =
      MakeMetricSummary(font_ascent);
  typeface["probe_summary"]["scaler_context_result"] =
      Json::Value(Json::objectValue);
  typeface["probe_summary"]["scaler_context_result"]["font_metrics"] =
      MakeMetricSummary(scaler_ascent);
  typeface["probe_summary"]["glyphs"] = Json::Value(Json::arrayValue);

  Json::Value glyph(Json::objectValue);
  glyph["label"] = "U+0041";
  glyph["code_point"] = 65;
  glyph["glyph_id"] = glyph_id;
  typeface["probe_summary"]["glyphs"].append(std::move(glyph));

  root["font_manager_probe"]["matched_typefaces"].append(std::move(typeface));
  return root;
}

void AddMatchedTypefaceTables(Json::Value* root, int head_table_size) {
  Json::Value table(Json::objectValue);
  table["tag"] = "head";
  table["tag_value"] = 1751474532;
  table["size"] = head_table_size;

  Json::Value tables(Json::arrayValue);
  tables.append(std::move(table));

  Json::Value& typeface = (*root)["font_manager_probe"]["matched_typefaces"][0];
  typeface["table_count"] = 1;
  typeface["copied_table_tag_count"] = 1;
  typeface["tables"] = std::move(tables);
}

void RunFontManagerCompare(TempDir* temp, const Json::Value& expected,
                           const Json::Value& actual, CompareResult* result) {
  const std::filesystem::path case_path =
      temp->root() / "cases/font_manager.json";
  const std::filesystem::path expected_path =
      temp->root() / "artifacts/expected.json";
  const std::filesystem::path actual_path =
      temp->root() / "artifacts/actual.json";
  WriteJson(case_path, MakeFontManagerCase());
  WriteJson(expected_path, expected);
  WriteJson(actual_path, actual);

  CompareRequest request;
  request.repo_root = temp->root();
  request.case_path = case_path;
  request.expected_path = expected_path;
  request.actual_path = actual_path;
  request.backend = kBackend;
  *result = RunCompare(request);
}

Json::Value MakeGlyphImageArtifact(double origin_y) {
  Json::Value root(Json::objectValue);
  root["schema_version"] = 1;
  root["artifact_type"] = "font_probe_result";
  root["case_id"] = kGlyphImageCaseId;
  root["backend"] = kBackend;
  root["ok"] = true;

  Json::Value image(Json::objectValue);
  image["origin_x"] = 1.0;
  image["origin_y"] = origin_y;
  image["origin_x_for_raster"] = 1.0;
  image["origin_y_for_raster"] = -46.0;
  image["width"] = 32.0;
  image["height"] = 48.0;
  image["format"] = "A8";
  image["has_buffer"] = true;
  image["byte_size"] = 1536;

  Json::Value glyph(Json::objectValue);
  glyph["label"] = "U+0041";
  glyph["code_point"] = 65;
  glyph["glyph_id"] = 1;
  glyph["image"] = std::move(image);

  root["glyph_image_probe"] = Json::Value(Json::objectValue);
  root["glyph_image_probe"]["font_result"] = Json::Value(Json::objectValue);
  root["glyph_image_probe"]["font_result"]["glyph_images"] =
      Json::Value(Json::arrayValue);
  root["glyph_image_probe"]["font_result"]["glyph_images"].append(
      std::move(glyph));
  return root;
}

void RunGlyphImageCompare(TempDir* temp, const Json::Value& expected,
                          const Json::Value& actual, CompareResult* result) {
  WriteTextFile(temp->root() / "fonts/synthetic.ttf", "synthetic font bytes");
  const std::filesystem::path case_path =
      temp->root() / "cases/glyph_image.json";
  const std::filesystem::path expected_path =
      temp->root() / "artifacts/expected.json";
  const std::filesystem::path actual_path =
      temp->root() / "artifacts/actual.json";
  WriteJson(case_path, MakeGlyphImageCase());
  WriteJson(expected_path, expected);
  WriteJson(actual_path, actual);

  CompareRequest request;
  request.repo_root = temp->root();
  request.case_path = case_path;
  request.expected_path = expected_path;
  request.actual_path = actual_path;
  request.backend = kBackend;
  *result = RunCompare(request);
}

void ExpectSingleDiff(const CompareResult& result,
                      const std::string& reason_code,
                      const std::string& diff_path) {
  EXPECT_EQ(CompareStatus::kMismatch, result.status)
      << WriteJsonString(result.report);
  EXPECT_FALSE(result.report["passed"].asBool());
  EXPECT_EQ(reason_code, result.report["reason_code"].asString());
  EXPECT_EQ(1, result.report["diff_count"].asInt());
  EXPECT_EQ(diff_path, result.report["diff_path"].asString());
  ASSERT_TRUE(result.report["diffs"].isArray());
  ASSERT_EQ(1u, result.report["diffs"].size());
  EXPECT_EQ(reason_code, result.report["diffs"][0]["reason_code"].asString());
  EXPECT_EQ(diff_path, result.report["diffs"][0]["path"].asString());
}

Json::Value MakeSyntheticPath() {
  Json::Value path(Json::objectValue);
  path["mode"] = "path_normalized";
  path["epsilon"] = 0.001;
  path["empty"] = false;
  path["finite"] = true;
  path["verb_count"] = 2;
  path["point_count"] = 2;
  path["verb_sequence"] = Json::Value(Json::arrayValue);
  path["verb_sequence"].append("move");
  path["verb_sequence"].append("line");

  Json::Value move(Json::objectValue);
  move["index"] = 0;
  move["verb"] = "move";
  move["point_count"] = 1;
  move["points"] = Json::Value(Json::arrayValue);
  Json::Value move_point(Json::objectValue);
  move_point["x"] = 0.0;
  move_point["y"] = 0.0;
  move["points"].append(std::move(move_point));

  Json::Value line(Json::objectValue);
  line["index"] = 1;
  line["verb"] = "line";
  line["point_count"] = 2;
  line["points"] = Json::Value(Json::arrayValue);
  Json::Value line_start(Json::objectValue);
  line_start["x"] = 0.0;
  line_start["y"] = 0.0;
  line["points"].append(std::move(line_start));
  Json::Value line_end(Json::objectValue);
  line_end["x"] = 1.0;
  line_end["y"] = 1.0;
  line["points"].append(std::move(line_end));

  path["verbs"] = Json::Value(Json::arrayValue);
  path["verbs"].append(std::move(move));
  path["verbs"].append(std::move(line));
  return path;
}

Json::Value MakePathArtifact(bool use_normalized_path_field,
                             bool scaler_unavailable) {
  Json::Value root(Json::objectValue);
  root["schema_version"] = 1;
  root["artifact_type"] = "font_probe_result";
  root["case_id"] = kGlyphPathCaseId;
  root["backend"] = kBackend;
  root["ok"] = true;

  Json::Value glyph(Json::objectValue);
  glyph["label"] = "U+0041";
  glyph["code_point"] = 65;
  glyph["glyph_id"] = 1;
  glyph[use_normalized_path_field ? "normalized_path" : "path"] =
      MakeSyntheticPath();

  root["glyph_path_probe"] = Json::Value(Json::objectValue);
  root["glyph_path_probe"]["font_result"] = Json::Value(Json::objectValue);
  root["glyph_path_probe"]["font_result"]["glyph_paths"] =
      Json::Value(Json::arrayValue);
  root["glyph_path_probe"]["font_result"]["glyph_paths"].append(glyph);

  root["glyph_path_probe"]["scaler_context_result"] =
      Json::Value(Json::objectValue);
  if (scaler_unavailable) {
    root["glyph_path_probe"]["scaler_context_result"]["available"] = false;
    root["glyph_path_probe"]["scaler_context_result"]["reason"] =
        "synthetic unavailable";
  } else {
    root["glyph_path_probe"]["scaler_context_result"]["glyph_paths"] =
        Json::Value(Json::arrayValue);
    root["glyph_path_probe"]["scaler_context_result"]["glyph_paths"].append(
        std::move(glyph));
  }
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

TEST(FontHarnessCompareEngineTest,
     ComparesPathArtifactsWithLegacyPathFieldAndUnavailableScalerOracle) {
  TempDir temp("glyph_path_compare");
  WriteTextFile(temp.root() / "fonts/synthetic.ttf", "synthetic font bytes");

  const std::filesystem::path case_path = temp.root() / "cases/glyph_path.json";
  const std::filesystem::path expected_path =
      temp.root() / "artifacts/expected.json";
  const std::filesystem::path actual_path =
      temp.root() / "artifacts/actual.json";
  WriteJson(case_path, MakeGlyphPathCase());
  WriteJson(expected_path, MakePathArtifact(/*use_normalized_path_field=*/false,
                                            /*scaler_unavailable=*/true));
  WriteJson(actual_path, MakePathArtifact(/*use_normalized_path_field=*/true,
                                          /*scaler_unavailable=*/false));

  CompareRequest request;
  request.repo_root = temp.root();
  request.case_path = case_path;
  request.expected_path = expected_path;
  request.actual_path = actual_path;
  request.backend = kBackend;

  CompareResult pass = RunCompare(request);
  EXPECT_EQ(CompareStatus::kPass, pass.status) << WriteJsonString(pass.report);
  EXPECT_TRUE(pass.report["passed"].asBool());
}

TEST(FontHarnessCompareEngineTest,
     ShortCircuitsFontManagerCompareWhenAvailabilityDiffers) {
  TempDir temp("font_manager_availability");
  CompareResult result;
  RunFontManagerCompare(
      &temp,
      MakeFontManagerArtifact(/*available=*/false, "Synthetic-Regular",
                              /*glyph_id=*/1, /*font_ascent=*/10.0,
                              /*scaler_ascent=*/10.0),
      MakeFontManagerArtifact(/*available=*/true, "Synthetic-Regular",
                              /*glyph_id=*/2, /*font_ascent=*/12.0,
                              /*scaler_ascent=*/12.0),
      &result);

  ExpectSingleDiff(result, "selection_mismatch",
                   "font_manager_probe.matched_typefaces[0].available");
}

TEST(FontHarnessCompareEngineTest,
     ShortCircuitsFontManagerCompareWhenIdentityDiffers) {
  TempDir temp("font_manager_identity");
  CompareResult result;
  RunFontManagerCompare(
      &temp,
      MakeFontManagerArtifact(/*available=*/true, "Synthetic-Regular",
                              /*glyph_id=*/1, /*font_ascent=*/10.0,
                              /*scaler_ascent=*/10.0),
      MakeFontManagerArtifact(/*available=*/true, "Synthetic-Bold",
                              /*glyph_id=*/2, /*font_ascent=*/12.0,
                              /*scaler_ascent=*/12.0),
      &result);

  ExpectSingleDiff(
      result, "selection_mismatch",
      "font_manager_probe.matched_typefaces[0].identity.post_script_name");
}

TEST(FontHarnessCompareEngineTest,
     ComparesFontManagerMetricsAfterSelectionMatches) {
  TempDir temp("font_manager_metrics");
  CompareResult result;
  RunFontManagerCompare(
      &temp,
      MakeFontManagerArtifact(/*available=*/true, "Synthetic-Regular",
                              /*glyph_id=*/1, /*font_ascent=*/10.0,
                              /*scaler_ascent=*/10.0),
      MakeFontManagerArtifact(/*available=*/true, "Synthetic-Regular",
                              /*glyph_id=*/1, /*font_ascent=*/12.0,
                              /*scaler_ascent=*/10.0),
      &result);

  ExpectSingleDiff(
      result, "metrics_mismatch",
      "font_manager_probe.matched_typefaces[0].probe_summary.font_metrics."
      "ascent");
}

TEST(FontHarnessCompareEngineTest,
     IgnoresFontManagerMatchedTablesWhenOracleOmitsTables) {
  TempDir temp("font_manager_legacy_tables");
  CompareResult result;
  Json::Value expected = MakeFontManagerArtifact(
      /*available=*/true, "Synthetic-Regular", /*glyph_id=*/1,
      /*font_ascent=*/10.0, /*scaler_ascent=*/10.0);
  Json::Value actual = expected;
  AddMatchedTypefaceTables(&actual, /*head_table_size=*/54);

  RunFontManagerCompare(&temp, expected, actual, &result);

  EXPECT_EQ(CompareStatus::kPass, result.status)
      << WriteJsonString(result.report);
  EXPECT_TRUE(result.report["passed"].asBool());
}

TEST(FontHarnessCompareEngineTest,
     ComparesFontManagerMatchedTablesWhenOracleDeclaresTables) {
  TempDir temp("font_manager_declared_tables");
  CompareResult result;
  Json::Value expected = MakeFontManagerArtifact(
      /*available=*/true, "Synthetic-Regular", /*glyph_id=*/1,
      /*font_ascent=*/10.0, /*scaler_ascent=*/10.0);
  Json::Value actual = expected;
  AddMatchedTypefaceTables(&expected, /*head_table_size=*/54);
  AddMatchedTypefaceTables(&actual, /*head_table_size=*/56);

  RunFontManagerCompare(&temp, expected, actual, &result);

  ExpectSingleDiff(result, "table_mismatch",
                   "font_manager_probe.matched_typefaces[0].tables[0].size");
}

TEST(FontHarnessCompareEngineTest,
     ComparesGlyphImageOriginYInSkiaMaskTopSpace) {
  TempDir temp("glyph_image_origin_y_mask_top");
  CompareResult result;
  RunGlyphImageCompare(&temp, MakeGlyphImageArtifact(-46.0),
                       MakeGlyphImageArtifact(46.0), &result);

  EXPECT_EQ(CompareStatus::kPass, result.status)
      << WriteJsonString(result.report);
  EXPECT_TRUE(result.report["passed"].asBool());
}

TEST(FontHarnessCompareEngineTest, ReportsGlyphImageOriginYWhenSignMatches) {
  TempDir temp("glyph_image_origin_y_same_sign");
  CompareResult result;
  RunGlyphImageCompare(&temp, MakeGlyphImageArtifact(-46.0),
                       MakeGlyphImageArtifact(-46.0), &result);

  ExpectSingleDiff(
      result, "image_mismatch",
      "glyph_image_probe.font_result.glyph_images[0].image.origin_y");
}

TEST(FontHarnessCompareEngineTest,
     ReportsGlyphImageOriginYWhenMagnitudeDiffers) {
  TempDir temp("glyph_image_origin_y_magnitude");
  CompareResult result;
  RunGlyphImageCompare(&temp, MakeGlyphImageArtifact(-46.0),
                       MakeGlyphImageArtifact(47.0), &result);

  ExpectSingleDiff(
      result, "image_mismatch",
      "glyph_image_probe.font_result.glyph_images[0].image.origin_y");
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

TEST(FontHarnessCaseDocumentTest, AcceptsIosSimulatorCoreTextPlatform) {
  TempDir temp("case_ios_sim_coretext");
  WriteTextFile(temp.root() / "fonts/synthetic.ttf", "synthetic font bytes");

  Json::Value root = MakeTypefaceCase();
  root["platforms"] = Json::Value(Json::arrayValue);
  root["platforms"].append("ios-sim-coretext");

  RepoUriResolver resolver(temp.root());
  CaseValidationResult result = ValidateCaseDocument(root, resolver);

  EXPECT_TRUE(result.valid) << WriteJsonString(result.errors.ToJson());
}

TEST(FontHarnessCaseDocumentTest, AcceptsReservedFuturePlatformTargets) {
  TempDir temp("case_future_platforms");
  WriteTextFile(temp.root() / "fonts/synthetic.ttf", "synthetic font bytes");

  RepoUriResolver resolver(temp.root());

  Json::Value ios_device = MakeTypefaceCase();
  ios_device["platforms"] = Json::Value(Json::arrayValue);
  ios_device["platforms"].append("ios-device-coretext");
  CaseValidationResult ios_device_result =
      ValidateCaseDocument(ios_device, resolver);
  EXPECT_TRUE(ios_device_result.valid)
      << WriteJsonString(ios_device_result.errors.ToJson());

  Json::Value android = MakeTypefaceCase();
  android["backend"] = "freetype";
  android["platforms"] = Json::Value(Json::arrayValue);
  android["platforms"].append("android-freetype");
  CaseValidationResult android_result = ValidateCaseDocument(android, resolver);
  EXPECT_TRUE(android_result.valid)
      << WriteJsonString(android_result.errors.ToJson());

  Json::Value windows = MakeTypefaceCase();
  windows["backend"] = "directwrite";
  windows["platforms"] = Json::Value(Json::arrayValue);
  windows["platforms"].append("windows-directwrite");
  CaseValidationResult windows_result = ValidateCaseDocument(windows, resolver);
  EXPECT_TRUE(windows_result.valid)
      << WriteJsonString(windows_result.errors.ToJson());
}

TEST(FontHarnessCaseDocumentTest, AllowsGlyphImageFontManagerSource) {
  RepoUriResolver resolver(".");
  Json::Value root = MakeGlyphImageCase();
  root.removeMember("font_files");
  root.removeMember("typeface_request");
  root["font_manager_request"] = MakeFontManagerCase()["font_manager_request"];

  CaseValidationResult result = ValidateCaseDocument(root, resolver);
  EXPECT_TRUE(result.valid) << WriteJsonString(result.errors.ToJson());
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
  EXPECT_EQ(kTargetPlatform, result.target_platform);
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

TEST(FontHarnessManifestDocumentTest, RejectsTargetPlatformOutsidePlatforms) {
  TempDir temp("manifest_target_platform");
  Json::Value root = MakeManifest();
  root["target_platform"] = "ios-sim-coretext";

  RepoUriResolver resolver(temp.root());
  ManifestValidationResult result = ValidateManifestDocument(root, resolver);

  EXPECT_FALSE(result.valid);
  EXPECT_FALSE(result.errors.ToJson().empty());
}

TEST(FontHarnessManifestDocumentTest, AcceptsReservedFuturePlatformTargets) {
  TempDir temp("manifest_future_platforms");
  RepoUriResolver resolver(temp.root());

  Json::Value android = MakeManifest();
  android["backend"] = "freetype";
  android["platforms"] = Json::Value(Json::arrayValue);
  android["platforms"].append("android-freetype");
  android["target_platform"] = "android-freetype";
  ManifestValidationResult android_result =
      ValidateManifestDocument(android, resolver);
  EXPECT_TRUE(android_result.valid)
      << WriteJsonString(android_result.errors.ToJson());
  EXPECT_EQ("android-freetype", android_result.target_platform);

  Json::Value windows = MakeManifest();
  windows["backend"] = "directwrite";
  windows["platforms"] = Json::Value(Json::arrayValue);
  windows["platforms"].append("windows-directwrite");
  windows["target_platform"] = "windows-directwrite";
  ManifestValidationResult windows_result =
      ValidateManifestDocument(windows, resolver);
  EXPECT_TRUE(windows_result.valid)
      << WriteJsonString(windows_result.errors.ToJson());
  EXPECT_EQ("windows-directwrite", windows_result.target_platform);
}

TEST(FontHarnessPlatformTargetTest, CanonicalizesLegacyAliases) {
  EXPECT_EQ("macos-coretext", CanonicalPlatformTarget("darwin-ct"));
  EXPECT_EQ("windows-directwrite", CanonicalPlatformTarget("windows-dwrite"));

  Json::Value platforms(Json::arrayValue);
  platforms.append("darwin-ct");
  EXPECT_TRUE(PlatformArrayContainsTarget(platforms, "macos-coretext"));
}

#if defined(_WIN32) && SKITY_FONT_HARNESS_HAS_DIRECTWRITE
TEST(FontHarnessDirectWriteEnvInfoTest, ReportsWindowsLocaleState) {
  DirectWriteEnvRequest request;
  request.repo_root = std::filesystem::current_path();

  Json::Value report = BuildDirectWriteEnvInfo(request);
  if (!report.get("backend_available", false).asBool()) {
    GTEST_SKIP() << report.get("message", "DirectWrite unavailable").asString();
  }

  ASSERT_TRUE(report.isMember("windows_locale"));
  const Json::Value& locale = report["windows_locale"];
  EXPECT_TRUE(locale.isMember("user_default_locale"));
  EXPECT_TRUE(locale.isMember("system_default_locale"));
  ASSERT_TRUE(locale["user_preferred_ui_languages"].isArray());
  ASSERT_TRUE(locale["system_preferred_ui_languages"].isArray());
  ASSERT_TRUE(locale["installed_ui_languages"].isArray());
}
#endif

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
