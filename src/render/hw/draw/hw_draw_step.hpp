// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_HW_DRAW_STEP_HPP
#define SRC_RENDER_HW_DRAW_HW_DRAW_STEP_HPP

#include <memory>
#include <skity/graphic/blend_mode.hpp>

#include "src/render/hw/draw/hw_wgsl_fragment.hpp"
#include "src/render/hw/draw/hw_wgsl_geometry.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_shader_generator.hpp"

namespace skity {

struct HWDrawStepContext {
  HWDrawContext* context = nullptr;
  HWDrawState state = {};
  Matrix transform = {};
  float clip_depth = 0.f;
  Rect scissor = {};
  GPUTextureFormat color_format = GPUTextureFormat::kRGBA8Unorm;
  uint32_t sample_count = 1;
  BlendMode blend_mode = BlendMode::kDefault;
  Vec2 scale = {1.f, 1.f};
};

class HWDrawStep : public HWShaderGenerator {
 public:
  HWDrawStep(HWWGSLGeometry* geometry, HWWGSLFragment* fragment,
             bool require_stencil, bool require_depth)
      : geometry_(geometry),
        fragment_(fragment),
        require_stencil_(require_stencil),
        require_depth_(require_depth) {}

  ~HWDrawStep() override = default;

  bool RequireStencil() const { return require_stencil_; }

  bool RequireDepth() const { return require_depth_; }

  void GenerateCommand(const HWDrawStepContext& ctx, Command* cmd,
                       Command* stencil_cmd);

  std::string GetVertexName() const override {
    return geometry_->GetShaderName();
  }

  std::string GenVertexWGSL() const override {
    return geometry_->GenSourceWGSL();
  }

  const char* GetVertexEntryPoint() const override {
    return geometry_->GetEntryPoint();
  }

  std::string GetFragmentName() const override {
    return fragment_->GetShaderName();
  }

  std::string GenFragmentWGSL() const override {
    return fragment_->GenSourceWGSL();
  }

  const char* GetFragmentEntryPoint() const override {
    return fragment_->GetEntryPoint();
  }

 protected:
  virtual GPUStencilState GetStencilState() = 0;

  virtual bool RequireDepthWrite() const = 0;

  virtual bool RequireColorWrite() const = 0;

 private:
  GPURenderPipeline* GetPipeline(HWDrawContext* context, HWDrawState state,
                                 GPUTextureFormat target_format,
                                 uint32_t sample_count, BlendMode blend_mode);

 private:
  HWWGSLGeometry* geometry_;
  HWWGSLFragment* fragment_;
  bool require_stencil_ = false;
  bool require_depth_ = false;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_HW_DRAW_STEP_HPP
