// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <skity/recorder/display_list.hpp>
#include <skity/skity.hpp>

namespace skity {
namespace testing {

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
bool CompareGoldenTexture(std::unique_ptr<DisplayList> dl, uint32_t width,
                          uint32_t height, const char* path);

struct DiffResult {
  // whether the test passed.
  bool passed = false;
  // the diff between the two images.
  float diff_percent = 0.f;
  // the max diff between the two images.
  float max_diff_percent = 0.f;
  // the diff pixel count between the two images.
  uint32_t diff_pixel_count = 0;

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
