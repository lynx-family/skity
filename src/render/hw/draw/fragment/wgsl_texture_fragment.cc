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
                                         float global_alpha)
    : x_tile_mode_(shader->GetXTileMode()),
      y_tile_mode_(shader->GetYTileMode()),
      texture_(std::move(texture)),
      sampler_(std::move(sampler)),
      global_alpha_(global_alpha) {
  auto image = shader->AsImage();
  if (image == nullptr) {
    return;
  }

  alpha_type_ = (*image)->GetAlphaType();
}

WGSLTextureFragment::WGSLTextureFragment(AlphaType alpha_type,
                                         TileMode x_tile_mode,
                                         TileMode y_tile_mode,
                                         std::shared_ptr<GPUTexture> texture,
                                         std::shared_ptr<GPUSampler> sampler,
                                         float global_alpha)
    : alpha_type_(alpha_type),
      x_tile_mode_(x_tile_mode),
      y_tile_mode_(y_tile_mode),
      texture_(std::move(texture)),
      sampler_(std::move(sampler)),
      global_alpha_(global_alpha) {}

uint32_t WGSLTextureFragment::NextBindingIndex() const { return 3; }

std::string WGSLTextureFragment::GenSourceWGSL() const {
  std::string wgsl_code = RemapTileFunction();

  wgsl_code += R"(
    struct ImageColorInfo {
        infos           : vec3<i32>,
        global_alpha    : f32,
    };

    @group(1) @binding(0) var<uniform>  image_color_info    : ImageColorInfo;
    @group(1) @binding(1) var           uSampler            : sampler;
    @group(1) @binding(2) var           uTexture            : texture_2d<f32>;
  )";

  if (filter_ != nullptr) {
    wgsl_code += filter_->GenSourceWGSL();
  }

  if (contour_aa_) {
    wgsl_code += R"(
      struct ImageAAFSInput {
        @location(0) frag_coord : vec2<f32>,
        @location(1) v_pos_aa   : f32,
      };

      @fragment
      fn fs_main(input: ImageAAFSInput) -> @location(0) vec4<f32> {
        var frag_coord: vec2<f32> = input.frag_coord;
    )";
  } else {
    wgsl_code += R"(
      @fragment
      fn fs_main(@location(0) frag_coord: vec2<f32>) -> @location(0) vec4<f32> {
    )";
  }

  wgsl_code += R"(
        var uv  : vec2<f32> = frag_coord;

        if (image_color_info.infos.y == 3 && (uv.x < 0.0 || uv.x >= 1.0)) || (image_color_info.infos.z == 3 && (uv.y < 0.0 || uv.y >= 1.0))
        {
            return vec4<f32>(0.0, 0.0, 0.0, 0.0);
        }

        uv.x = remap_float_tile(uv.x, image_color_info.infos.y);
        uv.y = remap_float_tile(uv.y, image_color_info.infos.z);

        var color : vec4<f32> = textureSample(uTexture, uSampler, uv);

        if image_color_info.infos.x == 3 {
            color.xyz *= color.w;
        }

        color *= image_color_info.global_alpha;
  )";

  if (filter_ != nullptr) {
    wgsl_code += R"(
      color = filter_color(color);
    )";
  }

  if (contour_aa_) {
    wgsl_code += R"(
      color *= input.v_pos_aa;
    )";
  }

  wgsl_code += R"(
        return color;
    }
  )";

  return wgsl_code;
}

std::string WGSLTextureFragment::GetShaderName() const {
  std::string name = "TextureFragmentWGSL";

  if (filter_ != nullptr) {
    name += "_" + filter_->GetShaderName();
  }

  if (contour_aa_) {
    name += "_AA";
  }

  return name;
}

const char* WGSLTextureFragment::GetEntryPoint() const { return "fs_main"; }

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

    UploadBindGroup(image_color_info_entry, cmd, context);
  }

  auto sampler_binding = group->GetEntry(1);
  auto texture_binding = group->GetEntry(2);

  if (sampler_binding == nullptr ||
      sampler_binding->type != wgx::BindingType::kSampler ||
      texture_binding == nullptr ||
      texture_binding->type != wgx::BindingType::kTexture) {
    return;
  }

  UploadBindGroup(sampler_binding, cmd, sampler_);
  UploadBindGroup(texture_binding, cmd, texture_);

  if (filter_ != nullptr) {
    filter_->SetupBindGroup(cmd, context);
  }
}

}  // namespace skity
