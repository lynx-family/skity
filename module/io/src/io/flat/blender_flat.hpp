// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_IO_SRC_IO_FLAT_BLENDER_FLAT_HPP
#define MODULE_IO_SRC_IO_FLAT_BLENDER_FLAT_HPP

#include <skity/graphic/blend_mode.hpp>
#include <skity/io/flattenable.hpp>

#include "src/io/memory_read.hpp"

namespace skity {

class BlenderModeFlattenable : public Flattenable {
 public:
  explicit BlenderModeFlattenable(BlendMode blend_mode)
      : blend_mode_(blend_mode) {}

  ~BlenderModeFlattenable() override = default;

  std::string_view ProcName() const override { return "BlendModeFlattener"; }

  void FlattenToBuffer(WriteBuffer& buffer) const override {
    buffer.WriteInt32(static_cast<int32_t>(blend_mode_));
  }

  static void SkipReadBlender(ReadBuffer& buffer);

 private:
  BlendMode blend_mode_;
};

}  // namespace skity

#endif  // MODULE_IO_SRC_IO_FLAT_BLENDER_FLAT_HPP
