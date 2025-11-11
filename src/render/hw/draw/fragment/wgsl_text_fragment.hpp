// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_TEXT_FRAGMENT_HPP
#define SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_TEXT_FRAGMENT_HPP

#include <array>
#include <skity/effect/shader.hpp>
#include <skity/graphic/color.hpp>

#include "src/render/hw/draw/hw_wgsl_fragment.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"

namespace skity {

/**
 * Common fragment for text shader.
 * It contains 4 font textures. and a sampler. located in group 1.
 * Also, it contains a function to get the color from the font texture.
 *
 * Subclass if need contains other uniforms. Should begin at group 1 binding 5.
 */
class WGSLTextFragment : public HWWGSLFragment {
 public:
  static constexpr const char* kCommonTextFragment = R"(
    @group(1) @binding(0) var uSampler      : sampler;
    @group(1) @binding(1) var uFontTexture0 : texture_2d<f32>;
    @group(1) @binding(2) var uFontTexture1 : texture_2d<f32>;
    @group(1) @binding(3) var uFontTexture2 : texture_2d<f32>;
    @group(1) @binding(4) var uFontTexture3 : texture_2d<f32>;

    fn get_texture_color(font_index: i32, uv: vec2<f32>) -> vec4<f32> {
       var texture_dimension : vec2<u32> = vec2<u32>(textureDimensions(uFontTexture0));
       var texture_uv        : vec2<f32> = vec2<f32>(uv.x / f32(texture_dimension.x),
                                                     uv.y / f32(texture_dimension.y));

       if font_index == 0 {
         return textureSample(uFontTexture0, uSampler, texture_uv);
       } else if font_index == 1 {
         return textureSample(uFontTexture1, uSampler, texture_uv);
       } else if font_index == 2 {
         return textureSample(uFontTexture2, uSampler, texture_uv);
       } else if font_index == 3 {
         return textureSample(uFontTexture3, uSampler, texture_uv);
       } else {
         return textureSample(uFontTexture0, uSampler, texture_uv);
       }
    }
  )";

  using BatchedTexture = std::array<std::shared_ptr<GPUTexture>, 4>;

  explicit WGSLTextFragment(BatchedTexture textures,
                            std::shared_ptr<GPUSampler> sampler);

  ~WGSLTextFragment() override = default;

  bool CanMerge(const HWWGSLFragment* other) const override;

  void PrepareCMD(Command* cmd, HWDrawContext* context) override;

 private:
  BatchedTexture textures_;
  std::shared_ptr<GPUSampler> sampler_;
};

class WGSLColorTextFragment : public WGSLTextFragment {
 public:
  WGSLColorTextFragment(BatchedTexture textures,
                        std::shared_ptr<GPUSampler> sampler, Color4f color)
      : WGSLTextFragment(std::move(textures), std::move(sampler)),
        color_(color) {}

  ~WGSLColorTextFragment() override = default;

  std::string GetShaderName() const override;

  uint32_t NextBindingIndex() const override;

  std::string GenSourceWGSL() const override;

  bool CanMerge(const HWWGSLFragment* other) const override;

  void PrepareCMD(Command* cmd, HWDrawContext* context) override;

 private:
  Color4f color_;
};

class WGSLColorEmojiFragment : public WGSLTextFragment {
 public:
  WGSLColorEmojiFragment(BatchedTexture textures,
                         std::shared_ptr<GPUSampler> sampler, bool swizzle_rb,
                         float alpha)
      : WGSLTextFragment(std::move(textures), std::move(sampler)),
        swizzle_rb_(swizzle_rb),
        alpha_(alpha) {}

  ~WGSLColorEmojiFragment() override = default;

  std::string GetShaderName() const override;

  uint32_t NextBindingIndex() const override;

  bool CanMerge(const HWWGSLFragment* other) const override;

  void PrepareCMD(Command* cmd, HWDrawContext* context) override;

  std::string GenSourceWGSL() const override;

 private:
  bool swizzle_rb_ = false;
  float alpha_ = 1.f;
};

class WGSLGradientTextFragment : public WGSLTextFragment {
 public:
  WGSLGradientTextFragment(BatchedTexture textures,
                           std::shared_ptr<GPUSampler> sampler,
                           Shader::GradientInfo info, Shader::GradientType type,
                           float global_alpha)
      : WGSLTextFragment(std::move(textures), std::move(sampler)),
        info_(info),
        type_(type),
        global_alpha_(global_alpha),
        gradient_fragment_(info_, type_) {}

  ~WGSLGradientTextFragment() override = default;

  uint32_t NextBindingIndex() const override;

  std::string GetShaderName() const override;

  std::string GenSourceWGSL() const override;

  bool CanMerge(const HWWGSLFragment* other) const override;

  void PrepareCMD(Command* cmd, HWDrawContext* context) override;

 private:
  Shader::GradientInfo info_;
  Shader::GradientType type_;
  float global_alpha_ = 1.f;
  WGXGradientFragment gradient_fragment_;
};

class WGSLSdfColorTextFragment : public WGSLTextFragment {
 public:
  WGSLSdfColorTextFragment(BatchedTexture textures,
                           std::shared_ptr<GPUSampler> sampler, Color4f color)
      : WGSLTextFragment(std::move(textures), std::move(sampler)),
        color_(color) {}

  ~WGSLSdfColorTextFragment() override = default;

  std::string GetShaderName() const override;

  uint32_t NextBindingIndex() const override;

  std::string GenSourceWGSL() const override;

  bool CanMerge(const HWWGSLFragment* other) const override;

  void PrepareCMD(Command* cmd, HWDrawContext* context) override;

 private:
  Color4f color_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_TEXT_FRAGMENT_HPP
