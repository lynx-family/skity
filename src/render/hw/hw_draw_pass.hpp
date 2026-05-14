// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_DRAW_PASS_HPP
#define SRC_RENDER_HW_HW_DRAW_PASS_HPP

#include <memory>
#include <optional>
#include <skity/geometry/matrix.hpp>
#include <skity/geometry/rect.hpp>
#include <skity/graphic/image.hpp>
#include <vector>

#include "src/gpu/gpu_render_pass.hpp"
#include "src/gpu/gpu_sampler.hpp"
#include "src/gpu/gpu_texture.hpp"

namespace skity {

class HWDraw;

struct DstTextureCopyInfo {
  Rect copy_rect = {};
  GPURegion copy_region = {};
  std::shared_ptr<GPUTexture> texture;
  std::shared_ptr<GPUSampler> sampler;
  Vec4 uv_mapping = {};
};

struct EmulatedLoadInfo {
  HWDraw* draw = nullptr;
  std::shared_ptr<DeferredTextureImage> resolve_image;
};

struct HWDrawPass {
  std::vector<HWDraw*> draw_ops;
  std::optional<DstTextureCopyInfo> dst_texture_copy_info = std::nullopt;
  const DstTextureCopyInfo* dst_read_texture_copy_info = nullptr;

  std::optional<EmulatedLoadInfo> emulated_load_info = std::nullopt;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_DRAW_PASS_HPP
