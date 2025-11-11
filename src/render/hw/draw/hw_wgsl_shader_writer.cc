

#include "src/render/hw/draw/hw_wgsl_shader_writer.hpp"

#include <sstream>

#include "src/logging.hpp"

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
  if (HasVarings()) {
    ss << R"(
@fragment
fn fs_main(input: FSInput) -> @location(0) vec4<f32> {
  var color : vec4<f32>;
)";
  } else {
    ss << R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  var color : vec4<f32>;
)";
  }

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
  std::string name = "VS_" + geometry_->GetShaderName();
  if (fragment_ && fragment_->AffectsVertex()) {
    DEBUG_CHECK(fragment_->GetVSNameSuffix().size() > 0);
    name += +"_" + fragment_->GetVSNameSuffix();
  }
  return name;
}

std::string HWWGSLShaderWriter::GetFSShaderName() const {
  DEBUG_CHECK(fragment_);
  std::string name = "FS_" + fragment_->GetShaderName();
  if (geometry_ && geometry_->AffectsFragment()) {
    DEBUG_CHECK(geometry_->GetFSNameSuffix().size() > 0);
    name += "_" + geometry_->GetFSNameSuffix();
  }
  if (fragment_->GetFilter()) {
    name += "_" + fragment_->GetFilter()->GetShaderName();
  }
  return name;
}

}  // namespace skity
