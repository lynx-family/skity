// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <array>
#include <cstring>
#include <skity/effect/color_filter.hpp>

#include "src/io/memory_read.hpp"
#include "src/picture_priv.hpp"

namespace skity {

namespace {

std::shared_ptr<ColorFilter> ReadMatrixColorFilter(ReadBuffer& buffer) {
  std::array<float, 20> matrix{};

  if (!buffer.ReadArrayN<float>(matrix.data(), matrix.size())) {
    return {};
  }

  (void)buffer.ReadBool();  // is_rgba;
  if (!buffer.IsVersionLT(Version::kUnclampedMatrixColorFilter)) {
    (void)buffer.ReadBool();  // clamp
  }

  return ColorFilters::Matrix(matrix.data());
}

std::shared_ptr<ColorFilter> ReadComposeColorFilter(ReadBuffer& buffer) {
  auto outer = buffer.ReadColorFilter();
  auto inner = buffer.ReadColorFilter();

  return outer ? ColorFilters::Compose(outer, inner) : inner;
}

std::shared_ptr<ColorFilter> ReadBlendModeColorFilter(ReadBuffer& buffer) {
  if (buffer.IsVersionLT(Version::kBlend4fColorFilter)) {
    auto color = buffer.ReadColor();

    auto mode = static_cast<BlendMode>(buffer.ReadU32());

    return ColorFilters::Blend(color, mode);
  } else {
    auto color = buffer.ReadColor4f();

    auto mode = static_cast<BlendMode>(buffer.ReadU32());

    return ColorFilters::Blend(Color4fToColor(color), mode);
  }
}

#define FixedToFloat(x) ((x) * 1.52587890625e-5f)

std::shared_ptr<ColorFilter> ReadColorSpaceXformColorFilter(
    ReadBuffer& buffer) {
  // src
  auto data = buffer.ReadByteArrayAsData();

  if (!data) {
    return {};
  }

  auto ptr = static_cast<const uint8_t*>(data->RawData());

  if (ptr[0] != 1) {  // bad version
    return {};
  }

  ptr += 4;  // skip version header

  std::array<float, 7> src_transfn{};
  std::array<float, 9> src_named_gamut{};

  std::memcpy(src_transfn.data(), ptr, src_transfn.size() * sizeof(float));
  ptr += src_transfn.size() * sizeof(float);

  std::memcpy(src_named_gamut.data(), ptr,
              src_named_gamut.size() * sizeof(float));
  ptr += src_named_gamut.size() * sizeof(float);

  // dst
  data = buffer.ReadByteArrayAsData();

  if (!data) {
    return {};
  }

  ptr = static_cast<const uint8_t*>(data->RawData());

  if (ptr[0] != 1) {  // bad version
    return {};
  }

  ptr += 4;  // skip version header

  std::array<float, 7> dst_transfn{};
  std::array<float, 9> dst_named_gamut{};

  std::memcpy(dst_transfn.data(), ptr, dst_transfn.size() * sizeof(float));
  ptr += dst_transfn.size() * sizeof(float);

  std::memcpy(dst_named_gamut.data(), ptr,
              dst_named_gamut.size() * sizeof(float));
  ptr += dst_named_gamut.size() * sizeof(float);

  constexpr std::array<float, 9> kSRGBNamedGamut = {
      FixedToFloat(0x6FA2), FixedToFloat(0x6299), FixedToFloat(0x24A0),
      FixedToFloat(0x38F5), FixedToFloat(0xB785), FixedToFloat(0x0F84),
      FixedToFloat(0x0390), FixedToFloat(0x18DA), FixedToFloat(0xB6CF),
  };

  constexpr std::array<float, 7> kTransferLINEAR = {
      1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
  };

  constexpr std::array<float, 7> kTransferSRGB = {
      2.4f,
      static_cast<float>(1 / 1.055),
      static_cast<float>(0.055 / 1.055),
      static_cast<float>(1 / 12.92),
      0.04045f,
      0.0f,
      0.0f,
  };

  if (src_named_gamut != kSRGBNamedGamut ||
      dst_named_gamut != kSRGBNamedGamut) {
    // we only support srgb named gamut
    return {};
  }

  if (src_transfn == kTransferLINEAR && dst_transfn == kTransferSRGB) {
    return ColorFilters::LinearToSRGBGamma();
  } else if (src_transfn == kTransferSRGB && dst_transfn == kTransferLINEAR) {
    return ColorFilters::SRGBToLinearGamma();
  }

  // other transfer function is not supported

  return {};
}

std::shared_ptr<ColorFilter> ReadLegacyGammaColorFilter(ReadBuffer& buffer) {
  auto dir = buffer.ReadU32();

  if (!buffer.Validate(dir <= 1)) {
    return {};
  }

  return dir == 0 ? ColorFilters::LinearToSRGBGamma()
                  : ColorFilters::SRGBToLinearGamma();
}

void SkipWorkingFormatColorFilter(ReadBuffer& buffer) {
  (void)buffer.ReadColorFilter();

  bool use_dst_tf = buffer.ReadBool();
  bool use_dst_gamut = buffer.ReadBool();
  bool used_dst_at = buffer.ReadBool();

  if (!use_dst_tf) {
    std::array<float, 7> tmp_buffer{};
    buffer.ReadArrayN<float>(tmp_buffer.data(), tmp_buffer.size());
  }

  if (!use_dst_gamut) {
    std::array<float, 9> tmp_buffer{};
    buffer.ReadArrayN<float>(tmp_buffer.data(), tmp_buffer.size());
  }

  if (!used_dst_at) {
    (void)buffer.ReadU32();
  }
}

void SkipTableColorFilter(ReadBuffer& buffer) {
  // just skip the color table
  buffer.Skip(4 * 256);
}

void SkipRuntimeColorFilter(ReadBuffer& buffer) {
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

  (void)buffer.ReadByteArrayAsData();  // skip uniforms

  // skip children

  auto count = buffer.ReadU32();

  for (uint32_t i = 0; i < count; i++) {
    (void)buffer.ReadRawFlattenable();
  }
}

}  // namespace

std::shared_ptr<Flattenable> ReadColorFilterFromMemory(
    const std::string& factory, ReadBuffer& buffer) {
  if (factory == "SkMatrixColorFilter" || factory == "SkColorFilter_Matrix") {
    return ReadMatrixColorFilter(buffer);
  } else if (factory == "SkComposeColorFilter") {
    return ReadComposeColorFilter(buffer);
  } else if (factory == "SkBlendModeColorFilter" ||
             factory == "SkModeColorFilter") {
    return ReadBlendModeColorFilter(buffer);
  } else if (factory == "SkColorSpaceXformColorFilter" ||
             factory == "ColorSpaceXformColorFilter") {
    return ReadColorSpaceXformColorFilter(buffer);
  } else if (factory == "SkSRGBGammaColorFilter") {
    return ReadLegacyGammaColorFilter(buffer);
  } else if (factory == "SkWorkingFormatColorFilter") {
    SkipWorkingFormatColorFilter(buffer);
  } else if (factory == "SkTableColorFilter" ||
             factory == "SkTable_ColorFilter") {
    SkipTableColorFilter(buffer);
  } else if (factory == "SkRuntimeColorFilter") {
    SkipRuntimeColorFilter(buffer);
  }

  return {};
}

FactoryProc GetColorFilterFactoryProc(const std::string& factory_name) {
  static std::vector<std::string> kMaskFilterFactories = {
      "SkMatrixColorFilter",        "SkColorFilter_Matrix",
      "SkComposeColorFilter",       "SkBlendModeColorFilter",
      "SkModeColorFilter",          "SkColorSpaceXformColorFilter",
      "ColorSpaceXformColorFilter", "SkSRGBGammaColorFilter",
      "SkWorkingFormatColorFilter", "SkTableColorFilter",
      "SkTable_ColorFilter",        "SkRuntimeColorFilter",
  };

  auto it = std::find(kMaskFilterFactories.begin(), kMaskFilterFactories.end(),
                      factory_name);
  if (it == kMaskFilterFactories.end()) {
    return nullptr;
  }

  return ReadColorFilterFromMemory;
}

}  // namespace skity
