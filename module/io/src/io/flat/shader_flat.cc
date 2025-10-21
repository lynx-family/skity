// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/effect/shader.hpp>
#include <skity/geometry/scalar.hpp>
#include <vector>

#include "src/io/flat/blender_flat.hpp"
#include "src/io/flat/local_matrix_flat.hpp"
#include "src/io/memory_read.hpp"
#include "src/picture_priv.hpp"

namespace skity {

namespace {

constexpr uint32_t kCustom_SkBlendMode = 0xFF;

// binary flattenable flags for gradient shader
enum GradientSerializationFlags {
  // Bits 29:31 used for various boolean flags
  kHasPosition_GSF = 0x80000000,
  kHasLegacyLocalMatrix_GSF = 0x40000000,
  kHasColorSpace_GSF = 0x20000000,

  // Bits 12:28 unused

  // Bits 8:11 for fTileMode
  kTileModeShift_GSF = 8,
  kTileModeMask_GSF = 0xF,

  // Bits 4:7 for fInterpolation.fColorSpace
  kInterpolationColorSpaceShift_GSF = 4,
  kInterpolationColorSpaceMask_GSF = 0xF,

  // Bits 1:3 for fInterpolation.fHueMethod
  kInterpolationHueMethodShift_GSF = 1,
  kInterpolationHueMethodMask_GSF = 0x7,

  // Bit 0 for fInterpolation.fInPremul
  kInterpolationInPremul_GSF = 0x1,
};

template <typename T>
bool validate_array(ReadBuffer& buffer, size_t count) {
  return buffer.ValidateCanReadN<T>(count);
}

bool ReadGradientInfo(ReadBuffer& buffer, Shader::GradientInfo& info) {
  auto flags = buffer.ReadU32();

  info.gradientFlags = (flags & kInterpolationInPremul_GSF);
  info.tile_mode =
      static_cast<TileMode>((flags >> kTileModeShift_GSF) & kTileModeMask_GSF);

  info.color_count = buffer.GetArrayCount();

  if (!validate_array<Color4f>(buffer, info.color_count)) {
    return false;
  }

  info.colors.resize(info.color_count);

  if (!buffer.ReadArrayN<Color4f>(info.colors.data(), info.color_count)) {
    return false;
  }

  if (flags & kHasColorSpace_GSF) {
    (void)buffer.ReadByteArrayAsData();  // skip color space
  }

  if (flags & kHasPosition_GSF) {
    info.color_offsets.resize(info.color_count);

    if (!validate_array<float>(buffer, info.color_count)) {
      return false;
    }

    if (!buffer.ReadArrayN<float>(info.color_offsets.data(),
                                  info.color_count)) {
      return false;
    }
  }

  if (flags & kHasLegacyLocalMatrix_GSF) {
    if (!buffer.IsVersionLT(Version::kNoShaderLocalMatrix)) {
      return false;
    }

    auto lm = buffer.ReadMatrix();

    if (!buffer.Validate(lm.has_value())) {
      return false;
    }
    info.local_matrix = lm.value();
  }

  return buffer.IsValid();
}

void SkipBlendShader(ReadBuffer& buffer) {
  (void)buffer.ReadShader();  // dst
  (void)buffer.ReadShader();  // src

  auto mode = buffer.ReadU32();

  if (mode == kCustom_SkBlendMode) {
    BlenderModeFlattenable::SkipReadBlender(buffer);
  }
}

void SkipColor4fShader(ReadBuffer& buffer) {
  (void)buffer.ReadColor4f();

  if (buffer.ReadBool()) {
    // skip color space
    // since we do not have color space, just skip the data followed
    (void)buffer.ReadByteArrayAsData();
  }
}

void SkipColorFilterShader(ReadBuffer& buffer) {
  (void)buffer.ReadShader();
  (void)buffer.ReadColorFilter();
}

void SkipCoordClampShader(ReadBuffer& buffer) {
  (void)buffer.ReadShader();

  (void)buffer.ReadRect();
}

void SkipPictureShader(ReadBuffer& buffer) {
  Matrix lm;

  if (buffer.IsVersionLT(Version::kNoShaderLocalMatrix)) {
    auto m = buffer.ReadMatrix();

    if (!buffer.Validate(m.has_value())) {
      return;
    }
    lm = m.value();
  }

  (void)buffer.ReadU32();  // tile mode x
  (void)buffer.ReadU32();  // tile mode y

  if (buffer.IsVersionLT(kNoFilterQualityShaders_Version)) {
    if (buffer.IsVersionLT(kPictureShaderFilterParam_Version)) {
      bool did_serialize = buffer.ReadBool();

      if (did_serialize) {
        SkipPictureInBuffer(buffer);
      }
    } else {
      auto legacy_filter = buffer.ReadU32();
      (void)legacy_filter;

      SkipPictureInBuffer(buffer);
    }
  } else {
    (void)buffer.ReadU32();  // filter

    SkipPictureInBuffer(buffer);
  }
}

std::shared_ptr<Shader> ReadConicalGradient(ReadBuffer& buffer) {
  Shader::GradientInfo info;

  if (!ReadGradientInfo(buffer, info)) {
    return {};
  }

  auto c1 = buffer.ReadPoint();
  auto c2 = buffer.ReadPoint();
  auto r1 = buffer.ReadFloat();
  auto r2 = buffer.ReadFloat();

  if (!buffer.IsValid()) {
    return {};
  }

  auto shader = Shader::MakeTwoPointConical(
      {
          c1.x,
          c1.y,
          0.f,
          1.f,
      },
      r1,
      {
          c2.x,
          c2.y,
          0.f,
          1.f,
      },
      r2, info.colors.data(), info.color_offsets.data(), info.color_count,
      info.tile_mode, info.gradientFlags);

  if (!info.local_matrix.IsIdentity()) {
    shader->SetLocalMatrix(info.local_matrix);
  }

  return shader;
}

std::shared_ptr<Shader> ReadLinearGradient(ReadBuffer& buffer) {
  Shader::GradientInfo info;

  if (!ReadGradientInfo(buffer, info)) {
    return {};
  }

  auto p1 = buffer.ReadPoint();
  auto p2 = buffer.ReadPoint();

  if (!buffer.IsValid()) {
    return {};
  }

  std::array<Point, 2> points = {
      Point{
          p1.x,
          p1.y,
          0.f,
          1.f,
      },
      Point{
          p2.x,
          p2.y,
          0.f,
          1.f,
      },
  };

  auto shader = Shader::MakeLinear(points.data(), info.colors.data(),
                                   info.color_offsets.data(), info.color_count,
                                   info.tile_mode, info.gradientFlags);

  if (!info.local_matrix.IsIdentity()) {
    shader->SetLocalMatrix(info.local_matrix);
  }

  return shader;
}

std::shared_ptr<Shader> ReadRadialGradient(ReadBuffer& buffer) {
  Shader::GradientInfo info;

  if (!ReadGradientInfo(buffer, info)) {
    return {};
  }

  auto center = buffer.ReadPoint();
  auto radius = buffer.ReadFloat();

  auto shader = Shader::MakeRadial(
      {
          center.x,
          center.y,
          0.f,
          1.f,
      },
      radius, info.colors.data(), info.color_offsets.data(), info.color_count,
      info.tile_mode, info.gradientFlags);

  if (!info.local_matrix.IsIdentity()) {
    shader->SetLocalMatrix(info.local_matrix);
  }

  return shader;
}

std::tuple<float, float> angles_from_t_coeff(float t_bias, float t_scale) {
  return {
      -t_bias * 360.f,
      (SkityIEEEFloatDivided(1.f, t_scale) - t_bias) * 360.f,
  };
}

std::shared_ptr<Shader> ReadSweepGradient(ReadBuffer& buffer) {
  Shader::GradientInfo info;

  if (!ReadGradientInfo(buffer, info)) {
    return {};
  }

  auto center = buffer.ReadPoint();
  auto t_bias = buffer.ReadFloat();
  auto t_scale = buffer.ReadFloat();

  auto [start_angle, end_angle] = angles_from_t_coeff(t_bias, t_scale);

  auto shader =
      Shader::MakeSweep(center.x, center.y, start_angle, end_angle,
                        info.colors.data(), info.color_offsets.data(),
                        info.color_count, info.tile_mode, info.gradientFlags);

  if (!info.local_matrix.IsIdentity()) {
    shader->SetLocalMatrix(info.local_matrix);
  }

  return shader;
}

void SkipPerlinNoiseShader(ReadBuffer& buffer) {
  (void)buffer.ReadU32();    // type
  (void)buffer.ReadFloat();  // freqX
  (void)buffer.ReadFloat();  // freqY
  (void)buffer.ReadU32();    // octaves

  (void)buffer.ReadFloat();  // seed
  (void)buffer.ReadInt();    // width;
  (void)buffer.ReadInt();    // height;
}

std::shared_ptr<Shader> ReadImageShader(ReadBuffer& buffer) {
  auto tmx = static_cast<TileMode>(buffer.ReadU32());
  auto tmy = static_cast<TileMode>(buffer.ReadU32());

  SamplingOptions sampling{};

  bool read_sampling = true;
  if (buffer.IsVersionLT(Version::kNoFilterQualityShaders_Version) &&
      !buffer.ReadBool()) {
    read_sampling = false;
  }

  if (read_sampling) {
    sampling = buffer.ReadSamplingOptions();
  }

  Matrix local_matrix;
  if (buffer.IsVersionLT(Version::kNoShaderLocalMatrix)) {
    auto m = buffer.ReadMatrix();

    if (!buffer.Validate(m.has_value())) {
      return {};
    }

    local_matrix = m.value();
  }

  auto image = buffer.ReadImage();

  if (image == nullptr) {
    // maybe use some image format we don't support
    return {};
  }

  // skip unused data
  buffer.IsVersionLT(Version::kRawImageShaders) ? false : buffer.ReadBool();

  return Shader::MakeShader(image, sampling, tmx, tmy, local_matrix);
}

void SkipColorShader(ReadBuffer& buffer) {
  if (buffer.IsVersionLT(Version::kCombineColorShaders)) {
    (void)buffer.ReadColor();
  } else {
    (void)buffer.ReadColor4f();
  }
}

void SkipRuntimeShader(ReadBuffer& buffer) {
  bool is_stable_effect = false;
  if (!buffer.IsVersionLT(Version::kSerializeStableKeys)) {
    auto candidate = buffer.ReadU32();  // skip candidate stable key

    is_stable_effect = (candidate > 500 && candidate <= 528);

    if (!is_stable_effect) {
      return;
    }
  }

  std::string sksl;

  buffer.ReadString(sksl);

  if (sksl.empty()) {
    return;
  }

  (void)buffer.ReadByteArrayAsData();  // skip uniforms

  if (buffer.IsVersionLT(Version::kNoShaderLocalMatrix)) {
    auto flags = buffer.ReadU32();

    static constexpr uint32_t kHasLegacyLocalMatrix_Flag = 1 << 1;

    if (flags & kHasLegacyLocalMatrix_Flag) {
      (void)buffer.ReadMatrix();
    }
  }

  auto child_count = buffer.ReadU32();

  for (uint32_t i = 0; i < child_count; i++) {
    (void)buffer.ReadRawFlattenable();
  }
}

void SkipWorkingColorSpaceShader(ReadBuffer& buffer) {
  (void)buffer.ReadShader();

  bool legacy_working_cs =
      buffer.IsVersionLT(Version::kWorkingColorSpaceOutput);

  bool work_in_unpremul = !legacy_working_cs && buffer.ReadBool();

  // input color space
  if (legacy_working_cs || buffer.ReadBool()) {
    (void)buffer.ReadByteArrayAsData();

    if (!buffer.IsValid()) {
      return;
    }
  }

  // output color space
  if (!legacy_working_cs && buffer.ReadBool()) {
    (void)buffer.ReadByteArrayAsData();

    if (!buffer.IsValid()) {
      return;
    }
  }
}

}  // namespace

std::shared_ptr<Flattenable> ReadShaderFromMemory(const std::string& factory,
                                                  ReadBuffer& buffer) {
  if (factory == "SkBlendShader" || factory == "SkShader_Blend") {
    SkipBlendShader(buffer);
  } else if (factory == "SkColor4fShader") {
    SkipColor4fShader(buffer);
  } else if (factory == "SkColorFilterShader") {
    SkipColorFilterShader(buffer);
  } else if (factory == "SkColorShader") {
    SkipColorShader(buffer);
  } else if (factory == "SkCoordClampShader" ||
             factory == "SkShader_CoordClamp") {
    SkipCoordClampShader(buffer);
  } else if (factory == "SkEmptyShader") {
    // empty shader, just skip
  } else if (factory == "SkLocalMatrixShader") {
    return LocalMatrixFlat::ReadFromBuffer(buffer);
  } else if (factory == "SkPictureShader") {
    SkipPictureShader(buffer);
  } else if (factory == "SkConicalGradient" ||
             factory == "SkTwoPointConicalGradient") {
    return ReadConicalGradient(buffer);
  } else if (factory == "SkLinearGradient") {
    return ReadLinearGradient(buffer);
  } else if (factory == "SkRadialGradient") {
    return ReadRadialGradient(buffer);
  } else if (factory == "SkSweepGradient") {
    return ReadSweepGradient(buffer);
  } else if (factory == "SkPerlinNoiseShader" ||
             factory == "SkPerlinNoiseShaderImpl") {
    SkipPerlinNoiseShader(buffer);
  } else if (factory == "SkImageShader") {
    return ReadImageShader(buffer);
  } else if (factory == "SkRuntimeShader" || factory == "SkRTShader") {
    SkipRuntimeShader(buffer);
  } else if (factory == "SkWorkingColorSpaceShader") {
    SkipWorkingColorSpaceShader(buffer);
  }

  return {};
}

FactoryProc GetShaderFactoryProc(const std::string& factory_name) {
  static std::vector<std::string> kShaderFactoryNames = {
      "SkBlendShader",
      "SkShader_Blend",
      "SkColor4Shader",
      "SkColorFilterShader",
      "SkColorShader",
      "SkCoordClampShader",
      "SkShader_CoordClamp",
      "SkEmptyShader",
      "SkLocalMatrixShader",
      "SkPictureShader",
      "SkConicalGradient",
      "SkTwoPointConicalGradient",
      "SkLinearGradient",
      "SkRadialGradient",
      "SkSweepGradient",
      "SkPerlinNoiseShader",
      "SkPerlinNoiseShaderImpl",
      "SkImageShader",
      "SkRuntimeShader",
      "SkRTShader",
      "SkWorkingColorSpaceShader",
  };

  if (std::find(kShaderFactoryNames.begin(), kShaderFactoryNames.end(),
                factory_name) != kShaderFactoryNames.end()) {
    return ReadShaderFromMemory;
  }

  return nullptr;
}

}  // namespace skity
