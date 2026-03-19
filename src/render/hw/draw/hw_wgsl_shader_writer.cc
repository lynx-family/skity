// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/hw_wgsl_shader_writer.hpp"

#include <sstream>

#include "src/logging.hpp"
#include "src/render/hw/hw_pipeline_key.hpp"

namespace skity {

std::string HWWGSLShaderWriter::GenVSSourceWGSL() const {
  std::stringstream ss;
  WriteVSFunctionsAndStructs(ss);
  WriteVSUniforms(ss);
  WriteVSInput(ss);
  WriteVSOutput(ss);
  WriteVSMain(ss);
  return ss.str();
}

std::string HWWGSLShaderWriter::GenFSSourceWGSL() const {
  std::stringstream ss;
  WriteFSFunctionsAndStructs(ss);
  WriteFSUniforms(ss);
  WriteFSInput(ss);
  WriteFSMain(ss);
  return ss.str();
}

void HWWGSLShaderWriter::WriteVSFunctionsAndStructs(
    std::stringstream& ss) const {
  DEBUG_CHECK(geometry_);
  geometry_->WriteVSFunctionsAndStructs(ss);
  if (fragment_ && fragment_->AffectsVertex()) {
    fragment_->WriteVSFunctionsAndStructs(ss);
  }
}

void HWWGSLShaderWriter::WriteVSUniforms(std::stringstream& ss) const {
  DEBUG_CHECK(geometry_);
  geometry_->WriteVSUniforms(ss);
  if (fragment_ && fragment_->AffectsVertex()) {
    fragment_->WriteVSUniforms(ss);
  }
}

void HWWGSLShaderWriter::WriteVSInput(std::stringstream& ss) const {
  DEBUG_CHECK(geometry_);
  geometry_->WriteVSInput(ss);
}

void HWWGSLShaderWriter::WriteVSOutput(std::stringstream& ss) const {
  DEBUG_CHECK(geometry_);
  ss << R"(
struct VSOutput {
  @builtin(position) pos: vec4<f32>,
)";
  WriteVaryings(ss);
  ss << R"(
};
)";
}

void HWWGSLShaderWriter::WriteVSMain(std::stringstream& ss) const {
  DEBUG_CHECK(geometry_);
  ss << R"(
@vertex
fn vs_main(input: VSInput) -> VSOutput {
  var output: VSOutput;
  var local_pos: vec2<f32>;
)";
  geometry_->WriteVSMain(ss);
  WriteVSAssgnShadingVarings(ss);
  ss << R"(
  return output;
};
)";
}

void HWWGSLShaderWriter::WriteVSAssgnShadingVarings(
    std::stringstream& ss) const {
  if (fragment_ && fragment_->AffectsVertex()) {
    fragment_->WriteVSAssgnShadingVarings(ss);
  }
}

void HWWGSLShaderWriter::WriteFSFunctionsAndStructs(
    std::stringstream& ss) const {
  DEBUG_CHECK(fragment_);
  fragment_->WriteFSFunctionsAndStructs(ss);
  if (geometry_ && geometry_->AffectsFragment()) {
    geometry_->WriteFSFunctionsAndStructs(ss);
  }
  if (fragment_->GetFilter()) {
    ss << fragment_->GetFilter()->GenSourceWGSL();
  }
  if (fragment_->GetProgrammableBlending()) {
    ss << fragment_->GetProgrammableBlending()->GenSourceWGSL();
  }
}

void HWWGSLShaderWriter::WriteFSUniforms(std::stringstream& ss) const {
  DEBUG_CHECK(fragment_);
  fragment_->WriteFSUniforms(ss);
}

void HWWGSLShaderWriter::WriteFSInput(std::stringstream& ss) const {
  DEBUG_CHECK(fragment_);
  if (!HasVarings()) {
    return;
  }
  ss << R"(
struct FSInput {
)";
  WriteVaryings(ss);
  ss << R"(
};
)";
}

void HWWGSLShaderWriter::WriteFSMain(std::stringstream& ss) const {
  DEBUG_CHECK(fragment_);
  ss << "@fragment\n";
  ss << "fn fs_main(";
  std::vector<std::string> fs_params;
  if (HasVarings()) {
    fs_params.push_back("input: FSInput");
  }
  if (NeedsFramebufferFetch()) {
    fs_params.push_back("@color(0) dst_color: vec4<f32>");
  }
  if (!fs_params.empty()) {
    ss << fs_params[0];
    for (size_t i = 1; i < fs_params.size(); i++) {
      ss << ", " << fs_params[i];
    }
  }
  ss << ") -> @location(0) vec4<f32> {\n";
  ss << "  var color : vec4<f32>;\n";

  fragment_->WriteFSMain(ss);
  if (fragment_->GetFilter()) {
    ss << R"(
  color = filter_color(color);
)";
  }

  if (geometry_ && geometry_->AffectsFragment()) {
    ss << R"(
  var mask_alpha: f32 = 1.0;
)";
    geometry_->WriteFSAlphaMask(ss);
    ss << R"(
  color = color * mask_alpha;
)";
  }

  if (NeedsFramebufferFetch()) {
    ss << R"(
  color = blending(color, dst_color);
)";
  }

  ss << R"(
  return color;
}
)";
}

void HWWGSLShaderWriter::WriteVaryings(std::stringstream& ss) const {
  uint32_t i = 0;
  if (fragment_ && fragment_->GetVarings().has_value()) {
    const auto fs_varyings = fragment_->GetVarings().value();
    for (const auto& varying : fs_varyings) {
      // all varyings provided by fragment must start with the prefix 'f_'.
      DEBUG_CHECK(varying.compare(0, 2, "f_") == 0);
      ss << "  @location(" << i << ") " << varying << ",\n";
      i++;
    }
  }
  if (geometry_ && geometry_->GetVarings().has_value()) {
    const auto vs_varyings = geometry_->GetVarings().value();
    for (auto& varying : vs_varyings) {
      // all varyings provided by geometry must start with the prefix 'v_'.
      DEBUG_CHECK(varying.compare(0, 2, "v_") == 0);
      ss << "  @location(" << i << ") " << varying << ",\n";
      i++;
    }
  }
}

bool HWWGSLShaderWriter::HasVarings() const {
  size_t varyings_count = 0;
  if (geometry_ && geometry_->GetVarings().has_value()) {
    varyings_count += geometry_->GetVarings().value().size();
  }

  if (fragment_ && fragment_->GetVarings().has_value()) {
    varyings_count += fragment_->GetVarings().value().size();
  }
  return varyings_count > 0;
}

std::string HWWGSLShaderWriter::GetVSShaderName() const {
  DEBUG_CHECK(geometry_);
  return "VS_" + VertexKeyToShaderName(GetVSKey());
}

HWFunctionBaseKey HWWGSLShaderWriter::GetVSKey() const {
  HWFunctionBaseKey main_key = geometry_->GetMainKey();
  HWFunctionBaseKey sub_key = 0;
  if (fragment_ && fragment_->AffectsVertex()) {
    sub_key = fragment_->GetVSSubKey();
  }
  return MakeFunctionBaseKey(main_key, sub_key, 0);
}

std::string HWWGSLShaderWriter::GetFSShaderName() const {
  DEBUG_CHECK(fragment_);
  auto fs_key = GetFSKey();
  std::string name =
      "FS_" + FragmentKeyToShaderName(fs_key, GetComposeKeys(fs_key));

  return name;
}

HWFunctionBaseKey HWWGSLShaderWriter::GetFSKey() const {
  DEBUG_CHECK(fragment_);
  HWFunctionBaseKey main_key = fragment_->GetMainKey();
  HWFunctionBaseKey sub_key = 0;
  HWFunctionBaseKey filter_key = 0;
  if (geometry_ && geometry_->AffectsFragment()) {
    sub_key = geometry_->GetFSSubKey();
  }
  if (fragment_->GetFilter()) {
    filter_key = fragment_->GetFilter()->GetType();
  }
  return MakeFunctionBaseKey(main_key, sub_key, filter_key);
}

}  // namespace skity
