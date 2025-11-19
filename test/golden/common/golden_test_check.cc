// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "common/golden_test_check.hpp"

#include <gtest/gtest.h>

#include <iostream>
#include <skity/codec/codec.hpp>

#include "common/golden_test_env.hpp"

#ifdef SKITY_GOLDEN_GUI
#include "playground/playground.hpp"
#endif

namespace skity {
namespace testing {

struct GoldenTestEnvConfig {
  static GoldenTestEnvConfig GPUTessellation() {
    return GoldenTestEnvConfig{true};
  }

  bool enable_gpu_tessellation = false;
  bool enable_simple_shape_pipeline = false;
};

struct AutoRestoreConfig {
  AutoRestoreConfig(GPUContext* gpu_context, GoldenTestEnvConfig config)
      : gpu_context(gpu_context),
        restore_config{gpu_context->IsEnableGPUTessellation(),
                       gpu_context->IsEnableSimpleShapePipeline()} {
    gpu_context->SetEnableGPUTessellation(config.enable_gpu_tessellation);
    gpu_context->SetEnableSimpleShapePipeline(
        config.enable_simple_shape_pipeline);
  }

  ~AutoRestoreConfig() {
    gpu_context->SetEnableGPUTessellation(
        restore_config.enable_gpu_tessellation);
    gpu_context->SetEnableSimpleShapePipeline(
        restore_config.enable_simple_shape_pipeline);
  }

  std::string GetNameSuffix() const {
    if (gpu_context->IsEnableGPUTessellation()) {
      return "gpu_tess";
    }

    if (gpu_context->IsEnableSimpleShapePipeline()) {
      return "simple_shape";
    }
    return "";
  }

 private:
  GPUContext* gpu_context = gpu_context;
  GoldenTestEnvConfig restore_config;
};

std::shared_ptr<Pixmap> ReadImage(const char* path) {
  auto data = skity::Data::MakeFromFileName(path);

  if (data == nullptr) {
    return {};
  }

  auto codec = skity::Codec::MakeFromData(data);

  if (codec == nullptr) {
    return {};
  }

  codec->SetData(data);

  return codec->Decode();
}

static bool CompareGoldenTextureImpl(DisplayList* dl, uint32_t width,
                                     uint32_t height, const char* path,
                                     GoldenTestEnvConfig config) {
  auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();

  std::cout << "test name: " << test_info->name() << std::endl;
  std::cout << "test case name: " << test_info->test_case_name() << std::endl;
  std::cout << "test suite name: " << test_info->test_suite_name() << std::endl;

  auto env = GoldenTestEnv::GetInstance();
  AutoRestoreConfig auto_restore_config(env->GetGPUContext(), config);
  auto texture = env->DisplayListToTexture(dl, width, height);

  EXPECT_TRUE(texture != nullptr)
      << "Failed to generate rendering result texture";

  auto source = texture->ReadPixels();

  EXPECT_TRUE(source != nullptr)
      << "Failed to read rendering result texture pixels";

  auto target = ReadImage(path);

  if (target != nullptr && (source->Width() != target->Width() ||
                            source->Height() != target->Height())) {
    // size is not match
    // maybe the test case is rewrite and the golden image is not updated.
    // so we just ignore the target image.
    // If it is in development enviornment let the developer use testing GUI
    // update it.
    // or if running in testing environment, ignore the target image and let the
    // case failed.
    target = nullptr;
  }

  auto result = ComparePixels(source, target);

#ifdef SKITY_GOLDEN_GUI
  return OpenPlayground(result.Passed(), texture, target, path,
                        auto_restore_config.GetNameSuffix());
#else
  return result.Passed();
#endif
}

bool CompareGoldenTexture(DisplayList* dl, uint32_t width, uint32_t height,
                          const char* path) {
  return CompareGoldenTextureImpl(dl, width, height, path, {});
}

bool CompareGoldenTexture(DisplayList* dl, uint32_t width, uint32_t height,
                          PathList path_list) {
  bool result = true;
  if (path_list.cpu_tess_path != nullptr) {
    if (!CompareGoldenTextureImpl(dl, width, height, path_list.cpu_tess_path,
                                  {})) {
      result = false;
    }
  }

  if (path_list.gpu_tess_path != nullptr) {
    if (!CompareGoldenTextureImpl(dl, width, height, path_list.gpu_tess_path,
                                  {.enable_gpu_tessellation = true})) {
      result = false;
    }
  }

  if (path_list.simple_shape_path != nullptr) {
    if (!CompareGoldenTextureImpl(dl, width, height,
                                  path_list.simple_shape_path,
                                  {.enable_simple_shape_pipeline = true})) {
      result = false;
    }
  }
  return result;
}

bool DiffResult::Passed() const {
  if (!passed) {
    return false;
  }

  // check the diff percent.

  if (diff_percent > 0.1f) {
    // If more than 10% pixel is different, the test is failed.
    return false;
  }

  if (max_diff_percent > 50.0f) {
    // If the max channel diff is more than 50%, the test is failed.
    return false;
  }

  if (diff_pixel_count > 50) {
    // If the diff pixel count is more than 50, the test is failed.
    return false;
  }

  return true;
}

DiffResult ComparePixels(const std::shared_ptr<Pixmap>& source,
                         const std::shared_ptr<Pixmap>& target) {
  DiffResult result;

  if (target == nullptr) {
    result.passed = false;
  } else {
    result.passed = true;

    // running the diff test.
    auto width = source->Width();
    auto height = source->Height();
    auto src_data = reinterpret_cast<const uint8_t*>(source->Addr());
    auto dst_data = reinterpret_cast<const uint8_t*>(target->Addr());

    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        auto src = src_data + (y * width + x) * 4;
        auto dst = dst_data + (y * width + x) * 4;

        for (int i = 0; i < 4; ++i) {
          uint8_t src_channel = src[i];
          uint8_t dst_channel = dst[i];

          // FIXME: our test image is premul alpha, but the target image is
          // unpremul alpha.
          // so we need to convert the target image to premul alpha.
          if (target->GetAlphaType() == AlphaType::kUnpremul_AlphaType &&
              i < 3) {
            dst_channel =
                static_cast<uint8_t>(std::round(dst_channel * dst[3] / 255.f));
          }

          auto diff = std::abs(src_channel - dst_channel);

          if (diff > 0) {
            result.diff_percent += 1.f / float(width * height * 4);
            result.max_diff_percent =
                std::max(result.max_diff_percent, diff / float(255));
            result.diff_pixel_count += 1;
          }
        }
      }
    }
  }

  return result;
}

}  // namespace testing
}  // namespace skity
