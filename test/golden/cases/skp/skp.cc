// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <filesystem>
#include <skity/io/picture.hpp>
#include <skity/io/stream.hpp>
#include <skity/recorder/picture_recorder.hpp>
#include <skity/skity.hpp>

#include "common/golden_test_check.hpp"

static const char* kGoldenTestImageDir = CASE_DIR;
static const char* kTigerSKP = RESOURCES_DIR "/skp/tiger.skp";

TEST(SKP_Golden, Tiger) {
  auto stream = skity::ReadStream::CreateFromFile(kTigerSKP);
  ASSERT_NE(stream, nullptr) << "Failed to open SKP file: " << kTigerSKP;

  auto picture = skity::Picture::MakeFromStream(*stream);
  ASSERT_NE(picture, nullptr) << "Failed to parse SKP file: " << kTigerSKP;

  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(1000.f, 1000.f));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Translate(-130, 20);
  picture->PlayBack(canvas);

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("tiger.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 1000, 1000,
      skity::testing::PathList{.cpu_tess_path = expected_image_path.c_str(),
                               .gpu_tess_path = expected_image_path.c_str()}));
}
