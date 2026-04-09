// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "common/golden_test_check.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <skity/codec/codec.hpp>
#include <sstream>

#include "common/golden_test_env.hpp"

#ifdef SKITY_GOLDEN_GUI
#include "playground/playground.hpp"
#endif

namespace skity {

namespace {

constexpr uint8_t kIgnoredChannelDiff = 2;
constexpr uint8_t kIgnoredSoftEdgeChannelDiff = 8;

uint8_t GetComparableChannel(const std::shared_ptr<Pixmap>& pixmap,
                             const uint8_t* pixel, int channel_index) {
  auto channel = pixel[channel_index];

  // The rendered result is premultiplied, while decoded golden images may be
  // unpremultiplied. Align them before comparing.
  if (pixmap->GetAlphaType() == AlphaType::kUnpremul_AlphaType &&
      channel_index < 3) {
    channel = static_cast<uint8_t>(std::round(channel * pixel[3] / 255.f));
  }

  return channel;
}

bool IsSignificantPixelDiffWithThreshold(const std::shared_ptr<Pixmap>& source,
                                         const std::shared_ptr<Pixmap>& target,
                                         const uint8_t* src, const uint8_t* dst,
                                         uint8_t ignored_channel_diff,
                                         float* pixel_max_diff_percent) {
  bool pixel_diff = false;

  for (int i = 0; i < 4; ++i) {
    uint8_t src_channel = GetComparableChannel(source, src, i);
    uint8_t dst_channel = GetComparableChannel(target, dst, i);
    auto diff =
        static_cast<uint8_t>(std::abs(int(src_channel) - int(dst_channel)));

    if (diff <= ignored_channel_diff) {
      continue;
    }

    *pixel_max_diff_percent =
        std::max(*pixel_max_diff_percent, diff / float(255));
    pixel_diff = true;
  }

  return pixel_diff;
}

bool IsSignificantPixelDiff(const std::shared_ptr<Pixmap>& source,
                            const std::shared_ptr<Pixmap>& target,
                            const uint8_t* src, const uint8_t* dst,
                            float* pixel_max_diff_percent) {
  return IsSignificantPixelDiffWithThreshold(
      source, target, src, dst, kIgnoredChannelDiff, pixel_max_diff_percent);
}

bool IsSoftEdgePixel(const std::shared_ptr<Pixmap>& source,
                     const std::shared_ptr<Pixmap>& target, const uint8_t* src,
                     const uint8_t* dst) {
  uint8_t src_alpha = GetComparableChannel(source, src, 3);
  uint8_t dst_alpha = GetComparableChannel(target, dst, 3);

  return (src_alpha != 0 && src_alpha != 255) ||
         (dst_alpha != 0 && dst_alpha != 255);
}

}  // namespace

static std::string EscapeJSONString(const std::string& input) {
  std::ostringstream ss;
  for (char c : input) {
    if (c == '"') {
      ss << "\\\"";
    } else if (c == '\\') {
      ss << "\\\\";
    } else if (c == '\b') {
      ss << "\\b";
    } else if (c == '\f') {
      ss << "\\f";
    } else if (c == '\n') {
      ss << "\\n";
    } else if (c == '\r') {
      ss << "\\r";
    } else if (c == '\t') {
      ss << "\\t";
    } else if (static_cast<unsigned char>(c) <= 0x1f) {
      ss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
         << static_cast<int>(c);
    } else {
      ss << c;
    }
  }
  return ss.str();
}
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

  if (!result.Passed()) {
    std::string actual_path;
    std::string diff_pixels_path;

    const char* out_dir = std::getenv("AGENT_OUT_DIR");
    if (path && out_dir) {
      std::string p(path);
      auto slash_pos = p.find_last_of("/\\");
      std::string basename =
          (slash_pos != std::string::npos) ? p.substr(slash_pos + 1) : p;
      auto dot_pos = basename.find_last_of('.');
      std::string name = (dot_pos != std::string::npos)
                             ? basename.substr(0, dot_pos)
                             : basename;
      std::string ext =
          (dot_pos != std::string::npos) ? basename.substr(dot_pos) : ".png";

      std::string dir = std::string(out_dir) + "/";

      actual_path = dir + name + "_actual" + ext;
      diff_pixels_path = dir + name + "_diff_pixels.json";

      env->SaveGoldenImage(source, actual_path.c_str());

      // Write pixel differences to JSON file
      if (!result.diff_pixels.empty()) {
        std::ofstream ofs(diff_pixels_path);
        if (ofs.is_open()) {
          ofs << "[\n";
          for (size_t i = 0; i < result.diff_pixels.size(); ++i) {
            auto& pd = result.diff_pixels[i];
            ofs << "  {\"x\":" << pd.x << ",\"y\":" << pd.y << ",\"src\":["
                << (int)pd.src[0] << "," << (int)pd.src[1] << ","
                << (int)pd.src[2] << "," << (int)pd.src[3] << "]"
                << ",\"dst\":[" << (int)pd.dst[0] << "," << (int)pd.dst[1]
                << "," << (int)pd.dst[2] << "," << (int)pd.dst[3] << "]}";
            if (i + 1 < result.diff_pixels.size()) ofs << ",";
            ofs << "\n";
          }
          ofs << "]\n";
          ofs.close();
        }
      }
    }

    std::cout << "[[GOLDEN_TEST_FAILED_METRICS]]" << std::endl;
    std::cout << "{" << std::endl;
    std::cout << "  \"diff_percent\": " << result.diff_percent << ","
              << std::endl;
    std::cout << "  \"max_diff_percent\": " << result.max_diff_percent << ","
              << std::endl;
    std::cout << "  \"diff_pixel_count\": " << result.diff_pixel_count << ","
              << std::endl;
    if (result.min_x <= result.max_x && result.min_y <= result.max_y) {
      std::cout << "  \"diff_bbox\": [" << std::dec << result.min_x << ", "
                << result.min_y << ", " << (result.max_x - result.min_x + 1)
                << ", " << (result.max_y - result.min_y + 1) << "],"
                << std::endl;
    }
    std::cout << "  \"expected_image\": \""
              << EscapeJSONString(path ? path : "") << "\"";
    if (!actual_path.empty()) {
      std::cout << "," << std::endl
                << "  \"actual_image\": \"" << EscapeJSONString(actual_path)
                << "\"";
    }
    if (!diff_pixels_path.empty()) {
      std::cout << "," << std::endl
                << "  \"diff_pixels_file\": \""
                << EscapeJSONString(diff_pixels_path) << "\"";
    }
    std::cout << std::endl << "}" << std::endl;
  }

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

    result.min_x = width;
    result.min_y = height;
    result.max_x = -1;
    result.max_y = -1;

    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        auto src = src_data + (y * width + x) * 4;
        auto dst = dst_data + (y * width + x) * 4;

        float pixel_max_diff_percent = 0.f;
        bool pixel_diff = IsSignificantPixelDiff(source, target, src, dst,
                                                 &pixel_max_diff_percent);

        if (pixel_diff && IsSoftEdgePixel(source, target, src, dst) &&
            !IsSignificantPixelDiffWithThreshold(source, target, src, dst,
                                                 kIgnoredSoftEdgeChannelDiff,
                                                 &pixel_max_diff_percent)) {
          pixel_diff = false;
          pixel_max_diff_percent = 0.f;
        }

        if (pixel_diff) {
          result.diff_percent += 1.f / float(width * height);
          result.max_diff_percent =
              std::max(result.max_diff_percent, pixel_max_diff_percent);
          result.diff_pixel_count += 1;
          result.min_x = std::min(result.min_x, x);
          result.min_y = std::min(result.min_y, y);
          result.max_x = std::max(result.max_x, x);
          result.max_y = std::max(result.max_y, y);

          if (result.diff_pixels.size() < 1000) {
            PixelDiff pd;
            pd.x = x;
            pd.y = y;
            for (int i = 0; i < 4; ++i) {
              pd.src[i] = src[i];
              pd.dst[i] = dst[i];
            }
            result.diff_pixels.push_back(pd);
          }
        }
      }
    }
  }

  return result;
}

}  // namespace testing
}  // namespace skity
