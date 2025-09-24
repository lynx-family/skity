// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_STATIC_BUFFER_HPP
#define SRC_RENDER_HW_HW_STATIC_BUFFER_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "src/render/hw/hw_stage_buffer.hpp"

namespace skity {

class GPUDevice;

class HWStaticBuffer final {
 public:
  explicit HWStaticBuffer(GPUDevice* device);

  ~HWStaticBuffer();

  void Flush();

  GPUBufferView GetTessPathFillVertexBufferView();

  GPUBufferView GetTessPathFillIndexBufferView();

  GPUBufferView GetTessPathStrokeVertexBufferView();

  GPUBufferView GetTessPathStrokeIndexBufferView();

 private:
  void Initialize();

  std::unique_ptr<HWStageBuffer> stage_buffer_;

  std::optional<GPUBufferView> tess_path_fill_vertex_buffer_view_;
  std::optional<GPUBufferView> tess_path_fill_index_buffer_view_;
  std::optional<GPUBufferView> tess_path_stroke_vertex_buffer_view_;
  std::optional<GPUBufferView> tess_path_stroke_index_buffer_view_;
  bool initialized_ = false;
  bool needs_flush_ = false;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_STATIC_BUFFER_HPP
