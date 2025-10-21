// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/effect/mask_filter.hpp>

#include "src/io/memory_read.hpp"

namespace skity {

namespace {

struct Light {
  float direction[3];
  uint16_t pad;
  uint8_t ambient;
  uint8_t specular;
};

void SkipEmbossMaskFilter(ReadBuffer& buffer) {
  Light light;

  buffer.ReadArrayN<uint8_t>(&light, sizeof(Light));
}

float sigma_to_radius(float sigma) {
  return sigma > 0.5f ? (sigma - 0.5f) / 0.57735f : 0.0f;
}

std::shared_ptr<MaskFilter> ReadBlurMaskFilterImpl(ReadBuffer& buffer) {
  auto sigma = buffer.ReadFloat();
  auto style = static_cast<BlurStyle>(buffer.ReadU32());

  uint32_t flags = buffer.ReadU32() & 0x3;

  return MaskFilter::MakeBlur(style, sigma_to_radius(sigma));
}

void SkipShaderMaskFilter(ReadBuffer& buffer) { (void)buffer.ReadShader(); }

}  // namespace

std::shared_ptr<Flattenable> ReadMaskFilterFromMemory(
    const std::string& factory, ReadBuffer& buffer) {
  if (factory == "SkEmbossMaskFilter") {
    SkipEmbossMaskFilter(buffer);
  } else if (factory == "SkBlurMaskFilterImpl") {
    return ReadBlurMaskFilterImpl(buffer);
  } else if (factory == "SkShaderMaskFilterImpl" || factory == "SkShaderMF") {
  }

  return {};
}

FactoryProc GetMaskFilterFactoryProc(const std::string& factory_name) {
  static std::vector<std::string> kMaskFilterFactories = {
      "SkEmbossMaskFilter",
      "SkBlurMaskFilterImpl",
      "SkShaderMaskFilterImpl",
      "SkShaderMF",
  };

  auto it = std::find(kMaskFilterFactories.begin(), kMaskFilterFactories.end(),
                      factory_name);
  if (it == kMaskFilterFactories.end()) {
    return nullptr;
  }

  return ReadMaskFilterFromMemory;
}

}  // namespace skity
