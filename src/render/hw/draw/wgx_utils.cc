// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/wgx_utils.hpp"

#include "src/effect/pixmap_shader.hpp"
#include "src/gpu/gpu_context_impl.hpp"
#include "src/gpu/gpu_render_pass.hpp"
#include "src/logging.hpp"
#include "src/render/hw/draw/fragment/wgsl_gradient_fragment.hpp"
#include "src/render/hw/draw/fragment/wgsl_solid_color.hpp"
#include "src/render/hw/draw/fragment/wgsl_stencil_fragment.hpp"
#include "src/render/hw/draw/fragment/wgsl_texture_fragment.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"

namespace skity {

void UploadBindGroup(const wgx::BindGroupEntry* entry, Command* cmd,
                     HWDrawContext* ctx) {
  if (entry->type != wgx::BindingType::kUniformBuffer) {
    return;
  }

  auto allocation =
      ctx->stageBuffer->Allocate(entry->type_definition->size, true);
  entry->type_definition->WriteToBuffer(allocation.addr, 0);
  cmd->uniform_bindings.emplace_back(UniformBinding{
      ToShaderStage(entry->stage),
      entry->index,
      entry->name,
      GPUBufferView{
          ctx->stageBuffer->GetGPUBuffer(),
          allocation.offset,
          allocation.size,
      },
  });
}

void UploadBindGroup(const wgx::BindGroupEntry* entry, Command* cmd,
                     const std::shared_ptr<GPUSampler>& sampler) {
  if (entry->type != wgx::BindingType::kSampler) {
    return;
  }

  cmd->sampler_bindings.emplace_back(SamplerBinding{
      ToShaderStage(entry->stage),
      entry->index,
      entry->units,
      entry->name,
      sampler,
  });
}

void UploadBindGroup(const wgx::BindGroupEntry* entry, Command* cmd,
                     const std::shared_ptr<GPUTexture>& texture) {
  if (entry->type != wgx::BindingType::kTexture) {
    return;
  }

  cmd->texture_bindings.emplace_back(TextureBinding{
      ToShaderStage(entry->stage),
      entry->index,
      entry->name,
      texture,
  });
}

GPUShaderStageMask ToShaderStage(wgx::ShaderStage stage) {
  uint32_t mask = 0;

  if (stage & wgx::ShaderStage::ShaderStage_kVertex) {
    mask |= static_cast<uint32_t>(GPUShaderStage::kVertex);
  }

  if (stage & wgx::ShaderStage::ShaderStage_kFragment) {
    mask |= static_cast<uint32_t>(GPUShaderStage::kFragment);
  }

  return mask;
}

const char* RemapTileFunction() {
  return R"(
    fn remap_float_tile(t: f32, tile_mode: i32) -> f32 {
        if tile_mode == 0 {
            return clamp(t, 0.0, 1.0);
        } else if tile_mode == 1 {
            return fract(t);
        } else if tile_mode == 2 {
            var t1: f32 = t - 1.0;
            var t2: f32 = t1 - 2.0 * floor(t1 / 2.0) - 1.0;

            return abs(t2);
        }
        return t;
    }
  )";
}

const char* CommonVertexWGSL() {
  return R"(
struct CommonSlot {
    mvp           : mat4x4<f32>,
    userTransform : mat4x4<f32>,
    extraInfo     : vec4<f32>,
};

fn get_vertex_position(a_pos: vec2<f32>, cs: CommonSlot) -> vec4<f32> {
    var pos: vec4<f32> = cs.mvp * cs.userTransform * vec4<f32>(a_pos, 0.0, 1.0);
    return vec4<f32>(pos.x, pos.y, cs.extraInfo[0] * pos.w, pos.w);
}
  )";
}

bool SetupCommonInfo(const wgx::BindGroupEntry* entry, const Matrix& mvp,
                     const Matrix& user_transform, float clip_depth) {
  if (entry == nullptr || entry->type_definition == nullptr ||
      entry->type_definition->name != "CommonSlot") {
    return false;
  }

  auto common_info =
      static_cast<wgx::StructDefinition*>(entry->type_definition.get());

  common_info->GetMember("mvp")->type->SetData(&mvp, sizeof(Matrix));
  common_info->GetMember("userTransform")
      ->type->SetData(&user_transform, sizeof(Matrix));
  common_info->GetMember("extraInfo")
      ->type->SetData(std::array<float, 4>{clip_depth, 0.f, 0.f, 0.f});

  return true;
}

bool SetupInvMatrix(const wgx::BindGroupEntry* inv_matrix_entry,
                    const Matrix& local_matrix) {
  if (inv_matrix_entry == nullptr ||
      inv_matrix_entry->type_definition->name != "mat4x4<f32>") {
    return false;
  }

  Matrix inv_matrix{};

  local_matrix.Invert(&inv_matrix);

  inv_matrix_entry->type_definition->SetData(&inv_matrix, sizeof(Matrix));
  return true;
}

bool SetupImageBoundsInfo(const wgx::BindGroupEntry* image_bounds_entry,
                          const Matrix& local_matrix, float width,
                          float height) {
  if (image_bounds_entry == nullptr ||
      image_bounds_entry->type_definition->name != "ImageBoundsInfo") {
    return false;
  }

  auto image_bounds_struct = static_cast<wgx::StructDefinition*>(
      image_bounds_entry->type_definition.get());

  std::array<float, 2> bounds{width, height};
  image_bounds_struct->GetMember("bounds")->type->SetData(
      bounds.data(), bounds.size() * sizeof(float));

  image_bounds_struct->GetMember("inv_matrix")
      ->type->SetData(&local_matrix, sizeof(Matrix));
  return true;
}

WGXGradientFragment::WGXGradientFragment(const Shader::GradientInfo& info,
                                         Shader::GradientType type)
    : info_(info), type_(type), max_color_count_(RoundGradientColorCount()) {}

std::string WGXGradientFragment::GenSourceWGSL(size_t index) const {
  std::string wgsl = GenerateGradientCommonWGSL(index);

  if (type_ == Shader::kConical) {
    wgsl += R"(
      struct ConicalInfo {
        center1 : vec2<f32>,
        center2 : vec2<f32>,
        radius1 : f32,
        radius2 : f32,
      };

      fn _wgx_inverse_3x3_f32(m: mat3x3<f32>) -> mat3x3<f32> {
        var adj: mat3x3<f32>;

        adj[0][0] =   (m[1][1] * m[2][2] - m[2][1] * m[1][2]);
        adj[1][0] = - (m[1][0] * m[2][2] - m[2][0] * m[1][2]);
        adj[2][0] =   (m[1][0] * m[2][1] - m[2][0] * m[1][1]);
        adj[0][1] = - (m[0][1] * m[2][2] - m[2][1] * m[0][2]);
        adj[1][1] =   (m[0][0] * m[2][2] - m[2][0] * m[0][2]);
        adj[2][1] = - (m[0][0] * m[2][1] - m[2][0] * m[0][1]);
        adj[0][2] =   (m[0][1] * m[1][2] - m[1][1] * m[0][2]);
        adj[1][2] = - (m[0][0] * m[1][2] - m[1][0] * m[0][2]);
        adj[2][2] =   (m[0][0] * m[1][1] - m[1][0] * m[0][1]);

        let det: f32 = (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
            - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
            + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]));

        return adj * (1.0 / det);
      }

      fn map_to_unit_x(p0: vec2<f32>, p1: vec2<f32>) -> mat3x3<f32> {
        // Returns a matrix that maps [p0, p1] to [(0, 0), (1, 0)]. Results are
        // undefined if p0 = p1.
        return
          mat3x3<f32>(0.0, -1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0)
            *
          _wgx_inverse_3x3_f32(mat3x3<f32>(p1.y - p0.y, p0.x - p1.x, 0.0, p1.x - p0.x, p1.y - p0.y, 0.0, p0.x, p0.y, 1.0));
      }

      fn compute_conical_t(c00: vec2<f32>, r0: f32, c11: vec2<f32>, r1: f32, pos: vec2<f32>) -> vec2<f32> {
        var c0                  : vec2<f32>  = c00;
        var c1                  : vec2<f32>  = c11;
        var scalar_nearly_zero  : f32 = 1.0 / f32(1 << 12);
        var d_center            : f32 = distance(c0, c1);
        var d_radius            : f32 = r1 - r0;

        // Degenerate case: a radial gradient (p0 = p1)
        var radial              : bool = abs(d_center) < scalar_nearly_zero;

        // Degenerate case: a strip with bandwidth 2r (r0 = r1).
        var strip               : bool = abs(d_radius) < scalar_nearly_zero;

        if radial {
          if strip {
            return vec2<f32>(0.0, -1.0);
          }

          var scale              : f32        = 1.0 / d_radius;
          var scale_sign         : f32        = sign(d_radius);
          var bias               : f32        = r0 / d_radius;

          var pt                 : vec2<f32>  = (pos - c0) * scale;
          var t                  : f32        = length(pt) * scale_sign - bias;

          return vec2<f32>(t, 1.0);
        } else if strip {
          var transform          : mat3x3<f32>  = map_to_unit_x(c0, c1);
          var r                  : f32          = r0 / d_center;
          var r_2                : f32          = r * r;

          var pt                 : vec2<f32>    = (transform * vec3<f32>(pos, 1.0)).xy;
          var t                  : f32          = r_2 - pt.y * pt.y;

          if t < 0.0 {
            return vec2<f32>(0.0, -1.0);
          }

          t = pt.x + sqrt(t);

          return vec2<f32>(t, 1.0);
        } else {
          // See https://skia.org/docs/dev/design/conical/ for details on how this
          // algorithm works. Calculate f and swap inputs if necessary (steps 1 and 2).

          var f                 : f32       = r0 / (r0 - r1);

          var is_swapped        : bool      = abs(f - 1.0) < scalar_nearly_zero;
          if is_swapped {
            var tmp_pt          : vec2<f32> = c0;

            c0  = c1;
            c1  = tmp_pt;
            f   = 0.0;
          }

          // Apply mapping from [Cf, C1] to unit x, and apply the precalculations from
          // steps 3 and 4, all in the same transformation.

          var cf                : vec2<f32>     = c0 * (1.0 - f) + c1 * f;
          var transform         : mat3x3<f32>   = map_to_unit_x(cf, c1);

          var scale_x           : f32           = abs(1.0 - f);
          var scale_y           : f32           = scale_x;
          var r11                : f32          = abs(r1 - r0) / d_center;
          var is_focal_on_circle: bool          = abs(r11 - 1.0) < scalar_nearly_zero;

          if is_focal_on_circle {
            scale_x *= 0.5;
            scale_y *= 0.5;
          } else {
            scale_x *= r11 / (r11 * r11 - 1.0);
            scale_y /= sqrt(abs(r11 * r11 - 1.0));
          }

          transform = mat3x3<f32>(scale_x, 0.0, 0.0, 0.0, scale_y, 0.0, 0.0, 0.0, 1.0) * transform;

          var pt                : vec2<f32>     = (transform * vec3<f32>(pos, 1.0)).xy;

          // Continue with step 5 onward.

          var inv_r1            : f32           = 1.0 / r11;
          var d_radius_sign     : f32           = sign(1.0 - f);
          var is_well_behaved   : bool          = !is_focal_on_circle && r11 > 1.0;

          var x_t               : f32           = -1.0;
          if is_focal_on_circle {
            x_t = dot(pt, pt) / pt.x;
          } else if is_well_behaved {
            x_t = length(pt) - pt.x * inv_r1;
          } else {
            var temp            : f32           = pt.x * pt.x - pt.y * pt.y;
            if temp >= 0.0 {
              if is_swapped || d_radius_sign < 0.0 {
                x_t = -sqrt(temp) - pt.x * inv_r1;
              } else {
                x_t = sqrt(temp) - pt.x * inv_r1;
              }
            }
          }

          if !is_well_behaved && x_t < 0.0 {
            return vec2<f32>(0.0, -1.0);
          }

          var t                : f32 = f + d_radius_sign * x_t;

          if is_swapped {
            t = 1.0 - t;
          }

          return vec2<f32>(t, 1.0);
        }
      }

      fn calculate_conical_t(currentPoint: vec2<f32>, c0: vec2<f32>, c1: vec2<f32>, r0: f32, r1: f32) -> vec2<f32> {
        var p     : vec2<f32> = currentPoint;
        var res   : vec2<f32> = compute_conical_t(c0, r0, c1, r1, p);
        return res;
      }
    )";
  }

  return wgsl;
}

std::string WGXGradientFragment::GetShaderName() const {
  std::string name = "Gradient";
  name += GradientTypeName();
  name += std::to_string(max_color_count_);
  if (info_.color_offsets.empty()) {
    name += "OffsetFast";
  }

  if (info_.color_count == 2) {
    name += "ColorFast";
  }

  return name;
}

bool WGXGradientFragment::SetupCommonInfo(const wgx::BindGroupEntry* info_entry,
                                          float global_alpha) const {
  if (info_entry->type_definition == nullptr ||
      info_entry->type_definition->name != "GradientInfo") {
    return false;
  }

  auto gradient_info_struct =
      static_cast<wgx::StructDefinition*>(info_entry->type_definition.get());

  std::array<int32_t, 4> infos{
      static_cast<int32_t>(info_.color_count),
      static_cast<int32_t>(info_.color_offsets.size()),
      static_cast<int32_t>(info_.tile_mode),
      0,
  };

  gradient_info_struct->GetMember("infos")->type->SetData(
      infos.data(), infos.size() * sizeof(int32_t));

  auto colors = static_cast<wgx::ArrayDefinition*>(
      gradient_info_struct->GetMember("colors")->type);

  for (size_t i = 0; i < info_.colors.size(); i++) {
    colors->SetDataAt(i, &info_.colors[i], sizeof(float) * 4);
  }

  if (!info_.color_offsets.empty()) {
    auto stops = static_cast<wgx::ArrayDefinition*>(
        gradient_info_struct->GetMember("stops")->type);

    size_t i = 0;
    for (; i < info_.color_offsets.size(); i += 4) {
      std::array<float, 4> stop{};

      for (size_t j = 0; j < 4; j++) {
        if (i + j < info_.color_offsets.size()) {
          stop[j] = info_.color_offsets[i + j];
        } else {
          stop[j] = 1.0;
        }
      }

      stops->SetDataAt(static_cast<uint32_t>(i / 4), stop);
    }
  }

  gradient_info_struct->GetMember("global_alpha")->type->SetData(global_alpha);

  return true;
}

bool WGXGradientFragment::SetupGradientInfo(
    const wgx::BindGroupEntry* info_entry) const {
  if (type_ == Shader::kLinear) {
    return SetupLinearInfo(info_entry);
  } else if (type_ == Shader::kRadial) {
    return SetupRadialInfo(info_entry);
  } else if (type_ == Shader::kConical) {
    return SetupConicalInfo(info_entry);
  } else if (type_ == Shader::kSweep) {
    return SetupSweepInfo(info_entry);
  }

  return false;
}

const char* WGXGradientFragment::GradientTypeName() const {
  switch (type_) {
    case Shader::kLinear:
      return "Linear";
    case Shader::kRadial:
      return "Radial";
    case Shader::kConical:
      return "Conical";
    case Shader::kSweep:
      return "Sweep";
    default:
      return "Unknown";
  }
}

uint32_t WGXGradientFragment::GetOffsetCount() const {
  return (max_color_count_ + 3) / 4;
}

uint32_t WGXGradientFragment::RoundGradientColorCount() const {
  uint32_t count = static_cast<uint32_t>(info_.colors.size());

  uint32_t round_count = 1;
  while (round_count < count) {
    round_count <<= 1;
  }

  if (round_count > 64) {
    round_count = 64;
  }

  return round_count;
}

std::string WGXGradientFragment::GenerateGradientCommonWGSL(
    size_t index) const {
  std::string wgsl = R"(
    struct GradientInfo {
      infos : vec4<i32>,
      colors: array<vec4<f32>, )";

  wgsl += std::to_string(max_color_count_);
  wgsl += ">,\n";

  if (!info_.color_offsets.empty()) {
    wgsl += "stops: array<vec4<f32>, ";
    wgsl += std::to_string(GetOffsetCount());
    wgsl += ">,\n";
  }

  wgsl += "global_alpha: f32,\n";
  wgsl += "};\n";

  wgsl += RemapTileFunction();

  wgsl += "\n @group(1) @binding(";
  wgsl += std::to_string(index);
  wgsl += ") var<uniform> gradient_info    : GradientInfo;\n";

  if (info_.color_offsets.empty()) {
    wgsl += R"(
      fn get_stop(index: i32) -> f32 {
    )";

    wgsl += R"(
        var colorCount: i32 = gradient_info.infos.x;
        var step: f32 = 1.0 / f32(colorCount - 1);
        return step * f32(index);
      }
    )";
  } else {
    wgsl += R"(
      fn get_stop(index: i32) -> f32 {
        var batchIndex: i32 = index / 4;
        var batchOffset: i32 = index % 4;

        var offset: vec4<f32> = gradient_info.stops[batchIndex];
        return offset[batchOffset];
      }
    )";
  }

  if (info_.colors.size() == 2) {
    wgsl += R"(
      fn lerp_color(current: f32) -> vec4<f32> {
        return mix(gradient_info.colors[0], gradient_info.colors[1], current);
      }
    )";
  } else {
    wgsl += R"(
      fn lerp_color(current: f32) -> vec4<f32> {
        var t : f32 = current;
        if t > 1.0 {
            t = 1.0;
        }

        var colorCount  : i32 = gradient_info.infos.x;
        var stopCount   : i32 = gradient_info.infos.y;

        var startIndex  : i32 = 0;
        var endIndex    : i32 = 1;

        if stopCount > 0 && t <= get_stop(0) {
            return gradient_info.colors[0];
        }

        var step    : f32 = 1.0 / f32(colorCount - 1);
        var i       : i32 = 0;
        var start   : f32 = 0.0;
        var end     : f32 = 1.0;

        for (; i < colorCount - 1; i += 1) {
            if stopCount > 0 {
                start = get_stop(i);
                end = get_stop(i + 1);
            } else {
                start = step * f32(i);
                end = step * f32(i + 1);
            }

            if t >= start && t < end {
                startIndex = i;
                endIndex = i + 1;
                break;
            }
        }

        if i == colorCount - 1 && colorCount > 0 {
            return gradient_info.colors[colorCount - 1];
        }

        var total : f32 = end - start;
        var value : f32 = t - start;

        var mixValue: f32 = 0.5;
        if total > 0.0 {
            mixValue = value / total;
        }

        return mix(gradient_info.colors[startIndex], gradient_info.colors[endIndex], mixValue);
      }
    )";
  }

  wgsl += R"(
    fn calculate_gradient_color(t: f32) -> vec4<f32> {
        if gradient_info.infos.z == 3 && (t < 0.0 || t >= 1.0) {
            return vec4<f32>(0.0, 0.0, 0.0, 0.0);
        }

        var t1: f32 = remap_float_tile(t, gradient_info.infos.z);

        return lerp_color(t1);
    }
  )";

  return wgsl;
}

bool WGXGradientFragment::SetupLinearInfo(
    const wgx::BindGroupEntry* info_entry) const {
  if (info_entry->type_definition->name != "vec4<f32>") {
    return false;
  }

  std::array<float, 4> linear_pts{
      info_.point[0].x,
      info_.point[0].y,
      info_.point[1].x,
      info_.point[1].y,
  };

  info_entry->type_definition->SetData(linear_pts.data(),
                                       linear_pts.size() * sizeof(float));

  return true;
}

bool WGXGradientFragment::SetupRadialInfo(
    const wgx::BindGroupEntry* info_entry) const {
  if (info_entry->type_definition->name != "vec3<f32>") {
    return false;
  }

  std::array<float, 3> radial_pts{
      info_.point[0].x,
      info_.point[0].y,
      info_.radius[0],
  };

  info_entry->type_definition->SetData(radial_pts.data(),
                                       radial_pts.size() * sizeof(float));

  return true;
}

bool WGXGradientFragment::SetupConicalInfo(
    const wgx::BindGroupEntry* info_entry) const {
  if (info_entry->type_definition->name != "ConicalInfo") {
    return false;
  }

  auto conic_info_struct =
      static_cast<wgx::StructDefinition*>(info_entry->type_definition.get());

  conic_info_struct->GetMember("center1")->type->SetData(info_.point[0]);
  conic_info_struct->GetMember("center2")->type->SetData(info_.point[1]);
  conic_info_struct->GetMember("radius1")->type->SetData(info_.radius[0]);
  conic_info_struct->GetMember("radius2")->type->SetData(info_.radius[1]);

  return true;
}

bool WGXGradientFragment::SetupSweepInfo(
    const wgx::BindGroupEntry* info_entry) const {
  if (info_entry->type_definition->name != "vec4<f32>") {
    return false;
  }

  std::array<float, 4> sweep_pts{
      info_.point[0].x,
      info_.point[0].y,
      info_.radius[0],
      info_.radius[1],
  };

  info_entry->type_definition->SetData(sweep_pts.data(),
                                       sweep_pts.size() * sizeof(float));

  return true;
}

HWWGSLFragment* GenShadingFragment(HWDrawContext* context, const Paint& paint,
                                   bool is_stroke) {
  auto arena_allocator = context->arena_allocator;
  if (paint.GetShader()) {
    auto type = paint.GetShader()->AsGradient(nullptr);

    if (type == Shader::GradientType::kNone) {
      // handle image rendering in the future
      auto pixmap_shader =
          std::static_pointer_cast<PixmapShader>(paint.GetShader());

      const std::shared_ptr<Image>& image = *(pixmap_shader->AsImage());

      std::shared_ptr<GPUTexture> texture;
      if (image->GetTexture()) {
        const auto& texture_image = *(image->GetTexture());
        texture = texture_image->GetGPUTexture();
      } else if (image->GetPixmap()) {
        const auto& pixmap_image = *(image->GetPixmap());
        auto texture_handler =
            context->gpuContext->GetTextureManager()->FindOrCreateTexture(
                Texture::FormatFromColorType(pixmap_image->GetColorType()),
                pixmap_image->Width(), pixmap_image->Height(),
                pixmap_image->GetAlphaType(), pixmap_image);
        texture_handler->UploadImage(pixmap_image);
        texture = texture_handler->GetGPUTexture();
      } else {
        auto texture_handler = image->GetTextureByContext(context->gpuContext);

        if (texture_handler) {
          texture = texture_handler->GetGPUTexture();
        }
      }

      if (texture != nullptr) {
        GPUSamplerDescriptor descriptor;
        descriptor.mag_filter =
            ToGPUFilterMode(pixmap_shader->GetSamplingOptions()->filter);
        descriptor.min_filter =
            ToGPUFilterMode(pixmap_shader->GetSamplingOptions()->filter);
        descriptor.mipmap_filter =
            ToGPUMipmapMode(pixmap_shader->GetSamplingOptions()->mipmap);
        auto sampler =
            context->gpuContext->GetGPUDevice()->CreateSampler(descriptor);

        Matrix inv_local_matrix{};
        pixmap_shader->GetLocalMatrix().Invert(&inv_local_matrix);

        return arena_allocator->Make<WGSLTextureFragment>(
            pixmap_shader, texture, sampler, paint.GetAlphaF(),
            inv_local_matrix, static_cast<float>(image->Width()),
            static_cast<float>(image->Height()));

      } else {
        return arena_allocator->Make<WGSLSolidColor>(Colors::kRed);
      }

    } else {
      Shader::GradientInfo info{};

      paint.GetShader()->AsGradient(&info);

      return arena_allocator->Make<WGSLGradientFragment>(
          info, type, paint.GetAlphaF(), paint.GetShader()->GetLocalMatrix());
    }

  } else {
    if (is_stroke) {
      return arena_allocator->Make<WGSLSolidColor>(paint.GetStrokeColor());
    } else {
      return arena_allocator->Make<WGSLSolidColor>(paint.GetFillColor());
    }
  }
}

}  // namespace skity
