// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/geometry/wgsl_text_geometry.hpp"

#include <utility>

#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/tracing.hpp"

namespace skity {
namespace {

constexpr HWFunctionBaseKey kHasDeviceToLocal = 1;

Rect GetGlyphRectBounds(const GlyphRect& glyph_rect, const Matrix& transform) {
  return transform.MapRect(
      Rect::MakeLTRB(glyph_rect.vertex_coord.x, glyph_rect.vertex_coord.y,
                     glyph_rect.vertex_coord.z, glyph_rect.vertex_coord.w));
}

struct Instance {
  Vec4 vertex_coord;
  Vec4 texture_coord;
  Vec4 color;
};

static_assert(sizeof(Instance) == 48);

}  // namespace

std::vector<GlyphRectBatch> BuildGlyphRectBatches(
    ArrayList<GlyphRect, 16> glyph_rects, const Matrix& transform,
    ArenaAllocator* arena_allocator, bool split_overlapping_glyphs) {
  std::vector<GlyphRectBatch> batches;
  if (glyph_rects.empty()) {
    return batches;
  }

  if (!split_overlapping_glyphs) {
    GlyphRectBatch batch;
    for (const auto& glyph_rect : glyph_rects) {
      batch.bounds.Join(GetGlyphRectBounds(glyph_rect, transform));
    }
    batch.glyph_rects = std::move(glyph_rects);
    batches.push_back(std::move(batch));
    return batches;
  }

  // ponytail: O(n^2) within one glyph batch; use a spatial index only if long
  // overlapping runs show up in profiles.
  std::vector<Rect> occupied_bounds;
  auto begin_batch = [&]() -> GlyphRectBatch& {
    batches.emplace_back();
    batches.back().glyph_rects.SetArenaAllocator(arena_allocator);
    occupied_bounds.clear();
    return batches.back();
  };

  auto* batch = &begin_batch();
  for (const auto& glyph_rect : glyph_rects) {
    const Rect glyph_bounds = GetGlyphRectBounds(glyph_rect, transform);
    bool overlaps = false;
    if (Rect::Intersect(batch->bounds, glyph_bounds)) {
      for (const auto& occupied : occupied_bounds) {
        if (Rect::Intersect(occupied, glyph_bounds)) {
          overlaps = true;
          break;
        }
      }
    }

    if (overlaps) {
      batch = &begin_batch();
    }
    batch->glyph_rects.push_back(glyph_rect);
    batch->bounds.Join(glyph_bounds);
    occupied_bounds.push_back(glyph_bounds);
  }
  return batches;
}

WGSLTextGeometry::WGSLTextGeometry(
    std::vector<BatchGroup<GlyphRect>> glyph_rects, BatchedTexture textures,
    std::shared_ptr<GPUSampler> sampler, TextAtlasEffect effect,
    std::optional<Matrix> device_to_local)
    : HWWGSLGeometry(Flags::kSnippet | Flags::kAffectsFragment |
                     ((effect == TextAtlasEffect::kColor ||
                       effect == TextAtlasEffect::kColorSwizzleRB)
                          ? Flags::kAffectsFragmentColor
                          : 0)),
      glyph_rects_(std::move(glyph_rects)),
      textures_(std::move(textures)),
      sampler_(std::move(sampler)),
      effect_(effect),
      device_to_local_(std::move(device_to_local)) {}

std::vector<GPUVertexBufferLayout> WGSLTextGeometry::GetBufferLayout() {
  return {
      GPUVertexBufferLayout{
          4 * sizeof(float),
          GPUVertexStepMode::kVertex,
          {
              GPUVertexAttribute{GPUVertexFormat::kFloat32x4, 0, 0},
          },
      },
      GPUVertexBufferLayout{
          12 * sizeof(float),
          GPUVertexStepMode::kInstance,
          {
              GPUVertexAttribute{GPUVertexFormat::kFloat32x4, 0, 1},
              GPUVertexAttribute{GPUVertexFormat::kFloat32x4, 4 * sizeof(float),
                                 2},
              GPUVertexAttribute{GPUVertexFormat::kFloat32x4, 8 * sizeof(float),
                                 3},
          },
      },
  };
}

HWFunctionBaseKey WGSLTextGeometry::GetMainKey() const {
  return MakeMainKey(HWGeometryKeyType::kText,
                     device_to_local_.has_value() ? kHasDeviceToLocal : 0);
}

HWFunctionBaseKey WGSLTextGeometry::GetFSSubKey() const {
  switch (effect_) {
    case TextAtlasEffect::kA8Coverage:
      return HWGeometryFSKeyType::kTextA8;
    case TextAtlasEffect::kSDFCoverage:
      return HWGeometryFSKeyType::kTextSDF;
    case TextAtlasEffect::kColor:
      return HWGeometryFSKeyType::kTextColor;
    case TextAtlasEffect::kColorSwizzleRB:
      return HWGeometryFSKeyType::kTextColorSwizzleRB;
  }
  return HWGeometryFSKeyType::kNone;
}

void WGSLTextGeometry::WriteVSFunctionsAndStructs(std::stringstream& ss) const {
  ss << CommonVertexWGSL();
  ss << R"(
fn get_texture_index(u: f32) -> i32 {
  return i32(u) >> 14;
}

fn get_texture_uv(uv: vec2<f32>) -> vec2<f32> {
  let u: i32 = i32(uv.x);
  return vec2<f32>(f32(u & 0x3FFF), uv.y);
}
)";
}

void WGSLTextGeometry::WriteVSUniforms(std::stringstream& ss) const {
  ss << "@group(0) @binding(0) var<uniform> common_slot: CommonSlot;\n";
  if (device_to_local_) {
    ss << "@group(0) @binding(2) var<uniform> "
          "uTextDeviceToLocal: mat4x4<f32>;\n";
  }
}

void WGSLTextGeometry::WriteVSInput(std::stringstream& ss) const {
  ss << R"(
struct VSInput {
  @location(0) a_offset: vec4<f32>,
  @location(1) a_pos: vec4<f32>,
  @location(2) a_uv: vec4<f32>,
  @location(3) color: vec4<f32>,
};
)";
}

void WGSLTextGeometry::WriteVSMain(std::stringstream& ss) const {
  ss << R"(
  let pos: vec2<f32> = vec2<f32>(
      input.a_offset.x * input.a_pos.x + input.a_offset.z * input.a_pos.z,
      input.a_offset.y * input.a_pos.y + input.a_offset.w * input.a_pos.w);
  let uv: vec2<f32> = vec2<f32>(
      input.a_offset.x * input.a_uv.x + input.a_offset.z * input.a_uv.z,
      input.a_offset.y * input.a_uv.y + input.a_offset.w * input.a_uv.w);
)";
  if (device_to_local_) {
    ss << R"(
  local_pos = (uTextDeviceToLocal * common_slot.userTransform *
               vec4<f32>(pos, 0.0, 1.0)).xy;
)";
  } else {
    ss << "  local_pos = pos;\n";
  }
  ss << R"(
  output.pos = get_vertex_position(pos, common_slot);
  output.v_txt_index = get_texture_index(uv.x);
  output.v_uv = get_texture_uv(uv);
)";
}

std::optional<std::vector<std::string>> WGSLTextGeometry::GetVarings() const {
  return std::vector<std::string>{
      "@interpolate(flat) v_txt_index: i32",
      "v_uv: vec2<f32>",
  };
}

void WGSLTextGeometry::WriteFSFunctionsAndStructs(std::stringstream& ss) const {
  ss << R"(
fn get_text_atlas_color(font_index: i32, uv: vec2<f32>) -> vec4<f32> {
  var dimensions: vec2<u32> = vec2<u32>(textureDimensions(uTextAtlas0));
  var texture_uv: vec2<f32> =
      vec2<f32>(uv.x / f32(dimensions.x), uv.y / f32(dimensions.y));
  var color0: vec4<f32> =
      textureSample(uTextAtlas0, uTextSampler, texture_uv);
  var color1: vec4<f32> =
      textureSample(uTextAtlas1, uTextSampler, texture_uv);
  var color2: vec4<f32> =
      textureSample(uTextAtlas2, uTextSampler, texture_uv);
  var color3: vec4<f32> =
      textureSample(uTextAtlas3, uTextSampler, texture_uv);

  if font_index == 0 {
    return color0;
  } else if font_index == 1 {
    return color1;
  } else if font_index == 2 {
    return color2;
  } else if font_index == 3 {
    return color3;
  } else {
    return color0;
  }
}
)";
}

void WGSLTextGeometry::WriteFSUniforms(std::stringstream& ss) const {
  ss << R"(
@group(3) @binding(0) var uTextSampler: sampler;
@group(3) @binding(1) var uTextAtlas0: texture_2d<f32>;
@group(3) @binding(2) var uTextAtlas1: texture_2d<f32>;
@group(3) @binding(3) var uTextAtlas2: texture_2d<f32>;
@group(3) @binding(4) var uTextAtlas3: texture_2d<f32>;
)";
}

void WGSLTextGeometry::WriteFSColor(std::stringstream& ss) const {
  if (effect_ != TextAtlasEffect::kColor &&
      effect_ != TextAtlasEffect::kColorSwizzleRB) {
    return;
  }

  ss << R"(
  let text_paint_alpha: f32 = color.a;
  color = get_text_atlas_color(input.v_txt_index, input.v_uv);
)";
  if (effect_ == TextAtlasEffect::kColorSwizzleRB) {
    ss << "  color = color.bgra;\n";
  }
  ss << "  color *= text_paint_alpha;\n";
}

void WGSLTextGeometry::WriteFSAlphaMask(std::stringstream& ss) const {
  if (effect_ == TextAtlasEffect::kA8Coverage) {
    ss << R"(
  mask_alpha = get_text_atlas_color(input.v_txt_index, input.v_uv).r;
)";
  } else if (effect_ == TextAtlasEffect::kSDFCoverage) {
    ss << R"(
  var distance: f32 =
      7.96875 * (get_text_atlas_color(input.v_txt_index, input.v_uv).r -
                 0.5019608);
  var distance_gradient: vec2<f32> =
      vec2<f32>(dFdx(distance), dFdy(distance));
  let gradient_length_squared: f32 =
      dot(distance_gradient, distance_gradient);
  if gradient_length_squared < 0.0001 {
    distance_gradient = vec2<f32>(0.7071);
  } else {
    distance_gradient =
        distance_gradient * inversesqrt(gradient_length_squared);
  }
  let jacobian: mat2x2<f32> =
      mat2x2<f32>(dFdx(input.v_uv), dFdy(input.v_uv));
  let gradient: vec2<f32> = jacobian * distance_gradient;
  let antialias_width: f32 = 0.65 * length(gradient);
  mask_alpha = smoothstep(-antialias_width, antialias_width, distance);
)";
  }
}

void WGSLTextGeometry::PrepareCMD(Command* cmd, HWDrawContext* context,
                                  const Matrix& transform, float clip_depth,
                                  Command* stencil_cmd) {
  SKITY_TRACE_EVENT(WGSLTextGeometry_PrepareCMD);
  (void)stencil_cmd;

  if (cmd == nullptr || cmd->pipeline == nullptr) {
    return;
  }

  cmd->vertex_buffer = context->static_buffer->GetUnitQuadVertexBufferView();
  cmd->index_buffer = context->static_buffer->GetUnitQuadIndexBufferView();
  cmd->index_count = cmd->index_buffer.range / sizeof(uint32_t);

  context->stageBuffer->BeginWritingInstance(
      glyph_rects_.size() * sizeof(Instance), alignof(Instance));
  for (const auto& glyph_rect : glyph_rects_) {
    context->stageBuffer->AppendInstance<Instance>(
        glyph_rect.item.vertex_coord,
        Vec4{glyph_rect.item.texture_coord_tl,
             glyph_rect.item.texture_coord_br},
        glyph_rect.paint.GetColor4f());
  }
  cmd->instance_buffer = context->stageBuffer->EndWritingInstance();
  cmd->instance_count = cmd->instance_buffer.range / sizeof(Instance);

  auto common_group = cmd->pipeline->GetBindingGroup(0);
  if (common_group == nullptr) {
    return;
  }

  auto common_slot = common_group->GetEntry(0);
  if (!SetupCommonInfo(common_slot, context->mvp, transform, clip_depth)) {
    return;
  }
  UploadBindGroup(common_group->group, common_slot, cmd, context);

  if (device_to_local_) {
    auto entry = common_group->GetEntry(2);
    if (entry == nullptr || entry->type_definition == nullptr ||
        entry->type_definition->name != "mat4x4<f32>") {
      return;
    }
    entry->type_definition->SetData(&*device_to_local_, sizeof(Matrix));
    UploadBindGroup(common_group->group, entry, cmd, context);
  }

  if (textures_[0] == nullptr) {
    return;
  }
  auto atlas_group = cmd->pipeline->GetBindingGroup(3);
  if (atlas_group == nullptr) {
    return;
  }

  auto sampler_entry = atlas_group->GetEntry(0);
  if (sampler_entry == nullptr ||
      sampler_entry->type != wgx::BindingType::kSampler) {
    return;
  }
  UploadBindGroup(atlas_group->group, sampler_entry, cmd, sampler_);

  std::shared_ptr<GPUTexture> last_texture;
  for (size_t i = 0; i < textures_.size(); ++i) {
    auto texture_entry = atlas_group->GetEntry(i + 1);
    if (texture_entry == nullptr ||
        texture_entry->type != wgx::BindingType::kTexture) {
      return;
    }
    if (textures_[i] != nullptr) {
      last_texture = textures_[i];
    }
    if (last_texture == nullptr) {
      return;
    }
    UploadBindGroup(atlas_group->group, texture_entry, cmd, last_texture);
  }
}

}  // namespace skity
