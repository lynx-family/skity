// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <functional>
#include <optional>
#include <skity/gpu/gpu_context_gl.hpp>
#include <skity/recorder/display_list.hpp>
#include <skity/skity.hpp>
#include <vector>

namespace skity {
namespace testing {

struct PathList {
  const char* cpu_tess_path = nullptr;
  const char* gpu_tess_path = nullptr;
  const char* simple_shape_path = nullptr;
};

struct GoldenTestEnvConfig {
  static GoldenTestEnvConfig GPUTessellation() {
    GoldenTestEnvConfig config;
    config.enable_gpu_tessellation = true;
    return config;
  }

  bool enable_gpu_tessellation = false;
  bool enable_simple_shape_pipeline = false;
  std::optional<bool> supports_framebuffer_fetch = std::nullopt;
  std::optional<GLSurfaceMode> gl_surface_mode = std::nullopt;
  std::optional<bool> gl_has_stencil_attachment = std::nullopt;
  uint32_t sample_count = 4;
  bool use_backend_specific_golden = false;
};

/**
 * @brief compare the display list with the golden texture. If the golden_test
 * is compiled with SKITY_GOLDEN_GUI, a window will be opened to show the
 * display list result, the expected result and the diff result.
 *
 * @param dl    the display list to be compared.
 * @param width the width of the target image list.
 * @param height the height of the target image list.
 * @param path  the path of the expected golden image. Missing golden images
 *              fail the test by default. Set
 *              SKITY_UPDATE_MISSING_GOLDEN=1 to generate them explicitly.
 *
 * @return true  the display list is the same as the golden texture.
 * @return false the display list is different from the golden texture.
 * @return
 */
bool CompareGoldenTexture(DisplayList* dl, uint32_t width, uint32_t height,
                          const char* path);

bool CompareGoldenTexture(DisplayList* dl, uint32_t width, uint32_t height,
                          const char* path, GoldenTestEnvConfig config);

bool CompareGoldenTexture(uint32_t width, uint32_t height, const char* path,
                          const std::function<void(Canvas*)>& render);

/**
 * @brief compare the display list with the golden texture. If the golden_test
 * is compiled with SKITY_GOLDEN_GUI, a window will be opened to show the
 * display list result, the expected result and the diff result.
 *
 * @param dl         the display list to be compared.
 * @param width      the width of the target image list.
 * @param height     the height of the target image list.
 * @param path_list  the path list of the expected golden images. Missing
 *                   golden images fail the test by default. Set
 *                   SKITY_UPDATE_MISSING_GOLDEN=1 to generate them explicitly.
 *
 * @return true  the display list is the same as the golden texture.
 * @return false the display list is different from the golden texture.
 * @return
 */
bool CompareGoldenTexture(DisplayList* dl, uint32_t width, uint32_t height,
                          PathList path_list);

struct PixelDiff {
  int32_t x = 0;
  int32_t y = 0;
  uint8_t src[4] = {0};
  uint8_t dst[4] = {0};
};

struct DiffResult {
  // whether the test passed.
  bool passed = false;
  // the diff between the two images.
  float diff_percent = 0.f;
  // the max diff between the two images.
  float max_diff_percent = 0.f;
  // the diff pixel count between the two images.
  uint32_t diff_pixel_count = 0;

  // bounding box of the differences
  int32_t min_x = -1;
  int32_t min_y = -1;
  int32_t max_x = -1;
  int32_t max_y = -1;

  // store up to 1000 diff pixels for agent analysis
  std::vector<PixelDiff> diff_pixels;

  bool Passed() const;
};

/**
 * @brief compare the two images.
 *
 * @param source the source image.
 * @param target the target image.
 *
 * @return DiffResult the diff result.
 */
DiffResult ComparePixels(const std::shared_ptr<Pixmap>& source,
                         const std::shared_ptr<Pixmap>& target);

}  // namespace testing
}  // namespace skity
