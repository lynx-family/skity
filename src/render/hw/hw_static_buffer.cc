// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_static_buffer.hpp"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <memory>

#include "src/gpu/gpu_buffer.hpp"
#include "src/render/hw/draw/geometry/wgsl_tess_path_fill_geometry.hpp"

namespace skity {

HWStaticBuffer::HWStaticBuffer(GPUDevice* device)
    : stage_buffer_(std::make_unique<HWStageBuffer>(device)) {}

HWStaticBuffer::~HWStaticBuffer() = default;

void HWStaticBuffer::Flush() {
  if (needs_flush_) {
    stage_buffer_->Flush();
  }
  needs_flush_ = false;
}

GPUBufferView HWStaticBuffer::GetTessPathFillVertexBufferView() {
  if (!initialized_) {
    Initialize();
  }
  return tess_path_fill_vertex_buffer_view_.value();
}

GPUBufferView HWStaticBuffer::GetTessPathFillIndexBufferView() {
  if (!initialized_) {
    Initialize();
  }
  return tess_path_fill_index_buffer_view_.value();
}

void HWStaticBuffer::Initialize() {
  tess_path_fill_vertex_buffer_view_ =
      WGSLTessPathFillGeometry::CreateVertexBufferView(stage_buffer_.get());
  tess_path_fill_index_buffer_view_ =
      WGSLTessPathFillGeometry::CreateIndexBufferView(stage_buffer_.get());
  initialized_ = true;
  needs_flush_ = true;
}

}  // namespace skity
