// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/coverage/coverage_aa_renderer.hpp"

#include "src/gpu/gpu_command_buffer.hpp"
#include "src/gpu/gpu_context_impl.hpp"
#include "src/gpu/gpu_device.hpp"
#include "src/gpu/gpu_texture.hpp"
#include "src/logging.hpp"
#include "src/render/hw/coverage/coverage_aa_line_encoder.hpp"
#include "src/render/hw/draw/hw_dynamic_coverage_path_draw.hpp"

namespace skity {

CoverageAARenderer::CoverageAARenderer()
    : path_tiler_(std::make_unique<CoverageAAPathTiler>(
          frame_data_.tiles, lines_, line_range_counts_)) {}

void CoverageAARenderer::AddDraw(HWDynamicCoveragePathDraw* draw) {
  draws_.push_back(draw);
  tiled_path_count_ += draw->GetPathGroups().size();
  draw->SetCoverageAAFrameData(&frame_data_);
}

void CoverageAARenderer::BeginFrame() {
  draws_.clear();
  tiled_path_count_ = 0;
  frame_data_.Reset();
  lines_.clear();
  line_range_counts_.clear();
}

void CoverageAARenderer::PrepareFrame(HWDrawContext* context,
                                      GPUCommandBuffer* command_buffer) {
  if (context == nullptr || command_buffer == nullptr ||
      context->gpuContext == nullptr || draws_.empty() ||
      context->gpuContext->GetGPUDevice() == nullptr) {
    return;
  }

  DEBUG_CHECK(frame_data_.tiled_paths.empty());
  frame_data_.tiled_paths.reserve(tiled_path_count_);
  for (auto* draw : draws_) {
    // Keep each draw's tiled paths and tiles contiguous. The geometry uses the
    // first and last tiled paths to derive the draw's complete tile span.
    size_t offset = frame_data_.tiled_paths.size();
    for (const auto& group : draw->GetPathGroups()) {
      frame_data_.tiled_paths.push_back(path_tiler_->Tile(
          group.item, group.transform, &draw->GetScissorBox()));
    }
    draw->SetTiledPathRange(offset, frame_data_.tiled_paths.size() - offset);
  }

  auto* device = context->gpuContext->GetGPUDevice();
  EncodeCoverageAALines(lines_, line_range_counts_, device->GetMaxTextureSize(),
                        frame_data_.encoded_lines);

  GPUTextureDescriptor line_desc{};
  line_desc.width = frame_data_.encoded_lines.texture_width;
  line_desc.height = frame_data_.encoded_lines.texture_height;
  line_desc.mip_level_count = 1;
  line_desc.sample_count = 1;
  line_desc.format = GPUTextureFormat::kRGBA16Uint;
  line_desc.storage_mode = GPUTextureStorageMode::kHostVisible;
  line_desc.usage =
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kCopyDst) |
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kTextureBinding);
  frame_data_.line_texture = device->CreateTexture(line_desc);
  if (frame_data_.line_texture == nullptr) {
    return;
  }
  frame_data_.line_texture->UploadData(
      0, 0, frame_data_.encoded_lines.texture_width,
      frame_data_.encoded_lines.texture_height,
      frame_data_.encoded_lines.texture_data.data());

  command_buffer->HoldResource(frame_data_.line_texture);
}

}  // namespace skity
