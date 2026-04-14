// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <functional>
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

/**
 * @brief compare the display list with the golden texture. If the golden_test
 * is compiled with SKITY_GOLDEN_GUI, a window will be opened to show the
 * display list result, the expected result and the diff result.
 *
 * @param dl    the display list to be compared.
 * @param width the width of the target image list.
 * @param height the height of the target image list.
 * @param path  the path of the expected golden image. If the image does not
 *              exist, the image will be saved to the path. And the test will
 *              be treated as passed.
 *
 * @return true  the display list is the same as the golden texture.
 * @return false the display list is different from the golden texture.
 * @return
 */
bool CompareGoldenTexture(DisplayList* dl, uint32_t width, uint32_t height,
                          const char* path);

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
 * @param path_list  the path list of the expected golden images. If the image
 *                   does not exist, the image will be saved to the path. And
 *                   the test will be treated as passed.
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
