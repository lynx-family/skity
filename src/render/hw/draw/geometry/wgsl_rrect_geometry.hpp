// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_RRECT_GEOMETRY_HPP
#define SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_RRECT_GEOMETRY_HPP

#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>

#include "src/render/hw/draw/hw_wgsl_geometry.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"

namespace skity {

class WGSLRRectGeometry : public HWWGSLGeometry {
 public:
  WGSLRRectGeometry(const RRect& rrect, const Paint& paint);

  ~WGSLRRectGeometry() override = default;

  const std::vector<GPUVertexBufferLayout>& GetBufferLayout() const override;

  std::string GetShaderName() const override;

  void WriteVSFunctionsAndStructs(std::stringstream& ss) const override;

  void WriteVSUniforms(std::stringstream& ss) const override;

  void WriteVSInput(std::stringstream& ss) const override;

  void WriteVSMain(std::stringstream& ss) const override;

  std::optional<std::vector<std::string>> GetVarings() const override;

  void WriteFSFunctionsAndStructs(std::stringstream& ss) const override;

  void WriteFSAlphaMask(std::stringstream& ss) const override;

  void PrepareCMD(Command* cmd, HWDrawContext* context, const Matrix& transform,
                  float clip_depth, Command* stencil_cmd) override;

  static GPUBufferView CreateVertexBufferView(HWStageBuffer* stage_bufer);

  static GPUBufferView CreateIndexBufferView(HWStageBuffer* stage_bufer);

 private:
  const RRect& rrect_;
  const Paint& paint_;
  std::vector<GPUVertexBufferLayout> layout_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_RRECT_GEOMETRY_HPP
