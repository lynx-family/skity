// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/geometry/wgsl_text_geometry.hpp"

#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/tracing.hpp"

namespace skity {
namespace {

constexpr HWFunctionBaseKey kHasLayerToLocal = 1;

struct Instance {
  Vec4 vertex_coord;
  Vec4 texture_coord;
  Vec4 color;
};

static_assert(sizeof(Instance) == 48);

}  // namespace

WGSLTextGeometry::WGSLTextGeometry(const TextGeometryData& data)
    : HWWGSLGeometry(Flags::kSnippet | Flags::kAffectsFragment |
                     ((data.effect == TextAtlasEffect::kColor ||
                       data.effect == TextAtlasEffect::kColorSwizzleRB)
                          ? Flags::kAffectsFragmentColor
                          : 0)),
      data_(data) {}

std::vector<GPUVertexBufferLayout> WGSLTextGeometry::GetBufferLayout() {
  return std::vector<GPUVertexBufferLayout>{
      GPUVertexBufferLayout{
          4 * sizeof(float),
          GPUVertexStepMode::kVertex,
          {
              GPUVertexAttribute{
                  GPUVertexFormat::kFloat32x4,
                  0,
                  0,
              },
          },
      },
      GPUVertexBufferLayout{
          12 * sizeof(float),
          GPUVertexStepMode::kInstance,
          {
              GPUVertexAttribute{
                  GPUVertexFormat::kFloat32x4,
                  0,
                  1,
              },
              GPUVertexAttribute{
                  GPUVertexFormat::kFloat32x4,
                  4 * sizeof(float),
                  2,
              },
              GPUVertexAttribute{
                  GPUVertexFormat::kFloat32x4,
                  8 * sizeof(float),
                  3,
              },
          },
      },
  };
}

HWFunctionBaseKey WGSLTextGeometry::GetMainKey() const {
  return MakeMainKey(HWGeometryKeyType::kText,
                     data_.layer_to_local.has_value() ? kHasLayerToLocal : 0);
}

HWFunctionBaseKey WGSLTextGeometry::GetFSSubKey() const {
  switch (data_.effect) {
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
  if (data_.layer_to_local) {
    // Bindings 0..4 hold the sampler and four atlas textures in the fragment
    // stage.
    ss << "@group(3) @binding(5) var<uniform> "
          "uTextLayerToLocal: mat4x4<f32>;\n";
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
  if (data_.layer_to_local) {
    ss << R"(
  local_pos = (uTextLayerToLocal * common_slot.userTransform *
               vec4<f32>(pos, 0.0, 1.0)).xy;
)";
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
  if (data_.effect != TextAtlasEffect::kColor &&
      data_.effect != TextAtlasEffect::kColorSwizzleRB) {
    return;
  }

  ss << R"(
  let text_paint_alpha: f32 = color.a;
  color = get_text_atlas_color(input.v_txt_index, input.v_uv);
)";
  if (data_.effect == TextAtlasEffect::kColorSwizzleRB) {
    ss << "  color = color.bgra;\n";
  }
  ss << "  color *= text_paint_alpha;\n";
}

void WGSLTextGeometry::WriteFSAlphaMask(std::stringstream& ss) const {
  if (data_.effect == TextAtlasEffect::kA8Coverage) {
    ss << R"(
  mask_alpha = get_text_atlas_color(input.v_txt_index, input.v_uv).r;
)";
  } else if (data_.effect == TextAtlasEffect::kSDFCoverage) {
    ss << R"(
  var distance: f32 = get_text_atlas_color(input.v_txt_index, input.v_uv).r;
  distance = 7.96875 * (distance - 0.5019608);

  var dist_grad:vec2<f32> = vec2<f32>(dFdx(distance), dFdy(distance));
  let dg_len2: f32 = dot(dist_grad, dist_grad);
  if(dg_len2 < 0.0001) {
    dist_grad = vec2<f32>(0.7071);
  } else {
    dist_grad = dist_grad * inversesqrt(dg_len2);
  }
  let jacobian: mat2x2<f32> = mat2x2<f32>(dFdx(input.v_uv), dFdy(input.v_uv));
  let grad: vec2<f32> = jacobian * dist_grad;
  let afwidth: f32 = 0.65 * length(grad);

  mask_alpha = smoothstep(-afwidth, afwidth, distance);
)";
  }
}

void WGSLTextGeometry::PrepareCMD(Command* cmd, HWDrawContext* context,
                                  const Matrix& transform, float clip_depth,
                                  Command* stencil_cmd) {
  SKITY_TRACE_EVENT(WGSLTextGeometry_PrepareCMD);

  if (cmd == nullptr || cmd->pipeline == nullptr) {
    return;
  }

  cmd->vertex_buffer = context->static_buffer->GetUnitQuadVertexBufferView();
  cmd->index_buffer = context->static_buffer->GetUnitQuadIndexBufferView();
  cmd->index_count = cmd->index_buffer.range / sizeof(uint32_t);

  context->stageBuffer->BeginWritingInstance(
      data_.glyph_rects.size() * sizeof(Instance), alignof(Instance));
  for (auto& glyph_rect : data_.glyph_rects) {
    context->stageBuffer->AppendInstance<Instance>(
        glyph_rect.item.vertex_coord,
        Vec4{glyph_rect.item.texture_coord_tl,
             glyph_rect.item.texture_coord_br},
        glyph_rect.paint.GetColor4f());
  }
  auto instance_buffer_view = context->stageBuffer->EndWritingInstance();
  cmd->instance_count = instance_buffer_view.range / sizeof(Instance);
  cmd->instance_buffer = instance_buffer_view;

  auto pipeline = cmd->pipeline;

  auto group = pipeline->GetBindingGroup(0);
  if (group == nullptr) {
    return;
  }

  // bind CommonSlot
  auto common_slot = group->GetEntry(0);

  if (!SetupCommonInfo(common_slot, context->mvp, transform, clip_depth)) {
    return;
  }

  UploadBindGroup(group->group, common_slot, cmd, context);

  auto geometry_group = pipeline->GetBindingGroup(3);
  if (geometry_group == nullptr) {
    return;
  }

  if (data_.layer_to_local) {
    auto entry = geometry_group->GetEntry(5);
    if (entry == nullptr || entry->type_definition == nullptr ||
        entry->type_definition->name != "mat4x4<f32>") {
      return;
    }
    entry->type_definition->SetData(&*data_.layer_to_local, sizeof(Matrix));
    UploadBindGroup(geometry_group->group, entry, cmd, context);
  }

  if (data_.textures[0] == nullptr) {
    return;
  }
  auto sampler_entry = geometry_group->GetEntry(0);
  if (sampler_entry == nullptr ||
      sampler_entry->type != wgx::BindingType::kSampler) {
    return;
  }
  UploadBindGroup(geometry_group->group, sampler_entry, cmd, data_.sampler);

  std::shared_ptr<GPUTexture> last_texture;
  for (size_t i = 0; i < data_.textures.size(); ++i) {
    auto texture_entry = geometry_group->GetEntry(i + 1);
    if (texture_entry == nullptr ||
        texture_entry->type != wgx::BindingType::kTexture) {
      return;
    }
    if (data_.textures[i] != nullptr) {
      last_texture = data_.textures[i];
    }
    if (last_texture == nullptr) {
      return;
    }
    UploadBindGroup(geometry_group->group, texture_entry, cmd, last_texture);
  }
}

}  // namespace skity
