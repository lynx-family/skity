// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_HW_HW_PIPELINE_KEY_HPP
#define SRC_RENDER_HW_DRAW_HW_HW_PIPELINE_KEY_HPP

#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "src/gpu/gpu_shader_function.hpp"
#include "src/logging.hpp"

namespace skity {

struct HWGeometryKeyType {
  enum Value : uint32_t {
    kPath = 1,
    kPathAA = 2,
    kTessFill = 3,
    kTessStroke = 4,
    kColorText = 5,
    kGradientText = 6,
    kRRect = 7,
    kClip = 8,
    kFilter = 9,
    kLast = kFilter,
  };
};
static_assert(HWGeometryKeyType::Value::kLast < 0xFF);

struct HWFragmentKeyType {
  enum Value : uint32_t {
    kSolid = 1,
    kSolidVertex = 2,
    kGradient = 3,
    kTexture = 4,
    kStencil = 5,
    kBlur = 6,
    kColorText = 7,
    kEmojiText = 8,
    kGradientText = 9,
    kSDFText = 10,
    kTextureText = 11,
    kImageFilter = 12,
    kLast = kImageFilter,
  };
};
static_assert(HWFragmentKeyType::Value::kLast < 0xFF);

struct HWColorFilterKeyType {
  enum Value : uint32_t {
    kUnknown = 0,
    kClear = 1,
    kSrc = 2,
    kDst = 3,
    kSrcOver = 4,
    kDstOver = 5,
    kSrcIn = 6,
    kDstIn = 7,
    kSrcOut = 8,
    kDstOut = 9,
    kSrcATop = 10,
    kDstATop = 11,
    kXor = 12,
    kPlus = 13,
    kModulate = 14,
    kScreen = 15,
    kMatrix = 16,
    kLinearToSRGBGamma = 17,
    kSRGBToLinearGamma = 18,
    kCompose = 0xFF,
  };
};
constexpr uint32_t kFilterKeyMask = 0xFF;

using HWPipelineBaseKey = uint64_t;
using HWFunctionBaseKey = uint32_t;

// HWFunctionBaseKey is a 32-bit key that consists of 3 parts:
// 1. Main: 16 bits
// 2. Sub: 8 bits
// 3. Filter: 8 bits
constexpr uint32_t kMainKeyShift = 16;
constexpr uint32_t kSubKeyShift = 8;
constexpr HWFunctionBaseKey MakeFunctionBaseKey(HWFunctionBaseKey main,
                                                HWFunctionBaseKey sub = 0,
                                                HWFunctionBaseKey filter = 0) {
  return (main << kMainKeyShift) | (sub << kSubKeyShift) | filter;
}

// Main Key is a 16-bit key that consists of 2 parts:
// 1. Custom: 8 bits
// 2. Main: 8 bits
constexpr uint32_t kCustomKeyShift = 8;
constexpr HWFunctionBaseKey MakeMainKey(HWFunctionBaseKey main,
                                        HWFunctionBaseKey custom) {
  return custom << kCustomKeyShift | main;
}

// Pipeline Key is a 64-bit key that consists of 2 parts:
// 1. Vertex: 32 bits
// 2. Fragment: 32 bits
constexpr uint32_t kVertexKeyShift = 32;
constexpr HWPipelineBaseKey MakePipelineBaseKey(
    HWFunctionBaseKey vertex_key, HWFunctionBaseKey fragment_key) {
  return (static_cast<uint64_t>(vertex_key) << kVertexKeyShift) |
         static_cast<uint64_t>(fragment_key);
}

struct HWPipelineKey;
using HWFunctionKey = HWPipelineKey;

struct HWPipelineKey {
  uint64_t base_key;
  std::optional<std::vector<uint32_t>> compose_keys;

  bool operator==(const HWPipelineKey& other) const {
    return base_key == other.base_key && compose_keys == other.compose_keys;
  }

  bool operator!=(const HWPipelineKey& other) const {
    return !(*this == other);
  }

  constexpr HWFunctionBaseKey GetVertexBaseKey() const {
    return base_key >> kVertexKeyShift;
  }

  constexpr HWFunctionBaseKey GetFragmentBaseKey() const {
    return base_key & 0xFFFFFFFF;
  }

  HWFunctionKey GetFunctionKey(GPUShaderStage stage) const {
    HWFunctionKey key;
    switch (stage) {
      case GPUShaderStage::kVertex:
        key.base_key = GetVertexBaseKey();
        break;
      case GPUShaderStage::kFragment:
        key.base_key = GetFragmentBaseKey();
        key.compose_keys = compose_keys;
        break;
    }
    // Add stage to base key to make sure vertex and fragment key are
    // different.
    key.base_key |= (static_cast<uint64_t>(stage) << 32);
    return key;
  }
};

struct HWPipelineKeyHash {
  std::size_t operator()(const HWPipelineKey& key) const {
    size_t res = 17;
    res += std::hash<uint64_t>()(key.base_key);
    if (key.compose_keys.has_value()) {
      for (uint32_t value : key.compose_keys.value()) {
        res += std::hash<uint32_t>()(value);
      }
    }
    return res;
  }
};
using HWFunctionKeyHash = HWPipelineKeyHash;

constexpr static uint32_t kGradientTypeShift = 0;
constexpr static uint32_t kMaxColorCountShift = 3;
constexpr static uint32_t kOffsetFastShift = 6;
constexpr static uint32_t kColorFastShift = 7;

constexpr static uint32_t kGradientTypeLinear = 1;
constexpr static uint32_t kGradientTypeRadial = 2;
constexpr static uint32_t kGradientTypeConical = 3;
constexpr static uint32_t kGradientTypeSweep = 4;

std::string FunctionBaseKeyToShaderName(uint64_t base_key);

std::string VertexKeyToShaderName(uint32_t base_key);

std::string FragmentKeyToShaderName(
    uint32_t base_key, std::optional<std::vector<uint32_t>> compose_keys);

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_HW_HW_PIPELINE_KEY_HPP
