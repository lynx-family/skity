// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_TEXTURE_FRAGMENT_HPP
#define SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_TEXTURE_FRAGMENT_HPP

#include <skity/graphic/color_space.hpp>
#include <skity/graphic/sampling_options.hpp>

#include "src/effect/pixmap_shader.hpp"
#include "src/render/hw/draw/hw_wgsl_fragment.hpp"
#include "src/render/hw/hw_pipeline_key.hpp"

namespace skity {

class WGSLTextureFragment : public HWWGSLFragment {
 public:
  WGSLTextureFragment(std::shared_ptr<PixmapShader> shader,
                      std::shared_ptr<GPUTexture> texture,
                      std::shared_ptr<GPUSampler> sampler, float global_alpha,
                      const Matrix& local_matrix, float width, float height);

  WGSLTextureFragment(AlphaType alpha_type, TileMode x_tile_mode,
                      TileMode y_tile_mode, std::shared_ptr<GPUTexture> texture,
                      std::shared_ptr<GPUSampler> sampler, float global_alpha,
                      const Matrix& local_matrix, float width, float height);

  ~WGSLTextureFragment() override = default;

  uint32_t NextBindingIndex() const override;

  void PrepareCMD(Command* cmd, HWDrawContext* context) override;

  void WriteFSFunctionsAndStructs(std::stringstream& ss) const override;

  void WriteFSUniforms(std::stringstream& ss) const override;

  void WriteFSMain(std::stringstream& ss) const override;

  void WriteVSFunctionsAndStructs(std::stringstream& ss) const override;

  void WriteVSUniforms(std::stringstream& ss) const override;

  void WriteVSAssgnShadingVarings(std::stringstream& ss) const override;

  void BindVSUniforms(Command* cmd, HWDrawContext* context,
                      const Matrix& transform, float clip_depth,
                      Command* stencil_cmd) override;

  HWFunctionBaseKey GetMainKey() const override;

  HWFunctionBaseKey GetVSSubKey() const override;

  std::optional<std::vector<std::string>> GetVarings() const override;

 private:
  AlphaType alpha_type_ = AlphaType::kUnpremul_AlphaType;
  TileMode x_tile_mode_ = TileMode::kClamp;
  TileMode y_tile_mode_ = TileMode::kClamp;
  std::shared_ptr<GPUTexture> texture_;
  std::shared_ptr<GPUSampler> sampler_;
  float global_alpha_ = 1.f;

  Matrix local_matrix_;
  float width_;
  float height_;
  CubicResampler cubic_;
  ColorSpace color_space_{};
  // Non-sRGB images generate a decode variant of the texture shader; sRGB
  // images produce the plain (pre-color-space) shader byte for byte.
  bool has_color_space_decode_ = false;
  // Precombined source -> working (sRGB) gamut matrix for the shader decode.
  Matrix gamut_to_working_{};
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_TEXTURE_FRAGMENT_HPP
