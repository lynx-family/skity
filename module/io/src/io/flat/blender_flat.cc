// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/io/flat/blender_flat.hpp"

#include "src/picture_priv.hpp"

namespace skity {

namespace {

void SkipBlendModeBlender(ReadBuffer& buffer) {
  (void)buffer.ReadU32();  // skip the factory id

  (void)buffer.ReadU32();  // skip the blend mode
}

void SkipRuntimeEffectChild(ReadBuffer& buffer) {}

void SkipRuntimeBlender(ReadBuffer& buffer) {
  // we do not have sksl compiler, so do not check if buffer allow read sksl
  bool is_stable_effect = false;
  if (!buffer.IsVersionLT(Version::kSerializeStableKeys)) {
    auto candidate = buffer.ReadU32();  // skip candidate stable key

    is_stable_effect = (candidate > 500 && candidate <= 528);

    if (!is_stable_effect) {
      return;
    }
  }

  std::string sksl;
  (void)buffer.ReadString(sksl);

  // uniforms
  (void)buffer.ReadByteArrayAsData();

  // skip childrens

  auto num_children = buffer.ReadU32();

  for (size_t i = 0; i < num_children; i++) {
    (void)buffer.ReadRawFlattenable();
  }
}

}  // namespace

void BlenderModeFlattenable::SkipReadBlender(ReadBuffer& buffer) {
  auto factory_index = buffer.ReadInt();

  if (factory_index == 0 || !buffer.IsValid()) {
    return;
  }

  uint32_t size_recorded = buffer.ReadU32();

  auto factory = buffer.GetFactoryName(factory_index - 1);

  if (factory.empty()) {
    return;
  }

  if (factory == "SkBlendModeBlender") {
    SkipBlendModeBlender(buffer);
  } else if (factory == "SkRuntimeBlender") {
    SkipRuntimeBlender(buffer);
  }
}

}  // namespace skity
