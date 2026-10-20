// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <skity_c/skity_canvas.h>
#include <skity_c/skity_picture.h>
#include <skity_c/skity_stream.h>

#include <filesystem>
#include <skity/recorder/picture_recorder.hpp>
#include <skity/skity.hpp>

#include "common/golden_test_check.hpp"
#include "handle.hpp"

static const char* kGoldenTestImageDir = CASE_DIR;
static const char* kTigerSKP = RESOURCES_DIR "/skp/tiger.skp";

static const char* kGoldenTestImageCPUTessDir = CASE_DIR "cpu_tess_images/";
static const char* kGoldenTestImageGPUTessDir = CASE_DIR "gpu_tess_images/";
static const char* kGoldenTestImageCoverageAADir =
    CASE_DIR "coverage_aa_images/";

namespace {

struct PathListContext {
  PathListContext(std::string name)
      : expected_image_cpu_tess_path(kGoldenTestImageCPUTessDir),
        expected_image_gpu_tess_path(kGoldenTestImageGPUTessDir),
        expected_image_coverage_aa_path(kGoldenTestImageCoverageAADir) {
    expected_image_cpu_tess_path.append(name);
    expected_image_gpu_tess_path.append(name);
    expected_image_coverage_aa_path.append(name);
  }

  skity::testing::PathList ToPathList() const {
    return {
        .cpu_tess_path = expected_image_cpu_tess_path.c_str(),
        .gpu_tess_path = expected_image_gpu_tess_path.c_str(),
        .simple_shape_path = nullptr,
        .coverage_aa_path = expected_image_coverage_aa_path.c_str(),
    };
  }

  std::filesystem::path expected_image_cpu_tess_path;
  std::filesystem::path expected_image_gpu_tess_path;
  std::filesystem::path expected_image_coverage_aa_path;
};

}  // namespace

TEST(SKP_Golden, Tiger) {
  // Picture / stream go through the module's C ABI; the recorder stays on
  // the core legacy C++ surface the rest of the golden suite uses.
  skity_read_stream stream = skity_read_stream_create_from_file(kTigerSKP);
  ASSERT_NE(stream, nullptr) << "Failed to open SKP file: " << kTigerSKP;

  skity_picture picture = skity_picture_create_from_stream(stream);
  ASSERT_NE(picture, nullptr) << "Failed to parse SKP file: " << kTigerSKP;

  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(1000.f, 1000.f));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Translate(-130, 20);
  // PlayBack takes the C ABI canvas handle; wrap the recorder's canvas.
  std::shared_ptr<skity::Canvas> canvas_view(canvas, [](skity::Canvas*) {});
  skity_canvas canvas_handle = skity::capi::alloc_handle<skity_canvas_s>(
      SKITY_OBJECT_TYPE_CANVAS, 0, std::move(canvas_view));
  skity_picture_play_back(picture, canvas_handle);

  PathListContext context("tiger.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 1000, 1000,
                                                   context.ToPathList()));

  skity_picture_destroy(picture);
  skity_read_stream_destroy(stream);
}
