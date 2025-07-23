// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_TEXT_ATLAS_ATLAS_ALLOCATOR_HPP
#define SRC_RENDER_TEXT_ATLAS_ATLAS_ALLOCATOR_HPP

#include <skity/geometry/vector.hpp>
#include <vector>

namespace skity {

/**
 * AtlasAllocator is used to allocate small regions in a big
 * rectangle bitmap or texture for small glyphs one by one.
 *
 * The actual implementation is based on the article by Jukka Jylänki :
 * "A Thousand Ways to Pack the Bin - A Practical Approach to Two-Dimensional
 * Rectangle Bin Packing", February 27, 2010.
 */
class AtlasAllocator {
 public:
  AtlasAllocator(uint32_t width, uint32_t height);

  /**
   * @brief               Allocate a new region in atlas.
   *
   * @param width         width of region to allocate
   * @param height        height of region to allocate
   * @return glm::ivec2   coordinates of the allocated region
   */
  glm::ivec4 AllocateRegion(uint32_t width, uint32_t height);

  /**
   * @brief Remove all allocated regions from the atlas.
   *
   */
  void Clear();

  /**
   * @brief   Calculate texture uv coordinate
   */
  Vec2 CalculateUV(uint32_t x, uint32_t y, bool normalized = true);

  uint32_t Width() const { return width_; }
  uint32_t Height() const { return height_; }

 private:
  int32_t QueryFitY(int32_t index, uint32_t width, uint32_t height);
  void MergeNodes();

 private:
  // Width (in pixels) of the underlying texture
  uint32_t width_ = {};
  // Height(in pixels) of the underlying texture
  uint32_t height_ = {};
  // allocated surface size
  uint32_t used_ = {};
  // Atlas has been modified
  // Allocated nodes, [x, y, width]
  std::vector<glm::ivec3> nodes_ = {};
};

}  // namespace skity

#endif  // SRC_RENDER_TEXT_ATLAS_ATLAS_ALLOCATOR_HPP
