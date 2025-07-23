// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_TEXT_ATLAS_ATLAS_MANAGER_HPP
#define SRC_RENDER_TEXT_ATLAS_ATLAS_MANAGER_HPP

#include <memory>
#include <optional>
#include <skity/graphic/paint.hpp>
#include <skity/text/font.hpp>
#include <skity/text/typeface.hpp>
#include <vector>

#include "src/gpu/gpu_sampler.hpp"
#include "src/gpu/gpu_texture.hpp"
#include "src/render/text/atlas/atlas_bitmap.hpp"
#include "src/render/text/atlas/atlas_glyph.hpp"
#include "src/render/text/atlas/atlas_texture.hpp"

namespace skity {

class GPUContextImpl;

class Atlas {
 public:
  Atlas(AtlasFormat format, GPUDevice* gpu_device, bool enable_larger_atlas);

  AtlasFormat GetFormat() { return format_; }

  const AtlasConfig& GetConfig() { return atlas_config_; }

  GlyphRegion GetGlyphRegion(const Font& font, GlyphID glyph_id,
                             const Paint& paint, bool load_sdf,
                             float context_scale, const Matrix& transform);

  // upload atlas from memory storage to gpu texture
  void UploadAtlas(uint32_t group_index);

  Vec2 CalculateUV(uint32_t bitmap_index, uint32_t x, uint32_t y);

  std::array<std::shared_ptr<GPUTexture>,
             AtlasConfig::MAX_NUM_TEXTURE_PER_ATLAS>
  GetGPUTexture(uint32_t index);

  std::array<std::shared_ptr<GPUSampler>,
             AtlasConfig::MAX_NUM_TEXTURE_PER_ATLAS>
  GetGPUSamplers(uint32_t index,
                 GPUFilterMode filter_mode = GPUFilterMode::kNearest);

  std::shared_ptr<GPUSampler> GetGPUSampler(
      uint32_t index, GPUFilterMode filter_mode = GPUFilterMode::kNearest);

  void ClearExtraRes();

 private:
  // add one glyph to memory atlas
  GlyphRegion GenerateGlyphRegion(const Font& font, GlyphKey const& key,
                                  const Paint& paint, bool load_sdf);

  GlyphRegion GenerateGlyphRegionInternal(const GlyphKey& key,
                                          const GlyphBitmapData& glyph_bitmap);

  AtlasFormat format_;
  GPUDevice* gpu_device_;
  const AtlasConfig atlas_config_;

  uint32_t bytes_per_pixel_;
  std::vector<std::unique_ptr<AtlasBitmap>> atlas_bitmap_;
  uint32_t current_bitmap_index_ = 0;
  std::vector<std::unique_ptr<AtlasTextureArray>> atlas_texture_array_;
  uint32_t least_used_index_ = 0;
};

class AtlasManager {
 public:
  AtlasManager(GPUDevice* gpu_device, GPUContextImpl* gpu_context);

  Atlas* GetAtlas(AtlasFormat format);

  void ClearExtraRes();

 private:
  std::unique_ptr<Atlas> atlas_[2]{nullptr, nullptr};
  GPUDevice* gpu_device_;
  GPUContextImpl* gpu_context_;
};

}  // namespace skity

#endif  // SRC_RENDER_TEXT_ATLAS_ATLAS_MANAGER_HPP
