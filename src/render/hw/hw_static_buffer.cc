// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_static_buffer.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <memory>

#include "src/gpu/gpu_buffer.hpp"
#include "src/render/hw/draw/geometry/wgsl_rrect_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_tess_path_fill_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_tess_path_stroke_geometry.hpp"

namespace skity {
namespace {

GPUBufferView CreateUnitQuadVertexBufferView(HWStageBuffer* stage_buffer) {
  std::array<float, 16> vertices = {
      1.0f, 1.0f, 0.0f, 0.0f,  // Top left
      1.0f, 0.0f, 0.0f, 1.0f,  // Bottom left
      0.0f, 1.0f, 1.0f, 0.0f,  // Top right
      0.0f, 0.0f, 1.0f, 1.0f,  // Bottom right
  };
  return stage_buffer->Push(vertices.data(), vertices.size() * sizeof(float));
}

GPUBufferView CreateUnitQuadIndexBufferView(HWStageBuffer* stage_buffer) {
  std::array<uint32_t, 6> indices = {0, 1, 2, 1, 3, 2};
  return stage_buffer->PushIndex(indices.data(),
                                 indices.size() * sizeof(uint32_t));
}

}  // namespace

HWStaticBuffer::HWStaticBuffer(GPUDevice* device)
    : stage_buffer_(std::make_unique<HWStageBuffer>(device)) {}

HWStaticBuffer::~HWStaticBuffer() = default;

void HWStaticBuffer::Flush(GPUCommandBuffer* command_buffer) {
  if (needs_flush_) {
    stage_buffer_->Flush(command_buffer);
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

GPUBufferView HWStaticBuffer::GetTessPathStrokeVertexBufferView() {
  if (!initialized_) {
    Initialize();
  }
  return tess_path_stroke_vertex_buffer_view_.value();
}

GPUBufferView HWStaticBuffer::GetTessPathStrokeIndexBufferView() {
  if (!initialized_) {
    Initialize();
  }
  return tess_path_stroke_index_buffer_view_.value();
}

GPUBufferView HWStaticBuffer::GetRRectVertexBufferView() {
  if (!initialized_) {
    Initialize();
  }
  return rrect_vertex_buffer_view_.value();
}

GPUBufferView HWStaticBuffer::GetRRectIndexBufferView() {
  if (!initialized_) {
    Initialize();
  }
  return rrect_index_buffer_view_.value();
}

GPUBufferView HWStaticBuffer::GetUnitQuadVertexBufferView() {
  if (!initialized_) {
    Initialize();
  }
  return unit_quad_vertex_buffer_view_.value();
}

GPUBufferView HWStaticBuffer::GetUnitQuadIndexBufferView() {
  if (!initialized_) {
    Initialize();
  }
  return unit_quad_index_buffer_view_.value();
}

void HWStaticBuffer::Initialize() {
  tess_path_fill_vertex_buffer_view_ =
      WGSLTessPathFillGeometry::CreateVertexBufferView(stage_buffer_.get());
  tess_path_fill_index_buffer_view_ =
      WGSLTessPathFillGeometry::CreateIndexBufferView(stage_buffer_.get());
  tess_path_stroke_vertex_buffer_view_ =
      WGSLTessPathStrokeGeometry::CreateVertexBufferView(stage_buffer_.get());
  tess_path_stroke_index_buffer_view_ =
      WGSLTessPathStrokeGeometry::CreateIndexBufferView(stage_buffer_.get());
  rrect_vertex_buffer_view_ =
      WGSLRRectGeometry::CreateVertexBufferView(stage_buffer_.get());
  rrect_index_buffer_view_ =
      WGSLRRectGeometry::CreateIndexBufferView(stage_buffer_.get());
  unit_quad_vertex_buffer_view_ =
      CreateUnitQuadVertexBufferView(stage_buffer_.get());
  unit_quad_index_buffer_view_ =
      CreateUnitQuadIndexBufferView(stage_buffer_.get());

  initialized_ = true;
  needs_flush_ = true;
}

}  // namespace skity
