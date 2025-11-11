// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_HW_WGSL_SHADER_WRITER_HPP
#define SRC_RENDER_HW_DRAW_HW_WGSL_SHADER_WRITER_HPP

#include <sstream>
#include <string>
#include <vector>

#include "src/render/hw/draw/hw_wgsl_fragment.hpp"
#include "src/render/hw/draw/hw_wgsl_geometry.hpp"

namespace skity {

class HWWGSLShaderWriter {
 public:
  HWWGSLShaderWriter(const HWWGSLGeometry* geometry,
                     const HWWGSLFragment* fragment)
      : geometry_(geometry), fragment_(fragment) {}

  std::string GenVSSourceWGSL() const;
  std::string GenFSSourceWGSL() const;
  std::string GetVSShaderName() const;
  std::string GetFSShaderName() const;

 private:
  void WriteVSFunctionsAndStructs(std::stringstream& ss) const;
  void WriteVSUniforms(std::stringstream& ss) const;
  void WriteVSInput(std::stringstream& ss) const;
  void WriteVSOutput(std::stringstream& ss) const;
  void WriteVSAssgnShadingVarings(std::stringstream& ss) const;
  void WriteVSMain(std::stringstream& ss) const;

  void WriteFSFunctionsAndStructs(std::stringstream& ss) const;
  void WriteFSUniforms(std::stringstream& ss) const;
  void WriteFSInput(std::stringstream& ss) const;
  void WriteFSMain(std::stringstream& ss) const;

  void WriteVaryings(std::stringstream& ss) const;
  bool HasVarings() const;

 private:
  const HWWGSLGeometry* geometry_ = nullptr;
  const HWWGSLFragment* fragment_ = nullptr;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_HW_WGSL_SHADER_WRITER_HPP
