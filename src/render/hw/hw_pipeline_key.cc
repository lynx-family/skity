// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_pipeline_key.hpp"

#include <sstream>
#include <string>

namespace skity {
namespace {

std::string HWGeometryKeyTypeToName(HWGeometryKeyType::Value value) {
  switch (value) {
    case HWGeometryKeyType::kPath:
      return "Path";
    case HWGeometryKeyType::kPathAA:
      return "PathAA";
    case HWGeometryKeyType::kTessFill:
      return "TessPathFill";
    case HWGeometryKeyType::kTessStroke:
      return "TessPathStroke";
    case HWGeometryKeyType::kColorText:
      return "TextSolidColorVertexWGSL";
    case HWGeometryKeyType::kGradientText:
      return "TextGradientVertexWGSL";
    case HWGeometryKeyType::kRRect:
      return "RRect";
    case HWGeometryKeyType::kClip:
      return "Clip";
    case HWGeometryKeyType::kFilter:
      return "CommonFilterVertexWGSL";
    default:
      return "UnknownGeometry";
  }
}

std::string HWGeometrySubKeyTypeToName(HWGeometryKeyType::Value value) {
  switch (value) {
    case HWGeometryKeyType::kPathAA:
      return "AA";
    case HWGeometryKeyType::kRRect:
      return "RRect";
    default:
      return "UnknownGeometry";
  }
}

std::string CustomKeyToGradientName(uint32_t custom) {
  std::stringstream ss;
  ss << "Gradient";
  uint32_t type = custom & 0x07;
  uint32_t max_color_shift = (custom >> kMaxColorCountShift) & 0x07;
  uint32_t max_color_count = 1u << max_color_shift;
  bool offset_fast = (custom >> kOffsetFastShift) & 0x01;
  bool color_fast = (custom >> kColorFastShift) & 0x01;

  switch (type) {
    case kGradientTypeLinear:
      ss << "Linear";
      break;
    case kGradientTypeRadial:
      ss << "Radial";
      break;
    case kGradientTypeConical:
      ss << "Conical";
      break;
    case kGradientTypeSweep:
      ss << "Sweep";
      break;
    default:
      ss << "Unknown";
      break;
  }

  ss << max_color_count;
  if (offset_fast) {
    ss << "OffsetFast";
  }
  if (color_fast) {
    ss << "ColorFast";
  }
  return ss.str();
}

std::string HWFragmentKeyTypeToName(HWFragmentKeyType::Value value,
                                    uint32_t custom) {
  switch (value) {
    case HWFragmentKeyType::kSolid:
      return "SolidColor";
    case HWFragmentKeyType::kSolidVertex:
      return "SolidVertexColor";
    case HWFragmentKeyType::kGradient:
      return CustomKeyToGradientName(custom);
    case HWFragmentKeyType::kTexture:
      return "Texture";
    case HWFragmentKeyType::kStencil:
      return "StencilFragmentWGSL";
    case HWFragmentKeyType::kBlur:
      return "BlurFragmentWGSL";
    case HWFragmentKeyType::kColorText:
      return "ColorTextFragmentWGSL";
    case HWFragmentKeyType::kEmojiText:
      return std::string("ColorEmoji") +
             (custom > 0 ? "SwizzleRB" : "NoSwizzle") + "FragmentWGSL";
    case HWFragmentKeyType::kGradientText:
      return CustomKeyToGradientName(custom) + "TextWGSL";
    case HWFragmentKeyType::kSDFText:
      return "SdfColorTextFragmentWGSL";
    case HWFragmentKeyType::kTextureText:
      return "TextureText";
    case HWFragmentKeyType::kImageFilter:
      return "ImageFilterFragmentWGSL";
    default:
      return "UnknownFragment";
  }
}

std::string HWFragmentKeySubTypeToName(HWFragmentKeyType::Value value) {
  switch (value) {
    case HWFragmentKeyType::kSolidVertex:
      return "SolidVertexColor";
    case HWFragmentKeyType::kGradient:
      return "Gradient";
    case HWFragmentKeyType::kTexture:
      return "Texture";
    default:
      return "UnknownFragment";
  }
}

std::string HWColorFilterKeyTypeToName(HWColorFilterKeyType::Value value) {
  switch (value) {
    case HWColorFilterKeyType::kClear:
      return "BlendClearFilter";
    case HWColorFilterKeyType::kSrc:
      return "BlendSrcFilter";
    case HWColorFilterKeyType::kDst:
      return "BlendDstFilter";
    case HWColorFilterKeyType::kSrcOver:
      return "BlendSrcOverFilter";
    case HWColorFilterKeyType::kDstOver:
      return "BlendDstOverFilter";
    case HWColorFilterKeyType::kSrcIn:
      return "BlendSrcInFilter";
    case HWColorFilterKeyType::kDstIn:
      return "BlendDstInFilter";
    case HWColorFilterKeyType::kSrcOut:
      return "BlendSrcOutFilter";
    case HWColorFilterKeyType::kDstOut:
      return "BlendDstOutFilter";
    case HWColorFilterKeyType::kSrcATop:
      return "BlendSrcATopFilter";
    case HWColorFilterKeyType::kDstATop:
      return "BlendDstATopFilter";
    case HWColorFilterKeyType::kXor:
      return "BlendXorFilter";
    case HWColorFilterKeyType::kPlus:
      return "BlendPlusFilter";
    case HWColorFilterKeyType::kModulate:
      return "BlendModulateFilter";
    case HWColorFilterKeyType::kScreen:
      return "BlendScreenFilter";
    case HWColorFilterKeyType::kMatrix:
      return "MatrixFilter";
    case HWColorFilterKeyType::kSRGBToLinearGamma:
      return "SRGBToLinearGammaFilter";
    case HWColorFilterKeyType::kLinearToSRGBGamma:
      return "LinearToSRGBGammaFilter";
    case HWColorFilterKeyType::kCompose:
      return "ComposeFilter";
    default:
      return "UnknownColorFilter";
  }
}

}  // namespace

std::string VertexKeyToShaderName(HWFunctionBaseKey base_key) {
  HWFunctionBaseKey main = (base_key >> kMainKeyShift) & 0xFF;
  HWFunctionBaseKey sub = (base_key >> kSubKeyShift) & 0xFF;
  std::stringstream ss;
  ss << HWGeometryKeyTypeToName(static_cast<HWGeometryKeyType::Value>(main));
  if (sub > 0) {
    ss << "_"
       << HWFragmentKeySubTypeToName(
              static_cast<HWFragmentKeyType::Value>(sub));
  }
  return ss.str();
}

std::string FragmentKeyToShaderName(
    HWFunctionBaseKey base_key,
    std::optional<std::vector<uint32_t>> compose_keys) {
  HWFunctionBaseKey custom =
      (base_key >> kMainKeyShift >> kCustomKeyShift) & 0xFF;
  HWFunctionBaseKey main = (base_key >> kMainKeyShift) & 0xFF;
  std::stringstream ss;
  ss << HWFragmentKeyTypeToName(static_cast<HWFragmentKeyType::Value>(main),
                                custom);

  HWFunctionBaseKey sub = (base_key >> kSubKeyShift) & 0xFF;
  HWFunctionBaseKey filter = base_key & 0xFF;
  if (sub > 0) {
    ss << "_"
       << HWGeometrySubKeyTypeToName(
              static_cast<HWGeometryKeyType::Value>(sub));
  }
  if (filter > 0) {
    ss << "_"
       << HWColorFilterKeyTypeToName(
              static_cast<HWColorFilterKeyType::Value>(filter));
  }

  if (compose_keys.has_value()) {
    for (const auto& key : compose_keys.value()) {
      ss << "_"
         << HWColorFilterKeyTypeToName(
                static_cast<HWColorFilterKeyType::Value>(key));
    }
  }
  return ss.str();
}

std::string FunctionBaseKeyToShaderName(uint64_t base_key) {
  switch (static_cast<GPUShaderStage>(base_key >> 32)) {
    case GPUShaderStage::kVertex:
      return "VS_" + VertexKeyToShaderName(base_key & 0xFFFFFFFF);
    case GPUShaderStage::kFragment:
      return "FS_" +
             FragmentKeyToShaderName(base_key & 0xFFFFFFFF, std::nullopt);
    default:
      return "Unknown_" + std::to_string(base_key);
  }
}

}  // namespace skity
