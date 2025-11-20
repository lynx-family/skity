// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/fragment/wgsl_texture_fragment.hpp"

#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/tracing.hpp"

namespace skity {

WGSLTextureFragment::WGSLTextureFragment(std::shared_ptr<PixmapShader> shader,
                                         std::shared_ptr<GPUTexture> texture,
                                         std::shared_ptr<GPUSampler> sampler,
                                         float global_alpha,
                                         const Matrix& local_matrix,
                                         float width, float height)
    : HWWGSLFragment(Flags::kSnippet | Flags::kAffectsVertex),
      x_tile_mode_(shader->GetXTileMode()),
      y_tile_mode_(shader->GetYTileMode()),
      texture_(std::move(texture)),
      sampler_(std::move(sampler)),
      global_alpha_(global_alpha),
      local_matrix_(local_matrix),
      width_(width),
      height_(height) {
  auto image = shader->AsImage();
  if (image == nullptr) {
    return;
  }

  alpha_type_ = (*image)->GetAlphaType();
}

WGSLTextureFragment::WGSLTextureFragment(
    AlphaType alpha_type, TileMode x_tile_mode, TileMode y_tile_mode,
    std::shared_ptr<GPUTexture> texture, std::shared_ptr<GPUSampler> sampler,
    float global_alpha, const Matrix& local_matrix, float width, float height)
    : HWWGSLFragment(Flags::kSnippet | Flags::kAffectsVertex),
      alpha_type_(alpha_type),
      x_tile_mode_(x_tile_mode),
      y_tile_mode_(y_tile_mode),
      texture_(std::move(texture)),
      sampler_(std::move(sampler)),
      global_alpha_(global_alpha),
      local_matrix_(local_matrix),
      width_(width),
      height_(height) {}

uint32_t WGSLTextureFragment::NextBindingIndex() const { return 3; }

void WGSLTextureFragment::WriteFSFunctionsAndStructs(
    std::stringstream& ss) const {
  ss << RemapTileFunction();
  ss << R"(
    struct ImageColorInfo {
        infos           : vec3<i32>,
        global_alpha    : f32,
    };
  )";
}

void WGSLTextureFragment::WriteFSUniforms(std::stringstream& ss) const {
  ss << R"(
    @group(1) @binding(0) var<uniform>  image_color_info    : ImageColorInfo;
    @group(1) @binding(1) var           uSampler            : sampler;
    @group(1) @binding(2) var           uTexture            : texture_2d<f32>;
  )";
}

void WGSLTextureFragment::WriteFSMain(std::stringstream& ss) const {
  ss << R"(
    var frag_coord: vec2<f32> = input.f_frag_coord;

    var uv  : vec2<f32> = frag_coord;

    if (image_color_info.infos.y == 3 && (uv.x < 0.0 || uv.x >= 1.0)) || (image_color_info.infos.z == 3 && (uv.y < 0.0 || uv.y >= 1.0))
    {
      return vec4<f32>(0.0, 0.0, 0.0, 0.0);
    }

    uv.x = remap_float_tile(uv.x, image_color_info.infos.y);
    uv.y = remap_float_tile(uv.y, image_color_info.infos.z);

    color = textureSample(uTexture, uSampler, uv);

    if image_color_info.infos.x == 3 {
      color.xyz *= color.w;
    }

    color *= image_color_info.global_alpha;
  )";
}

std::optional<std::vector<std::string>> WGSLTextureFragment::GetVarings()
    const {
  return std::vector<std::string>{"f_frag_coord: vec2<f32>"};
}

void WGSLTextureFragment::WriteVSFunctionsAndStructs(
    std::stringstream& ss) const {
  ss << R"(
    struct ImageBoundsInfo {
      bounds      : vec2<f32>,
      inv_matrix  : mat4x4<f32>,
    };
  )";
}

void WGSLTextureFragment::WriteVSUniforms(std::stringstream& ss) const {
  ss << R"(
    @group(0) @binding(1) var<uniform> image_bounds : ImageBoundsInfo;
  )";
}

void WGSLTextureFragment::WriteVSAssgnShadingVarings(
    std::stringstream& ss) const {
  ss << R"(
  {
    var mapped_pos  : vec2<f32>     = (image_bounds.inv_matrix * vec4<f32>(local_pos.xy, 0.0, 1.0)).xy;
    var mapped_lt   : vec2<f32>     = vec2<f32>(0.0, 0.0);
    var mapped_rb   : vec2<f32>     = image_bounds.bounds;
    var total_x     : f32           = mapped_rb.x - mapped_lt.x;
    var total_y     : f32           = mapped_rb.y - mapped_lt.y;
    var v_x         : f32           = (mapped_pos.x - mapped_lt.x) / total_x;
    var v_y         : f32           = (mapped_pos.y - mapped_lt.y) / total_y;
    output.f_frag_coord = vec2<f32>(v_x, v_y);
  }
)";
}

void WGSLTextureFragment::BindVSUniforms(Command* cmd, HWDrawContext* context,
                                         const Matrix& transform,
                                         float clip_depth,
                                         Command* stencil_cmd) {
  if (cmd->pipeline == nullptr) {
    return;
  }

  auto group = cmd->pipeline->GetBindingGroup(0);
  if (group == nullptr) {
    return;
  }

  auto image_bounds_entry = group->GetEntry(1);
  if (!SetupImageBoundsInfo(image_bounds_entry, local_matrix_, width_,
                            height_)) {
    return;
  }
  UploadBindGroup(group->group, image_bounds_entry, cmd, context);
}

std::string WGSLTextureFragment::GetShaderName() const { return "Texture"; }

void WGSLTextureFragment::PrepareCMD(Command* cmd, HWDrawContext* context) {
  SKITY_TRACE_EVENT(WGSLTextureFragment_PrepareCMD);

  if (cmd->pipeline == nullptr) {
    return;
  }

  auto group = cmd->pipeline->GetBindingGroup(1);
  if (group == nullptr) {
    return;
  }

  // ImageColorInfo
  {
    auto image_color_info_entry = group->GetEntry(0);

    if (image_color_info_entry == nullptr ||
        image_color_info_entry->type_definition->name != "ImageColorInfo") {
      return;
    }

    auto image_color_info_struct = static_cast<wgx::StructDefinition*>(
        image_color_info_entry->type_definition.get());

    std::array<int32_t, 3> infos{};
    infos[0] = alpha_type_;
    infos[1] = static_cast<int32_t>(x_tile_mode_);
    infos[2] = static_cast<int32_t>(y_tile_mode_);

    image_color_info_struct->GetMember("infos")->type->SetData(
        infos.data(), sizeof(int32_t) * infos.size());

    image_color_info_struct->GetMember("global_alpha")
        ->type->SetData(&global_alpha_, sizeof(float));

    UploadBindGroup(group->group, image_color_info_entry, cmd, context);
  }

  auto sampler_binding = group->GetEntry(1);
  auto texture_binding = group->GetEntry(2);

  if (sampler_binding == nullptr ||
      sampler_binding->type != wgx::BindingType::kSampler ||
      texture_binding == nullptr ||
      texture_binding->type != wgx::BindingType::kTexture) {
    return;
  }

  UploadBindGroup(group->group, sampler_binding, cmd, sampler_);
  UploadBindGroup(group->group, texture_binding, cmd, texture_);

  if (filter_ != nullptr) {
    filter_->SetupBindGroup(cmd, context);
  }
}

std::string WGSLTextureFragment::GetVSNameSuffix() const { return "Texture"; }

}  // namespace skity
