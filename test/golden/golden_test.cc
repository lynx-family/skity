// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <iostream>

#include "common/golden_test_env.hpp"

namespace {

// Add shared cases as they are validated on the software backend.
constexpr char kSoftwareGoldenFilter[] =
    "BlendModeGolden.Screen:"
    "ColorSpaceGolden.DisplayP3TargetDecodeBaseline:"
    "ColorSpaceGolden.P3TargetSpaceImagePassthrough:"
    "ColorSpaceGolden.P3TargetSpacePaintColor:"
    "ColorSpaceGolden.P3TargetSpaceResolvesSRGBPaintColor:"
    "ColorSpaceGolden.LinearTargetFoldsEncode:"
    "ColorSpaceGolden.Gamma22TargetParametricEncode:"
    "ColorSpaceGolden.P3TargetGradientStopsResolve:"
    "ColorSpaceGolden.P3TargetAlphaCompositingAndReadbackRoundTrip:"
    "ColorSpaceGolden.P3DeclaredStopsPassThroughOnP3Target:"
    "ColorSpaceGolden.AdobeRGBTargetPassthroughAndPaint:"
    "ColorSpaceGolden.AdobeRGBTargetResolvesSRGBImage:"
    "ShapeGolden.DrawEmptyPath:ShapeGolden.DrawDegenerateCubic:"
    "ShapeGolden.StrokeMiterLimit:ShapeGolden.WindingContours:"
    "ShapeGolden.EvenOddContours:SimpleShapeGolden.DrawFilledRect:"
    "SimpleShapeGolden.DrawStrokeRect:"
    "ClipGolden.ClipPathDifference:ClipGolden.ClipRRect:"
    "GradientGolden.LinearGradientFallbackTileMode:"
    "GradientGolden.RadialGradientFallbackTileMode:"
    "GradientGolden.LinearGradientTileMode:GradientGolden.RadialGradient:"
    "GradientGolden.RadialGradient2:GradientGolden.TwoPointConicalGradient_*:"
    "ImageGolden.ShaderTileModesLinear:"
    "ImageGolden.ShaderTileModesNearest:"
    "ImageGolden.NearestPaintColorFilter:"
    "InitialColorClearGolden.RetainedFrames:"
    "RecorderGolden.DisplayListCullRectClipDrawPaint:"
    "RecorderGolden.DisplayListCullRectClipRestoreToCount:"
    "RecorderGolden.DisplayListSaveLayerSetResetMatrix:"
    "SaveLayerGolden.ResetMatrixDrawColorFillsLayer:"
    "SoftwareTextGolden.PositionedGlyphs:SoftwareTextGolden.ClipRect";

}  // namespace

#ifdef SKITY_GOLDEN_GUI
namespace skity {
namespace testing {

::testing::Environment* CreateDiffEnv();

}
}  // namespace skity

#endif

int main(int argc, char* argv[]) {
  using skity::testing::Backend;
  Backend backend = Backend::kMetal;

  for (int i = 1; i + 1 < argc; ++i) {
    if (strcmp(argv[i], "--backend") != 0) {
      continue;
    }
    const char* name = argv[i + 1];
    if (strcmp(name, "gl") == 0) {
      backend = Backend::kGL;
    } else if (strcmp(name, "vulkan") == 0) {
      backend = Backend::kVulkan;
    } else if (strcmp(name, "metal") == 0) {
      backend = Backend::kMetal;
    } else if (strcmp(name, "software") == 0) {
      backend = Backend::kSoftware;
    }
    break;
  }

  const bool software = backend == Backend::kSoftware;
  if (software && std::getenv("GTEST_FILTER") == nullptr) {
    // InitGoogleTest can still override the default with --gtest_filter.
    GTEST_FLAG_SET(filter, kSoftwareGoldenFilter);
  }
  testing::InitGoogleTest(&argc, argv);

  auto* env = skity::testing::GoldenTestEnv::CreateInstance(backend);
  if (env == nullptr) {
    std::cerr << "Failed to create golden test environment. Check that the "
                 "requested backend is enabled in CMake."
              << std::endl;
    return EXIT_FAILURE;
  }

  testing::AddGlobalTestEnvironment(env);

#ifdef SKITY_GOLDEN_GUI
  // GUI needs Metal compute pipeline to generate diff images
  if (!software) {
    testing::AddGlobalTestEnvironment(skity::testing::CreateDiffEnv());
  }
#endif

  return RUN_ALL_TESTS();
}
