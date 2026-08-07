// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/geometry/wgsl_coverage_aa_tile_geometry.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

#include "src/render/hw/coverage/coverage_aa_line_encoder.hpp"
#include "src/render/hw/coverage/wgsl_coverage_aa_common.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_pipeline_key.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/tracing.hpp"

namespace skity {
namespace {

constexpr uint32_t kCoverageAAQuadIndexCount = 6;

struct CoverageAATileInstance {
  float tile_origin_x = 0.0f;
  float tile_origin_y = 0.0f;
  uint32_t line_start = 0;
  uint32_t line_count = 0;
  int32_t backdrop = 0;
  int32_t even_odd = 0;
};

static_assert(kCoverageAATileWidth == 16 && kCoverageAATileHeight == 16,
              "Update the Coverage AA WGSL tile constants");
static_assert(kCoverageAASubpixelScale == 256,
              "Update the Coverage AA WGSL fixed-point decoder");
static_assert(std::is_standard_layout_v<CoverageAATileInstance>,
              "Update the Coverage AA WGSL/instance layout");
static_assert(sizeof(CoverageAATileInstance) == 24,
              "Update the Coverage AA WGSL/instance layout");

}  // namespace

WGSLCoverageAATileGeometry::WGSLCoverageAATileGeometry(
    const CoverageAAFrameData* frame_data, size_t tiled_path_offset,
    size_t tiled_path_count, Matrix physical_to_layer,
    bool enable_conflation_correction)
    : HWWGSLGeometry(Flags::kSnippet | Flags::kAffectsFragment),
      frame_data_(frame_data),
      tiled_path_offset_(tiled_path_offset),
      tiled_path_count_(tiled_path_count),
      physical_to_layer_(physical_to_layer),
      enable_conflation_correction_(enable_conflation_correction) {}

std::vector<GPUVertexBufferLayout>
WGSLCoverageAATileGeometry::GetBufferLayout() {
  return {
      GPUVertexBufferLayout{
          4 * sizeof(float),
          GPUVertexStepMode::kVertex,
          {
              GPUVertexAttribute{GPUVertexFormat::kFloat32x4, 0, 0},
          },
      },
      GPUVertexBufferLayout{
          sizeof(CoverageAATileInstance),
          GPUVertexStepMode::kInstance,
          {
              GPUVertexAttribute{
                  GPUVertexFormat::kFloat32x2,
                  offsetof(CoverageAATileInstance, tile_origin_x), 1},
              GPUVertexAttribute{GPUVertexFormat::kUint32x2,
                                 offsetof(CoverageAATileInstance, line_start),
                                 2},
              GPUVertexAttribute{GPUVertexFormat::kSint32x2,
                                 offsetof(CoverageAATileInstance, backdrop), 3},
          },
      },
  };
}

HWFunctionBaseKey WGSLCoverageAATileGeometry::GetMainKey() const {
  return HWGeometryKeyType::kCoverageAA;
}

HWFunctionBaseKey WGSLCoverageAATileGeometry::GetFSSubKey() const {
  return enable_conflation_correction_
             ? HWFragmentMaskKeyType::kCoverageAAConflationCorrection
             : HWFragmentMaskKeyType::kCoverageAA;
}

void WGSLCoverageAATileGeometry::WriteVSFunctionsAndStructs(
    std::stringstream& ss) const {
  ss << CommonVertexWGSL();
}

void WGSLCoverageAATileGeometry::WriteVSUniforms(std::stringstream& ss) const {
  ss << R"(
@group(0) @binding(0) var<uniform> common_slot: CommonSlot;
@group(3) @binding(2) var<uniform> uCoverageAAGlobalToLocal: mat4x4<f32>;
)";
}

void WGSLCoverageAATileGeometry::WriteVSInput(std::stringstream& ss) const {
  ss << R"(
struct VSInput {
  @location(0) a_unit_quad: vec4<f32>,
  @location(1) a_tile_origin: vec2<f32>,
  @location(2) a_line_range: vec2<u32>,
  @location(3) a_backdrop_and_fill_rule: vec2<i32>,
};
)";
}

void WGSLCoverageAATileGeometry::WriteVSMain(std::stringstream& ss) const {
  ss << R"(
  let tile_pos: vec2<f32> = input.a_unit_quad.zw * vec2<f32>(16.0, 16.0);
  let global_pos: vec2<f32> = input.a_tile_origin + tile_pos;

  // Coverage AA tiles are placed in global space, but shading snippets expect
  // the original draw-local position.
  local_pos = (uCoverageAAGlobalToLocal * vec4<f32>(global_pos, 0.0, 1.0)).xy;
  output.pos = get_vertex_position(global_pos, common_slot);
  output.v_tile_pos = tile_pos;
  output.v_line_range = input.a_line_range;
  output.v_backdrop_and_fill_rule = input.a_backdrop_and_fill_rule;
)";
}

std::optional<std::vector<std::string>> WGSLCoverageAATileGeometry::GetVarings()
    const {
  return std::vector<std::string>{
      "v_tile_pos: vec2<f32>",
      "@interpolate(flat) v_line_range: vec2<u32>",
      "@interpolate(flat) v_backdrop_and_fill_rule: vec2<i32>",
  };
}

void WGSLCoverageAATileGeometry::WriteFSFunctionsAndStructs(
    std::stringstream& ss) const {
  ss << kCoverageAAFillRuleWGSL << kCoverageAAEdgeContributionWGSL
     << kCoverageAALineLoadWGSL;
  if (enable_conflation_correction_) {
    ss << kCoverageAAConflationCorrectionWGSL;
  } else {
    ss << kCoverageAAResolveWGSL;
  }
}

void WGSLCoverageAATileGeometry::WriteFSUniforms(std::stringstream& ss) const {
  ss << R"(
@group(3) @binding(1) var uCoverageAALineTexture: texture_2d<u32>;
)";
}

void WGSLCoverageAATileGeometry::WriteFSAlphaMask(std::stringstream& ss) const {
  ss << R"(
  // The quad reaches 16 on its right and bottom edges, but the last pixel in a
  // 16x16 tile is pixel 15. Line endpoints may still decode exactly to 16.
  let tile_pixel: vec2<f32> = clamp(floor(input.v_tile_pos),
                                    vec2<f32>(0.0, 0.0),
                                    vec2<f32>(15.0, 15.0));
)";

  if (enable_conflation_correction_) {
    ss << R"(
  mask_alpha = coverage_aa_resolve_pixel_with_conflation_correction(
      input.v_line_range, input.v_backdrop_and_fill_rule, tile_pixel);
)";
    return;
  }

  ss << R"(
  mask_alpha = coverage_aa_resolve_pixel(
      input.v_line_range, input.v_backdrop_and_fill_rule, tile_pixel);
)";
}

void WGSLCoverageAATileGeometry::PrepareCMD(Command* cmd,
                                            HWDrawContext* context,
                                            const Matrix& transform,
                                            float clip_depth, Command*) {
  SKITY_TRACE_EVENT(WGSLCoverageAATileGeometry_PrepareCMD);

  if (cmd == nullptr || cmd->pipeline == nullptr || context == nullptr ||
      context->static_buffer == nullptr || context->stageBuffer == nullptr ||
      frame_data_ == nullptr || tiled_path_count_ == 0 ||
      tiled_path_offset_ > frame_data_->tiled_paths.size() ||
      tiled_path_count_ >
          frame_data_->tiled_paths.size() - tiled_path_offset_ ||
      frame_data_->line_texture == nullptr) {
    return;
  }

  Matrix global_to_local;
  if (transform.HasPersp() || !transform.Invert(&global_to_local)) {
    return;
  }

  cmd->vertex_buffer = context->static_buffer->GetUnitQuadVertexBufferView();
  cmd->index_buffer = context->static_buffer->GetUnitQuadIndexBufferView();

  // The renderer appends every tiled path and its tiles in draw order, so a
  // draw's complete tile span is described by its first and last tiled paths.
  const size_t tiled_path_end = tiled_path_offset_ + tiled_path_count_;
  const size_t tile_begin =
      frame_data_->tiled_paths[tiled_path_offset_].tile_offset;
  const auto& last_tiled_path = frame_data_->tiled_paths[tiled_path_end - 1];
  if (tile_begin > frame_data_->tiles.size() ||
      last_tiled_path.tile_offset > frame_data_->tiles.size() ||
      last_tiled_path.tile_count >
          frame_data_->tiles.size() - last_tiled_path.tile_offset) {
    return;
  }
  const size_t draw_tile_end =
      last_tiled_path.tile_offset + last_tiled_path.tile_count;
  if (draw_tile_end < tile_begin) {
    return;
  }
  const size_t tile_count = draw_tile_end - tile_begin;
  if (tile_count == 0) {
    return;
  }

  context->stageBuffer->BeginWritingInstance(
      tile_count * sizeof(CoverageAATileInstance),
      alignof(CoverageAATileInstance));
  for (size_t i = tiled_path_offset_; i < tiled_path_end; ++i) {
    const auto& tiled_path = frame_data_->tiled_paths[i];
    const int32_t even_odd =
        tiled_path.fill_type == Path::PathFillType::kEvenOdd ? 1 : 0;
    size_t tiled_path_tile_end = tiled_path.tile_offset + tiled_path.tile_count;
    for (size_t tile_index = tiled_path.tile_offset;
         tile_index < tiled_path_tile_end; ++tile_index) {
      const auto& tile = frame_data_->tiles[tile_index];
      bool has_lines = tile.line_range_id.IsValid();
      uint32_t line_start = 0;
      uint32_t line_count = 0;
      if (has_lines) {
        uint32_t range_id = tile.line_range_id.value;
        uint32_t range_start =
            frame_data_->encoded_lines.range_offsets[range_id];
        uint32_t range_end =
            frame_data_->encoded_lines.range_offsets[range_id + 1];
        line_start = range_start;
        line_count = range_end - range_start;
      }
      context->stageBuffer->AppendInstance<CoverageAATileInstance>(
          static_cast<float>(tile.tile_x) * kCoverageAATileWidth,
          static_cast<float>(tile.tile_y) * kCoverageAATileHeight, line_start,
          line_count, tile.backdrop, even_odd);
    }
  }

  cmd->instance_buffer = context->stageBuffer->EndWritingInstance();
  cmd->index_count = kCoverageAAQuadIndexCount;
  cmd->instance_count =
      cmd->instance_buffer.range / sizeof(CoverageAATileInstance);

  auto common_group = cmd->pipeline->GetBindingGroup(0);
  if (common_group != nullptr) {
    auto common_slot = common_group->GetEntry(0);
    if (SetupCommonInfo(common_slot, context->mvp, physical_to_layer_,
                        clip_depth)) {
      UploadBindGroup(common_group->group, common_slot, cmd, context);
    }
  }

  auto coverage_group = cmd->pipeline->GetBindingGroup(3);
  if (coverage_group == nullptr) {
    return;
  }

  auto texture_entry = coverage_group->GetEntry(1);
  if (texture_entry != nullptr) {
    UploadBindGroup(coverage_group->group, texture_entry, cmd,
                    frame_data_->line_texture);
  }

  auto matrix_entry = coverage_group->GetEntry(2);
  if (matrix_entry != nullptr &&
      matrix_entry->type_definition->name == "mat4x4<f32>") {
    matrix_entry->type_definition->SetData(&global_to_local, sizeof(Matrix));
    UploadBindGroup(coverage_group->group, matrix_entry, cmd, context);
  }
}

}  // namespace skity
