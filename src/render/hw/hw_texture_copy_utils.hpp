// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_TEXTURE_COPY_UTILS_HPP
#define SRC_RENDER_HW_HW_TEXTURE_COPY_UTILS_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

#include "src/logging.hpp"
#include "src/render/hw/hw_draw_pass.hpp"

namespace skity {
namespace hw_texture_copy_utils {

inline Vec4 BuildDstUVMapping(const Rect& copy_rect, uint32_t layer_height,
                              bool bottom_left_origin) {
  DEBUG_CHECK(!copy_rect.IsEmpty());
  DEBUG_CHECK(layer_height > 0);

  const auto width = copy_rect.Width();
  const auto height = copy_rect.Height();
  auto left = copy_rect.Left();
  auto top = copy_rect.Top();

  if (bottom_left_origin) {
    top = layer_height - copy_rect.Top() - copy_rect.Height();
  }

  return Vec4{1.f / width, 1.f / height, -left / width, -top / height};
}

inline std::optional<DstTextureCopyInfo> BuildDstTextureCopyInfo(
    const Rect& layer_space_bounds, uint32_t layer_width, uint32_t layer_height,
    bool bottom_left_origin) {
  if (layer_space_bounds.IsEmpty()) {
    return std::nullopt;
  }

  const auto left =
      std::max(0, static_cast<int32_t>(std::floor(layer_space_bounds.Left())));
  const auto top =
      std::max(0, static_cast<int32_t>(std::floor(layer_space_bounds.Top())));
  const auto right =
      std::min(static_cast<int32_t>(layer_width),
               static_cast<int32_t>(std::ceil(layer_space_bounds.Right())));
  const auto bottom =
      std::min(static_cast<int32_t>(layer_height),
               static_cast<int32_t>(std::ceil(layer_space_bounds.Bottom())));

  const auto width = right - left;
  const auto height = bottom - top;
  if (width <= 0 || height <= 0) {
    return std::nullopt;
  }

  const auto copy_rect = Rect::MakeLTRB(left, top, right, bottom);
  DstTextureCopyInfo copy_info;
  copy_info.copy_region.x = static_cast<uint32_t>(left);
  copy_info.copy_region.y = static_cast<uint32_t>(top);
  copy_info.copy_region.width = static_cast<uint32_t>(width);
  copy_info.copy_region.height = static_cast<uint32_t>(height);
  copy_info.uv_mapping =
      BuildDstUVMapping(copy_rect, layer_height, bottom_left_origin);
  return copy_info;
}

}  // namespace hw_texture_copy_utils
}  // namespace skity

#endif  // SRC_RENDER_HW_HW_TEXTURE_COPY_UTILS_HPP
