// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_TEXTURE_FRAGMENT_HPP
#define SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_TEXTURE_FRAGMENT_HPP

#include "src/effect/pixmap_shader.hpp"
#include "src/render/hw/draw/hw_wgsl_fragment.hpp"

namespace skity {

class WGSLTextureFragment : public HWWGSLFragment {
 public:
  WGSLTextureFragment(std::shared_ptr<PixmapShader> shader,
                      std::shared_ptr<GPUTexture> texture,
                      std::shared_ptr<GPUSampler> sampler, float global_alpha);

  WGSLTextureFragment(AlphaType alpha_type, TileMode x_tile_mode,
                      TileMode y_tile_mode, std::shared_ptr<GPUTexture> texture,
                      std::shared_ptr<GPUSampler> sampler, float global_alpha);

  ~WGSLTextureFragment() override = default;

  uint32_t NextBindingIndex() const override;

  std::string GenSourceWGSL() const override;

  std::string GetShaderName() const override;

  const char* GetEntryPoint() const override;

  void PrepareCMD(Command* cmd, HWDrawContext* context) override;

 private:
  AlphaType alpha_type_ = AlphaType::kUnpremul_AlphaType;
  TileMode x_tile_mode_ = TileMode::kClamp;
  TileMode y_tile_mode_ = TileMode::kClamp;
  std::shared_ptr<GPUTexture> texture_;
  std::shared_ptr<GPUSampler> sampler_;
  float global_alpha_ = 1.f;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_TEXTURE_FRAGMENT_HPP
