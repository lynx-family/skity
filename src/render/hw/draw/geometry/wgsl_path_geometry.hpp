// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_PATH_GEOMETRY_HPP
#define SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_PATH_GEOMETRY_HPP

#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>

#include "src/render/hw/draw/hw_wgsl_geometry.hpp"

namespace skity {

class WGSLPathGeometry : public HWWGSLGeometry {
 public:
  WGSLPathGeometry(const Path& path, const Paint& paint, bool is_stroke);

  ~WGSLPathGeometry() override = default;

  const std::vector<GPUVertexBufferLayout>& GetBufferLayout() const override;

  std::string GetShaderName() const override;

  void PrepareCMD(Command* cmd, HWDrawContext* context, const Matrix& transform,
                  float clip_depth, Command* stencil_cmd) override;

  void WriteVSFunctionsAndStructs(std::stringstream& ss) const override;

  void WriteVSUniforms(std::stringstream& ss) const override;

  void WriteVSInput(std::stringstream& ss) const override;

  void WriteVSMain(std::stringstream& ss) const override;

 private:
  const Path& path_;
  const Paint& paint_;
  bool is_stroke_;
  std::vector<GPUVertexBufferLayout> layout_;
};

class WGSLPathAAGeometry : public HWWGSLGeometry {
 public:
  WGSLPathAAGeometry(const Path& path, const Paint& paint);

  ~WGSLPathAAGeometry() override = default;

  const std::vector<GPUVertexBufferLayout>& GetBufferLayout() const override;

  std::string GetShaderName() const override;

  void WriteVSFunctionsAndStructs(std::stringstream& ss) const override;

  void WriteVSUniforms(std::stringstream& ss) const override;

  void WriteVSInput(std::stringstream& ss) const override;

  void WriteVSMain(std::stringstream& ss) const override;

  void PrepareCMD(Command* cmd, HWDrawContext* context, const Matrix& transform,
                  float clip_depth, Command* stencil_cmd) override;

  std::optional<std::vector<std::string>> GetVarings() const override;

  std::string GetFSNameSuffix() const override;

  void WriteFSAlphaMask(std::stringstream& ss) const override;

 private:
  const Path& path_;
  const Paint& paint_;
  std::vector<GPUVertexBufferLayout> layout_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_PATH_GEOMETRY_HPP
