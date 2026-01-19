// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_BUFFER_LAYOUT_MAP_HPP
#define SRC_RENDER_HW_HW_BUFFER_LAYOUT_MAP_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "src/gpu/gpu_render_pipeline.hpp"
#include "src/render/hw/hw_pipeline_key.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"

namespace skity {

class GPUDevice;

class HWBufferLayoutMap {
 public:
  static HWBufferLayoutMap& GetInstance();

  HWBufferLayoutMap();

  const std::vector<GPUVertexBufferLayout>* GetBufferLayout(
      HWGeometryKeyType::Value key) const {
    return &buffer_layout_map_.at(key);
  }

 private:
  void RegisterAllBufferLayouts();

  std::unordered_map<HWGeometryKeyType::Value,
                     std::vector<GPUVertexBufferLayout>>
      buffer_layout_map_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_BUFFER_LAYOUT_MAP_HPP
