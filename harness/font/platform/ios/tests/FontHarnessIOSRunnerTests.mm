// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#import <XCTest/XCTest.h>

#include <filesystem>

#include "harness/font/platform/ios/runner/ios_runner.hpp"

@interface FontHarnessIOSRunnerTests : XCTestCase
@end

@implementation FontHarnessIOSRunnerTests

- (void)testWritesEnvironmentArtifact {
  skity::font_harness::ios::RunConfig config = [self runConfigFromBundleOrDefault];
  skity::font_harness::ios::RunResult result =
      skity::font_harness::ios::WriteEnvironmentArtifact(config);

  XCTAssertTrue(result.ok, @"%s", result.message.c_str());
  XCTAssertFalse(result.artifact_path.empty());
}

- (std::filesystem::path)stagedRepoRoot {
  NSBundle* bundle = [NSBundle bundleForClass:[self class]];
  return std::filesystem::path(bundle.resourcePath.fileSystemRepresentation) / "repo";
}

- (std::filesystem::path)runConfigPath {
  NSBundle* bundle = [NSBundle bundleForClass:[self class]];
  return std::filesystem::path(bundle.resourcePath.fileSystemRepresentation) / "runner-config.json";
}

- (skity::font_harness::ios::RunConfig)runConfigFromBundleOrDefault {
  skity::font_harness::ios::RunConfig config = skity::font_harness::ios::DefaultRunConfig();
  const std::filesystem::path config_path = [self runConfigPath];
  if (std::filesystem::exists(config_path)) {
    std::string error;
    XCTAssertTrue(skity::font_harness::ios::LoadRunConfig(config_path, &config, &error), @"%s",
                  error.c_str());
  }
  return config;
}

- (void)testWritesConfiguredArtifacts {
  skity::font_harness::ios::RunConfig config = [self runConfigFromBundleOrDefault];
  if (config.cases.empty()) {
    XCTSkip(@"runner-config cases are required for configured artifact run");
    return;
  }
  skity::font_harness::ios::SmokeRunResult result =
      skity::font_harness::ios::WriteConfiguredArtifacts(config, [self stagedRepoRoot]);

  XCTAssertTrue(result.ok, @"%s", result.message.c_str());
  XCTAssertEqual(result.cases.size(), config.cases.size());
  for (const skity::font_harness::ios::CaseRunResult& case_result : result.cases) {
    XCTAssertTrue(case_result.ok, @"%s: %s", case_result.case_id.c_str(),
                  case_result.message.c_str());
    XCTAssertFalse(case_result.artifact_path.empty());
  }
}

@end
