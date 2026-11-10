// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/fragment/wgsl_texture_fragment.hpp"

#include "src/graphic/color_space_priv.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/tracing.hpp"

namespace skity {

namespace {

// Emits the color space decode block: unpremultiply, linearize with the
// parametric transfer function, move to the working (sRGB) gamut, encode
// with the fixed sRGB curve, premultiply again. Only generated for the
// non-sRGB shader variant, so it runs unconditionally.
void WriteColorSpaceDecode(std::stringstream& ss) {
  ss << R"(
      {
        var cs_rgb: vec3<f32> = color.xyz;
        if (image_color_info.infos.x != 3 && color.w > 0.0) {
          cs_rgb = cs_rgb / color.w;
        }
        cs_rgb = vec3<f32>(
            cs_linearize_channel(cs_rgb.r, image_color_info.tf0, image_color_info.tf1),
            cs_linearize_channel(cs_rgb.g, image_color_info.tf0, image_color_info.tf1),
            cs_linearize_channel(cs_rgb.b, image_color_info.tf0, image_color_info.tf1));
        cs_rgb = (image_color_info.gamut * vec4<f32>(cs_rgb, 1.0)).xyz;
        cs_rgb = vec3<f32>(cs_srgb_encode_channel(cs_rgb.r),
                           cs_srgb_encode_channel(cs_rgb.g),
                           cs_srgb_encode_channel(cs_rgb.b));
        if (image_color_info.infos.x != 3 && color.w > 0.0) {
          cs_rgb = cs_rgb * color.w;
        }
        color = vec4<f32>(cs_rgb, color.w);
      }
  )";
}

}  // namespace

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
      height_(height),
      cubic_(shader->GetSamplingOptions()->cubic) {
  auto image = shader->AsImage();
  if (image == nullptr) {
    return;
  }

  alpha_type_ = (*image)->GetAlphaType();
  color_space_ = (*image)->GetColorSpace();
  if (!ColorSpaceIsSRGB(color_space_)) {
    // The GPU texture keeps the source-space encoded bytes; a decode
    // variant of the shader converts them, so precombine the src -> working
    // (sRGB) gamut matrix. sRGB images generate the plain shader instead.
    has_color_space_decode_ = true;
    float inv_working[9];
    float to_working[9];
    ColorSpaceInvert3x3(ColorSpace{}.gamut_xyz, inv_working);
    ColorSpaceMultiply3x3(inv_working, color_space_->gamut_xyz, to_working);
    gamut_to_working_ =
        Matrix{to_working[0], to_working[3], to_working[6], 0.f,  //
               to_working[1], to_working[4], to_working[7], 0.f,  //
               to_working[2], to_working[5], to_working[8], 0.f,  //
               0.f,           0.f,           0.f,           1.f};
  }
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
  if (cubic_.UseCubic()) {
    ss << R"(
      // Mitchell-Netravali cubic convolution weight at distance x (x >= 0).
      fn cubic_weight(x: f32, B: f32, C: f32) -> f32 {
          var ax: f32 = abs(x);
          var ax2: f32 = ax * ax;
          var ax3: f32 = ax2 * ax;
          var w01: f32 = ((12.0 - 9.0 * B - 6.0 * C) * ax3 +
                          (-18.0 + 12.0 * B + 6.0 * C) * ax2 + (6.0 - 2.0 * B)) /
                         6.0;
          var w12: f32 = ((-B - 6.0 * C) * ax3 + (6.0 * B + 30.0 * C) * ax2 +
                          (-12.0 * B - 48.0 * C) * ax + (8.0 * B + 24.0 * C)) /
                         6.0;
          // Three polynomial pieces: |x| < 1 -> w01, |x| < 2 -> w12, else 0.
          if (ax < 1.0) {
              return w01;
          } else if (ax < 2.0) {
              return w12;
          }
          return 0.0;
      }

      // Fast cubic on one axis: fold the 4 cubic taps into 2 bilinear samples.
      // Returns (uvA, uvB, cA, cB): the two bilinear sample coordinates and
      // their combining weights. Exact for all-positive kernels (B-spline); a
      // close approximation for kernels with negative lobes (Catmull-Rom),
      // traded for 4x fewer samples than a 16-tap convolution.
      fn cubic_sample_1d(coord: f32, dim: f32, B: f32, C: f32) -> vec4<f32> {
          var p: f32 = coord * dim;
          var it: f32 = floor(p - 0.5);
          var f: f32 = p - 0.5 - it;
          var wm1: f32 = cubic_weight(f + 1.0, B, C);
          var w0: f32 = cubic_weight(f, B, C);
          var w1: f32 = cubic_weight(1.0 - f, B, C);
          var w2: f32 = cubic_weight(2.0 - f, B, C);
          var cA: f32 = wm1 + w0;
          var cB: f32 = w1 + w2;
          // Guard division by zero (e.g. Catmull-Rom at f==0 gives cB==0).
          var uvA: f32 = (it - 0.5 + w0 / (cA + 0.000001)) / dim;
          var uvB: f32 = (it + 1.5 + w2 / (cB + 0.000001)) / dim;
          return vec4<f32>(uvA, uvB, cA, cB);
      }
    )";
  }
  ss << R"(
    struct ImageColorInfo {
        infos           : vec3<i32>,
        global_alpha    : f32,
  )";
  if (cubic_.UseCubic()) {
    ss << R"(        cubic           : vec2<f32>,
)";
  }
  if (has_color_space_decode_) {
    ss << R"(
        tf0             : vec4<f32>,
        tf1             : vec3<f32>,
        gamut           : mat4x4<f32>,
    };

    // Color space decode: linearize with the parametric transfer function
    // (tf0 = {g, a, b, c}, tf1 = {d, e, f}), move to the working-space
    // (sRGB) gamut, then encode with the fixed sRGB curve. The parameters
    // are passed as arguments because the uniforms are declared after the
    // functions in the generated shader.
    fn cs_linearize_channel(x: f32, tf0: vec4<f32>, tf1: vec3<f32>) -> f32 {
        if (x < tf1.x) {
            return tf0.w * x + tf1.z;
        }
        return pow(max(tf0.y * x + tf0.z, 0.0), tf0.x) + tf1.y;
    }

    fn cs_srgb_encode_channel(x: f32) -> f32 {
        if (x <= 0.0031308) {
            return x * 12.92;
        }
        return 1.055 * pow(x, 1.0 / 2.4) - 0.055;
    }
    )";
  } else {
    ss << R"(    };
    )";
  }
}

void WGSLTextureFragment::WriteFSUniforms(std::stringstream& ss) const {
  ss << R"(
    @group(1) @binding(0) var<uniform>  image_color_info    : ImageColorInfo;
    @group(1) @binding(1) var           uSampler            : sampler;
    @group(1) @binding(2) var           uTexture            : texture_2d<f32>;
  )";
}

void WGSLTextureFragment::WriteFSMain(std::stringstream& ss) const {
  if (cubic_.UseCubic()) {
    ss << R"(
      var frag_coord: vec2<f32> = input.f_frag_coord;

      var dim: vec2<u32> = textureDimensions(uTexture);
      var dimx: f32 = f32(dim.x);
      var dimy: f32 = f32(dim.y);
      var B: f32 = image_color_info.cubic.x;
      var C: f32 = image_color_info.cubic.y;

      var ux: f32 = remap_float_tile(frag_coord.x, image_color_info.infos.y);
      var uy: f32 = remap_float_tile(frag_coord.y, image_color_info.infos.z);

      // Fast bicubic: fold the 4 cubic taps on each axis into 2 bilinear
      // samples, then combine the 2x2 = 4 samples (vs 16 nearest taps).
      // NOTE: every textureSample must run in uniform control flow (WGSL
      // forbids sampling inside non-uniform branches), so -- like the
      // non-cubic path -- the decal early-out is evaluated AFTER sampling.
      var px: vec4<f32> = cubic_sample_1d(ux, dimx, B, C);
      var py: vec4<f32> = cubic_sample_1d(uy, dimy, B, C);

      var sAA: vec4<f32> = textureSample(uTexture, uSampler, vec2<f32>(remap_float_tile(px.x, image_color_info.infos.y), remap_float_tile(py.x, image_color_info.infos.z)));
      var sAB: vec4<f32> = textureSample(uTexture, uSampler, vec2<f32>(remap_float_tile(px.x, image_color_info.infos.y), remap_float_tile(py.y, image_color_info.infos.z)));
      var sBA: vec4<f32> = textureSample(uTexture, uSampler, vec2<f32>(remap_float_tile(px.y, image_color_info.infos.y), remap_float_tile(py.x, image_color_info.infos.z)));
      var sBB: vec4<f32> = textureSample(uTexture, uSampler, vec2<f32>(remap_float_tile(px.y, image_color_info.infos.y), remap_float_tile(py.y, image_color_info.infos.z)));

      color = px.z * py.z * sAA + px.z * py.w * sAB + px.w * py.z * sBA + px.w * py.w * sBB;

      if (image_color_info.infos.y == 3 && (frag_coord.x < 0.0 || frag_coord.x >= 1.0)) || (image_color_info.infos.z == 3 && (frag_coord.y < 0.0 || frag_coord.y >= 1.0))
      {
        color = vec4<f32>(0.0, 0.0, 0.0, 0.0);
      }

      )";
    if (has_color_space_decode_) {
      WriteColorSpaceDecode(ss);
    }
    ss << R"(
      if image_color_info.infos.x == 3 {
        color = vec4<f32>(color.xyz * color.w, color.w);
      }

      color *= image_color_info.global_alpha;
    )";
    return;
  }
  ss << R"(
    var frag_coord: vec2<f32> = input.f_frag_coord;

    var uv  : vec2<f32> = frag_coord;

    uv.x = remap_float_tile(uv.x, image_color_info.infos.y);
    uv.y = remap_float_tile(uv.y, image_color_info.infos.z);

    color = textureSample(uTexture, uSampler, uv);

    if (image_color_info.infos.y == 3 && (frag_coord.x < 0.0 || frag_coord.x >= 1.0)) || (image_color_info.infos.z == 3 && (frag_coord.y < 0.0 || frag_coord.y >= 1.0))
    {
      color = vec4<f32>(0.0, 0.0, 0.0, 0.0);
    }

    )";
  if (has_color_space_decode_) {
    WriteColorSpaceDecode(ss);
  }
  ss << R"(
    if image_color_info.infos.x == 3 {
      color = vec4<f32>(color.xyz * color.w, color.w);
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

HWFunctionBaseKey WGSLTextureFragment::GetMainKey() const {
  // Variant bits keep each generated shader cached independently: bit 0 is
  // the bicubic path, bit 1 the non-sRGB color space decode. sRGB images
  // without cubic sampling keep the plain (pre-color-space) key.
  uint32_t variant = 0;
  if (cubic_.UseCubic()) {
    variant |= 1;
  }
  if (has_color_space_decode_) {
    variant |= 2;
  }
  if (variant == 0) {
    return HWFragmentKeyType::kTexture;
  }
  return MakeMainKey(HWFragmentKeyType::kTexture, variant);
}

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

    if (has_color_space_decode_) {
      // Color space decode parameters for the non-sRGB shader variant.
      const float* tf = color_space_->transfer_fn;
      std::array<float, 4> tf0{tf[0], tf[1], tf[2], tf[3]};
      std::array<float, 3> tf1{tf[4], tf[5], tf[6]};
      image_color_info_struct->GetMember("tf0")->type->SetData(
          tf0.data(), tf0.size() * sizeof(float));
      image_color_info_struct->GetMember("tf1")->type->SetData(
          tf1.data(), tf1.size() * sizeof(float));
      image_color_info_struct->GetMember("gamut")->type->SetData(
          &gamut_to_working_, sizeof(Matrix));
    }

    if (cubic_.UseCubic()) {
      std::array<float, 2> cubic{{cubic_.B, cubic_.C}};
      image_color_info_struct->GetMember("cubic")->type->SetData(
          cubic.data(), sizeof(float) * cubic.size());
    }

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

HWFunctionBaseKey WGSLTextureFragment::GetVSSubKey() const {
  return HWFragmentKeyType::kTexture;
}

}  // namespace skity
