// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_COVERAGE_AA_TILE_GEOMETRY_HPP
#define SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_COVERAGE_AA_TILE_GEOMETRY_HPP

#include <cstddef>

#include "src/render/hw/coverage/coverage_aa_frame_data.hpp"
#include "src/render/hw/draw/hw_wgsl_geometry.hpp"

namespace skity {

class WGSLCoverageAATileGeometry : public HWWGSLGeometry {
 public:
  WGSLCoverageAATileGeometry(const CoverageAAFrameData* frame_data,
                             size_t tiled_path_offset, size_t tiled_path_count,
                             Matrix physical_to_layer = Matrix{});

  ~WGSLCoverageAATileGeometry() override = default;

  static std::vector<GPUVertexBufferLayout> GetBufferLayout();

  HWFunctionBaseKey GetMainKey() const override;

  HWFunctionBaseKey GetFSSubKey() const override;

  void WriteVSFunctionsAndStructs(std::stringstream& ss) const override;

  void WriteVSUniforms(std::stringstream& ss) const override;

  void WriteVSInput(std::stringstream& ss) const override;

  void WriteVSMain(std::stringstream& ss) const override;

  std::optional<std::vector<std::string>> GetVarings() const override;

  void WriteFSFunctionsAndStructs(std::stringstream& ss) const override;

  void WriteFSUniforms(std::stringstream& ss) const override;

  void WriteFSAlphaMask(std::stringstream& ss) const override;

  void PrepareCMD(Command* cmd, HWDrawContext* context, const Matrix& transform,
                  float clip_depth, Command* stencil_cmd) override;

 private:
  const CoverageAAFrameData* frame_data_ = nullptr;
  size_t tiled_path_offset_ = 0;
  size_t tiled_path_count_ = 0;
  Matrix physical_to_layer_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_COVERAGE_AA_TILE_GEOMETRY_HPP
