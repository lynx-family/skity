// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/atlas/atlas_allocator.hpp"

#include "src/render/text/atlas/atlas_glyph.hpp"

namespace skity {

AtlasAllocator::AtlasAllocator(uint32_t width, uint32_t height)
    : width_(width), height_(height) {
  // one pixel border for sampling texture
  glm::ivec3 node = {1, 1, width - 2};

  nodes_.emplace_back(node);
}

glm::ivec4 AtlasAllocator::AllocateRegion(uint32_t width, uint32_t height) {
  glm::ivec4 region = {0, 0, (int32_t)width, (int32_t)height};

  int32_t best_index = -1;
  int32_t best_height = std::numeric_limits<int32_t>::max();
  int32_t best_width = std::numeric_limits<int32_t>::max();

  int32_t y = -1;
  for (int32_t i = 0; i < static_cast<int32_t>(nodes_.size()); i++) {
    y = QueryFitY(i, width, height);
    if (y > 0) {
      glm::ivec3 node = nodes_[i];
      if ((y + static_cast<int32_t>(height)) < best_height ||
          (((y + static_cast<int32_t>(height)) == best_height) &&
           (node.z > 0 && node.z < best_width))) {
        best_index = i;
        best_height = y + height;
        best_width = node.z;
        region.x = node.x;
        region.y = y;
      }
    }
  }

  if (best_index == -1) {
    return INVALID_LOC;
  }

  glm::ivec3 node = {region.x, region.y + height, width};

  nodes_.insert(nodes_.begin() + best_index, node);

  for (auto i = nodes_.begin() + best_index + 1; i != nodes_.end(); i++) {
    auto i_node = i;
    auto p_node = i - 1;

    if (i_node->x < (p_node->x + p_node->z)) {
      int32_t shrink = p_node->x + p_node->z - i_node->x;
      i_node->x += shrink;
      i_node->z -= shrink;

      if (i_node->z <= 0) {
        i = nodes_.erase(i_node);
        i--;
      } else {
        break;
      }
    } else {
      break;
    }
  }
  MergeNodes();
  used_ += width * height;
  return region;
}

int32_t AtlasAllocator::QueryFitY(int32_t index, uint32_t width,
                                  uint32_t height) {
  glm::ivec3 node = nodes_[index];

  int32_t x = node.x;
  int32_t y = node.y;

  int32_t width_left = width;
  int32_t i = index;

  if (x + width > this->width_ - 1) {
    return -1;
  }

  y = node.y;
  while (width_left > 0) {
    node = nodes_[i];
    if (node.y > y) {
      y = node.y;
    }
    if (y + height > this->height_ - 1) {
      return -1;
    }

    width_left -= node.z;
    i++;
  }

  return y;
}

void AtlasAllocator::Clear() {
  nodes_.clear();
  nodes_.emplace_back(glm::ivec3{1, 1, width_ - 2});
  used_ = 0;
}

Vec2 AtlasAllocator::CalculateUV(uint32_t x, uint32_t y, bool normalized) {
  float u = static_cast<float>(x);
  float v = static_cast<float>(y);

  if (normalized) {
    u /= static_cast<float>(width_);
    v /= static_cast<float>(height_);
  }

  return {u, v};
}

void AtlasAllocator::MergeNodes() {
  for (auto node = nodes_.begin(); node < nodes_.end() - 1; node++) {
    auto next = node + 1;
    if (node->y == next->y) {
      node->z += next->z;
      nodes_.erase(next);
    }
  }
}

}  // namespace skity
