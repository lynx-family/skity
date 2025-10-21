// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/effect/path_effect.hpp>
#include <vector>

#include "src/io/memory_read.hpp"

namespace skity {

namespace {

void SkipCornerPathEffect(ReadBuffer &buffer) { (void)buffer.ReadFloat(); }

std::shared_ptr<PathEffect> ReadDiscretePathEffect(ReadBuffer &buffer) {
  auto seg_length = buffer.ReadFloat();
  auto perterb = buffer.ReadFloat();
  auto seed = buffer.ReadU32();

  return PathEffect::MakeDiscretePathEffect(seg_length, perterb, seed);
}

std::shared_ptr<PathEffect> ReadDashPathEffect(ReadBuffer &buffer) {
  float phase = buffer.ReadFloat();

  auto count = buffer.GetArrayCount();

  if (!buffer.ValidateCanReadN<float>(count)) {
    return {};
  }

  std::vector<float> intervals(count);

  if (!buffer.ReadArrayN<float>(intervals.data(), count)) {
    return {};
  }

  return PathEffect::MakeDashPathEffect(intervals.data(), count, phase);
}

void SkipLine2DPathEffect(ReadBuffer &buffer) {
  (void)buffer.ReadMatrix();
  (void)buffer.ReadFloat();  // width
}

void SkipPath2DPathEffect(ReadBuffer &buffer) {
  (void)buffer.ReadMatrix();
  (void)buffer.ReadPath();
}

void SkipPath1DPathEffect(ReadBuffer &buffer) {}

void SkipComposePathEffect(ReadBuffer &buffer) {}

void SkipSumPathEffect(ReadBuffer &buffer) {}

}  // namespace

std::shared_ptr<Flattenable> ReadPathEffectFromMemory(
    const std::string &factory, ReadBuffer &buffer) {
  if (factory == "SkCornerPathEffect") {
    SkipCornerPathEffect(buffer);
  } else if (factory == "SkDashImpl") {
    return ReadDashPathEffect(buffer);
  } else if (factory == "SkDiscretePathEffect") {
    return ReadDiscretePathEffect(buffer);
  } else if (factory == "SkLine2DPathEffectImpl") {
    SkipLine2DPathEffect(buffer);
  } else if (factory == "SkPath2DPathEffectImpl") {
    SkipPath2DPathEffect(buffer);
  } else if (factory == "SkPath1DPathEffectImpl") {
    SkipPath1DPathEffect(buffer);
  } else if (factory == "SkComposePathEffect") {
    SkipComposePathEffect(buffer);
  } else if (factory == "SkSumPathEffect") {
    SkipSumPathEffect(buffer);
  }

  return {};
}

FactoryProc GetPathEffectFactoryProc(const std::string &factory_name) {
  static std::vector<std::string> path_effect_factory_names = {
      "SkCornerPathEffect",     "SkDashImpl",
      "SkDiscretePathEffect",   "SkLine2DPathEffectImpl",
      "SkPath2DPathEffectImpl", "SkPath1DPathEffectImpl",
      "SkComposePathEffect",    "SkSumPathEffect"};

  if (std::find(path_effect_factory_names.begin(),
                path_effect_factory_names.end(),
                factory_name) == path_effect_factory_names.end()) {
    return nullptr;
  }

  return ReadPathEffectFromMemory;
}

}  // namespace skity
