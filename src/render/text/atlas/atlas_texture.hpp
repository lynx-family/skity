// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_TEXT_ATLAS_ATLAS_TEXTURE_HPP
#define SRC_RENDER_TEXT_ATLAS_ATLAS_TEXTURE_HPP

#include <memory>
#include <optional>
#include <skity/graphic/paint.hpp>
#include <skity/text/typeface.hpp>
#include <vector>

#include "src/gpu/gpu_sampler.hpp"
#include "src/gpu/gpu_texture.hpp"
#include "src/render/text/atlas/atlas_allocator.hpp"
#include "src/render/text/atlas/atlas_glyph.hpp"

namespace skity {

class GPUDevice;

class AtlasTexture {
 public:
  AtlasTexture(uint32_t width, uint32_t height, AtlasFormat format,
               GPUDevice* gpu_device);

  ~AtlasTexture();

  void UploadAtlas(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                   uint8_t* data);

  std::shared_ptr<GPUTexture> GetTexture() const;

  std::shared_ptr<GPUSampler> GetSampler(
      const GPUSamplerDescriptor& descriptor) const;

  uint32_t GetWidth() const { return width_; }

  uint32_t GetHeight() const { return height_; }

 private:
  void InitTexture();

  uint32_t width_;
  uint32_t height_;
  AtlasFormat format_;
  GPUDevice* gpu_device_;
  std::shared_ptr<GPUTexture> texture_;
  bool valid_texture_ = false;
};

class AtlasTextureArray {
 public:
  AtlasTextureArray(uint32_t width, uint32_t height, AtlasFormat format,
                    GPUDevice* gpu_device);
  ~AtlasTextureArray();

  std::array<std::shared_ptr<GPUTexture>,
             AtlasConfig::MAX_NUM_TEXTURE_PER_ATLAS>
  GetTextures() const;

  std::array<std::shared_ptr<GPUSampler>,
             AtlasConfig::MAX_NUM_TEXTURE_PER_ATLAS>
  GetSamplers(const GPUSamplerDescriptor& descriptor) const;

  std::shared_ptr<GPUSampler> GetSampler(
      const GPUSamplerDescriptor& descriptor) const;

  uint32_t GetWidth() const { return width_; }

  uint32_t GetHeight() const { return height_; }

  void UploadAtlas(uint32_t index, uint32_t x, uint32_t y, uint32_t width,
                   uint32_t height, uint8_t* data);

 private:
  uint32_t width_;
  uint32_t height_;
  AtlasFormat format_;
  GPUDevice* gpu_device_;
  std::array<std::unique_ptr<AtlasTexture>,
             AtlasConfig::MAX_NUM_TEXTURE_PER_ATLAS>
      texture_array_{};
};

}  // namespace skity

#endif  // SRC_RENDER_TEXT_ATLAS_ATLAS_TEXTURE_HPP
