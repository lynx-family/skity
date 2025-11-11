// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/hw_wgsl_shader_writer.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <skity/skity.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "src/effect/pixmap_shader.hpp"
#include "src/render/hw/draw/fragment/wgsl_gradient_fragment.hpp"
#include "src/render/hw/draw/fragment/wgsl_solid_color.hpp"
#include "src/render/hw/draw/fragment/wgsl_texture_fragment.hpp"
#include "src/render/hw/draw/geometry/wgsl_path_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_tess_path_fill_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_tess_path_stroke_geometry.hpp"

namespace {

std::string GetPathGeometryVS() {
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

@group(0) @binding(0) var<uniform> common_slot  : CommonSlot;

struct VSInput {
  @location(0)  a_pos: vec2<f32>,
};

struct VSOutput {
  @builtin(position) pos: vec4<f32>,
};

@vertex
fn vs_main(input: VSInput) -> VSOutput {
  var output: VSOutput;
  var local_pos: vec2<f32>;

  local_pos = input.a_pos;
  output.pos = get_vertex_position(input.a_pos, common_slot);

  return output;
};
)";
}

std::string GetPathGeometryGradientVS() {
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
  
@group(0) @binding(0) var<uniform> common_slot  : CommonSlot;
@group(0) @binding(1) var<uniform> inv_matrix   : mat4x4<f32>;
    
struct VSInput {
  @location(0)  a_pos: vec2<f32>,
};

struct VSOutput {
  @builtin(position) pos: vec4<f32>,
  @location(0) f_param_pos: vec2<f32>,
};

@vertex
fn vs_main(input: VSInput) -> VSOutput {
  var output: VSOutput;
  var local_pos: vec2<f32>;

  local_pos = input.a_pos;
  output.pos = get_vertex_position(input.a_pos, common_slot);
  output.f_param_pos = (inv_matrix * vec4<f32>(local_pos.xy, 0.0, 1.0)).xy;
  return output;
};
)";
}

std::string GetPathAAGeometryGradientVS() {
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
  
@group(0) @binding(0) var<uniform> common_slot  : CommonSlot;
@group(0) @binding(1) var<uniform> inv_matrix   : mat4x4<f32>;
    
struct VSInput {
  @location(0)  a_pos: vec2<f32>,
  @location(1)  a_pos_aa: f32,
};

struct VSOutput {
  @builtin(position) pos: vec4<f32>,
  @location(0) f_param_pos: vec2<f32>,
  @location(1) v_pos_aa: f32,
};

@vertex
fn vs_main(input: VSInput) -> VSOutput {
  var output: VSOutput;
  var local_pos: vec2<f32>;

  local_pos = input.a_pos;
  output.pos = get_vertex_position(input.a_pos, common_slot);
  output.v_pos_aa = input.a_pos_aa;
  output.f_param_pos = (inv_matrix * vec4<f32>(local_pos.xy, 0.0, 1.0)).xy;
  return output;
};
)";
}

std::string GetPathAAGeometryVS() {
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

@group(0) @binding(0) var<uniform> common_slot  : CommonSlot;

struct VSInput {
  @location(0)  a_pos: vec2<f32>,
  @location(1)  a_pos_aa: f32,
};

struct VSOutput {
  @builtin(position) pos: vec4<f32>,
  @location(0) v_pos_aa: f32,
};

@vertex
fn vs_main(input: VSInput) -> VSOutput {
  var output: VSOutput;
  var local_pos: vec2<f32>;

  local_pos = input.a_pos;
  output.pos = get_vertex_position(input.a_pos, common_slot);
  output.v_pos_aa = input.a_pos_aa;

  return output;
};
)";
}

std::string GetPathTextureVS() {
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

struct ImageBoundsInfo {
  bounds      : vec2<f32>,
  inv_matrix  : mat4x4<f32>,
};

@group(0) @binding(0) var<uniform> common_slot  : CommonSlot;
@group(0) @binding(1) var<uniform> image_bounds : ImageBoundsInfo;

struct VSInput {
  @location(0)  a_pos: vec2<f32>,
};

struct VSOutput {
  @builtin(position) pos: vec4<f32>,
  @location(0) f_frag_coord: vec2<f32>,
};

@vertex
fn vs_main(input: VSInput) -> VSOutput {
  var output: VSOutput;
  var local_pos: vec2<f32>;

  local_pos = input.a_pos;
  output.pos = get_vertex_position(input.a_pos, common_slot);

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
  return output;
};

)";
}

std::string GetPathAATextureVS() {
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

struct ImageBoundsInfo {
  bounds      : vec2<f32>,
  inv_matrix  : mat4x4<f32>,
};
@group(0) @binding(0) var<uniform> common_slot  : CommonSlot;
@group(0) @binding(1) var<uniform> image_bounds : ImageBoundsInfo;

struct VSInput {
  @location(0)  a_pos: vec2<f32>,
  @location(1)  a_pos_aa: f32,
};

struct VSOutput {
  @builtin(position) pos: vec4<f32>,
  @location(0) f_frag_coord: vec2<f32>,
  @location(1) v_pos_aa: f32,
};

@vertex
fn vs_main(input: VSInput) -> VSOutput {
  var output: VSOutput;
  var local_pos: vec2<f32>;

  local_pos = input.a_pos;
  output.pos = get_vertex_position(input.a_pos, common_slot);
  output.v_pos_aa = input.a_pos_aa;
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
  return output;
};

)";
}

std::string GetTessPathFillVS() {
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
  
@group(0) @binding(0) var<uniform> common_slot: CommonSlot;

struct VSInput {
  @location(0) index: f32,
  @location(1) p0p1: vec4<f32>,
  @location(2) p2p3: vec4<f32>,
  @location(3) fan_center: vec2<f32>,
  @location(4) index_offset: f32,
  @location(5) num_segments: f32,
};

struct VSOutput {
  @builtin(position) pos: vec4<f32>,
};

@vertex
fn vs_main(input: VSInput) -> VSOutput {
  var output: VSOutput;
  var local_pos: vec2<f32>;

  var pos: vec2<f32>;
  var global_index: f32 = input.index + input.index_offset;
  if input.index < 0.0 || global_index > input.num_segments {
    pos = input.fan_center;
  } else {
    var t: f32 = global_index / input.num_segments;
    var p0: vec2<f32> = input.p0p1.xy;
    var p1: vec2<f32> = input.p0p1.zw;
    var p2: vec2<f32> = input.p2p3.xy;
    var p3: vec2<f32> = input.p2p3.zw;

    var p01: vec2<f32> = mix(p0, p1, t);
    var p12: vec2<f32> = mix(p1, p2, t);
    var p23: vec2<f32> = mix(p2, p3, t);

    var p012: vec2<f32> = mix(p01, p12, t);
    var p123: vec2<f32> = mix(p12, p23, t);
    pos = mix(p012, p123, t);
  }

  local_pos = pos;
  output.pos = get_vertex_position(pos.xy, common_slot);
  return output;
};
)";
}

std::string GetTessPathStrokeVS() {
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

fn cubic_bezier_tangent(p0: vec2<f32>, p1: vec2<f32>, p2: vec2<f32>, p3: vec2<f32>, t: f32) -> vec2<f32> {
  var u: f32 = 1.0 - t;
  var tangent: vec2<f32> = 3.0 * u * u * (p1 - p0) +
                           6.0 * u * t * (p2 - p1) +
                           3.0 * t * t * (p3 - p2);
  return tangent;
}

fn get_join_pos(index: i32, j0: vec2<f32>, j1: vec2<f32>, j2: vec2<f32>, j3: vec2<f32>) -> vec2<f32> {
  var points: array<vec2<f32>, 4> = array<vec2<f32>, 4>(j0, j1, j2, j3);
  let idx: u32 = u32(-index - 1);
  return points[idx];
}

@group(0) @binding(0) var<uniform> common_slot: CommonSlot;

struct VSInput {
  @location(0) index: f32,
  @location(1) offset : f32,
  @location(2) p0p1 : vec4<f32>,
  @location(3) p2p3 : vec4<f32>,
  @location(4) j0j1 : vec4<f32>,
  @location(5) j2j3 : vec4<f32>,
  @location(6) pack : vec4<f32>,
};

struct VSOutput {
  @builtin(position) pos: vec4<f32>,
};

@vertex
fn vs_main(input: VSInput) -> VSOutput {
  var output: VSOutput;
  var local_pos: vec2<f32>;

  var pos: vec2<f32>;
  var p0: vec2<f32> = input.p0p1.xy;
  var p1: vec2<f32> = input.p0p1.zw;
  var p2: vec2<f32> = input.p2p3.xy;
  var p3: vec2<f32> = input.p2p3.zw;
  var j0: vec2<f32> = input.j0j1.xy;
  var j1: vec2<f32> = input.j0j1.zw;
  var j2: vec2<f32> = input.j2j3.xy;
  var j3: vec2<f32> = input.j2j3.zw;

  var index_offset: f32 = input.pack.x;
  var num_segments: f32 = input.pack.y;
  var stroke_radius: f32 = input.pack.z;
  var is_circle: f32 = input.pack.w;

  var global_index: f32 = input.index + index_offset;
  if is_circle == 1.0 {
    var angle: f32 = input.offset * global_index / num_segments * 3.1415926;
    var dir: vec2<f32> = vec2<f32>(cos(angle), sin(angle));
    pos = p0 + dir * stroke_radius;
  } else if input.index < 0.0 {
    pos = get_join_pos(i32(input.index), j0, j1, j2, j3);
  } else if global_index > num_segments {
    pos = p3;
  } else {
    var t: f32 = global_index / num_segments;
    var p01: vec2<f32> = mix(p0, p1, t);
    var p12: vec2<f32> = mix(p1, p2, t);
    var p23: vec2<f32> = mix(p2, p3, t);

    var p012: vec2<f32> = mix(p01, p12, t);
    var p123: vec2<f32> = mix(p12, p23, t);
    pos = mix(p012, p123, t);

    var tangent: vec2<f32> = normalize(cubic_bezier_tangent(p0, p1, p2, p3, t));
    var norm: vec2<f32> = vec2<f32>(tangent.y, -tangent.x);
    pos = pos + norm.xy * stroke_radius * input.offset;
  }

  local_pos = pos;
  output.pos = get_vertex_position(pos.xy, common_slot);
  return output;
};
)";
}

std::string GetSolidColorFS() {
  return R"(
@group(1) @binding(0) var<uniform> uColor: vec4<f32>;

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  var color : vec4<f32>;
  color = vec4<f32>(uColor.rgb * uColor.a, uColor.a);
  return color;
}
)";
}

std::string GetSolidColorAAFS() {
  return R"(
@group(1) @binding(0) var<uniform> uColor: vec4<f32>;

struct FSInput {
  @location(0) v_pos_aa: f32,
};

@fragment
fn fs_main(input: FSInput) -> @location(0) vec4<f32> {
  var color : vec4<f32>;
  color = vec4<f32>(uColor.rgb * uColor.a, uColor.a);
  var mask_alpha: f32 = 1.0;
  mask_alpha = input.v_pos_aa;
  color = color * mask_alpha;
  return color;
}
)";
}

std::string GetSolidColorAAWithCFFS() {
  return R"(
struct MatrixFilterInfo
{
  matrix_add : vec4<f32>,
  matrix_mul : mat4x4<f32>,
};

@group(1) @binding(1) var<uniform> uMatrixFilterInfo : MatrixFilterInfo;
fn filter_color(input_color: vec4<f32>) -> vec4<f32>{
  if input_color.a > 0.0 {
    input_color.rgb /= input_color.a;
  }

  var color: vec4<f32> = uMatrixFilterInfo.matrix_mul * input_color + uMatrixFilterInfo.matrix_add;
  return vec4<f32>(color.rgb * color.a, color.a);
}

@group(1) @binding(0) var<uniform> uColor: vec4<f32>;

struct FSInput {
  @location(0) v_pos_aa: f32,
};

@fragment
fn fs_main(input: FSInput) -> @location(0) vec4<f32> {
  var color : vec4<f32>;
  color = vec4<f32>(uColor.rgb * uColor.a, uColor.a);
  color = filter_color(color);
  var mask_alpha: f32 = 1.0;
  mask_alpha = input.v_pos_aa;
  color = color * mask_alpha;
  return color;
}
)";
}

std::string GetLinearGradientFS() {
  return R"(
struct GradientInfo {
  infos : vec4<i32>,
  colors: array<vec4<f32>, 2>,
  global_alpha: f32,
};

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

@group(1) @binding(0) var<uniform> gradient_info    : GradientInfo;

fn get_stop(index: i32) -> f32 {
  var colorCount: i32 = gradient_info.infos.x;
  var step: f32 = 1.0 / f32(colorCount - 1);
  return step * f32(index);
}

fn lerp_color(current: f32) -> vec4<f32> {
  return mix(gradient_info.colors[0], gradient_info.colors[1], current);
}

fn calculate_gradient_color(t: f32) -> vec4<f32> {
  if gradient_info.infos.z == 3 && (t < 0.0 || t >= 1.0) {
    return vec4<f32>(0.0, 0.0, 0.0, 0.0);
  }
  var t1: f32 = remap_float_tile(t, gradient_info.infos.z);
  return lerp_color(t1);
}

fn generate_gradient_color(v_pos: vec2<f32>) -> vec4<f32> {
  var cs: vec2<f32> = v_pos - linear_pts.xy;
  var se: vec2<f32> = linear_pts.zw - linear_pts.xy;
  var t: f32 = dot(cs, se) / dot(se, se);
  var color: vec4<f32> = calculate_gradient_color(t);
  color.xyz *= color.w;
  return color * gradient_info.global_alpha;
}

@group(1) @binding(1) var<uniform> linear_pts       : vec4<f32>;

struct FSInput {
  @location(0) f_param_pos: vec2<f32>,
};

@fragment
fn fs_main(input: FSInput) -> @location(0) vec4<f32> {
  var color : vec4<f32>;
  color = generate_gradient_color(input.f_param_pos);
  return color;
}
)";
}

std::string GetLinearGradientAAFS() {
  return R"(
struct GradientInfo {
  infos : vec4<i32>,
  colors: array<vec4<f32>, 2>,
  global_alpha: f32,
};

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

@group(1) @binding(0) var<uniform> gradient_info    : GradientInfo;

fn get_stop(index: i32) -> f32 {
  var colorCount: i32 = gradient_info.infos.x;
  var step: f32 = 1.0 / f32(colorCount - 1);
  return step * f32(index);
}

fn lerp_color(current: f32) -> vec4<f32> {
  return mix(gradient_info.colors[0], gradient_info.colors[1], current);
}

fn calculate_gradient_color(t: f32) -> vec4<f32> {
  if gradient_info.infos.z == 3 && (t < 0.0 || t >= 1.0) {
    return vec4<f32>(0.0, 0.0, 0.0, 0.0);
  }
  var t1: f32 = remap_float_tile(t, gradient_info.infos.z);
  return lerp_color(t1);
}

fn generate_gradient_color(v_pos: vec2<f32>) -> vec4<f32> {
  var cs: vec2<f32> = v_pos - linear_pts.xy;
  var se: vec2<f32> = linear_pts.zw - linear_pts.xy;
  var t: f32 = dot(cs, se) / dot(se, se);
  var color: vec4<f32> = calculate_gradient_color(t);
  color.xyz *= color.w;
  return color * gradient_info.global_alpha;
}

@group(1) @binding(1) var<uniform> linear_pts       : vec4<f32>;

struct FSInput {
  @location(0) f_param_pos: vec2<f32>,
  @location(1) v_pos_aa: f32,
};

@fragment
fn fs_main(input: FSInput) -> @location(0) vec4<f32> {
  var color : vec4<f32>;
  color = generate_gradient_color(input.f_param_pos);
  var mask_alpha: f32 = 1.0;
  mask_alpha = input.v_pos_aa;
  color = color * mask_alpha;
  return color;
}
)";
}

std::string GetTextureFS() {
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

struct ImageColorInfo {
  infos           : vec3<i32>,
  global_alpha    : f32,
};

@group(1) @binding(0) var<uniform>  image_color_info    : ImageColorInfo;
@group(1) @binding(1) var           uSampler            : sampler;
@group(1) @binding(2) var           uTexture            : texture_2d<f32>;

struct FSInput {
  @location(0) f_frag_coord: vec2<f32>,
};

@fragment
fn fs_main(input: FSInput) -> @location(0) vec4<f32> {
  var color : vec4<f32>;
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

  return color;
}
)";
}

std::string GetTextureAAFS() {
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

struct ImageColorInfo {
  infos           : vec3<i32>,
  global_alpha    : f32,
};

@group(1) @binding(0) var<uniform>  image_color_info    : ImageColorInfo;
@group(1) @binding(1) var           uSampler            : sampler;
@group(1) @binding(2) var           uTexture            : texture_2d<f32>;

struct FSInput {
  @location(0) f_frag_coord: vec2<f32>,
  @location(1) v_pos_aa: f32,
};

@fragment
fn fs_main(input: FSInput) -> @location(0) vec4<f32> {
  var color : vec4<f32>;
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

  var mask_alpha: f32 = 1.0;
  mask_alpha = input.v_pos_aa;
  color = color * mask_alpha;
  return color;
}
)";
}

skity::Path MakePath() {
  skity::Path path;
  path.MoveTo(100, 100);
  path.LineTo(200, 200);
  path.LineTo(100, 200);
  path.Close();
  return path;
}

static std::string ltrim(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  return (start == std::string::npos) ? "" : s.substr(start);
}

static std::vector<std::string> NormalizeLines(const std::string &input) {
  std::vector<std::string> lines;
  std::istringstream iss(input);
  std::string line;

  while (std::getline(iss, line)) {
    line = ltrim(line);
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  return lines;
}

bool CompareShader(const std::string &expected, const std::string &actual) {
  auto expLines = NormalizeLines(expected);
  auto actLines = NormalizeLines(actual);

  size_t maxLines = std::max(expLines.size(), actLines.size());
  for (size_t i = 0; i < maxLines; ++i) {
    std::string e = (i < expLines.size()) ? expLines[i] : "<no line>";
    std::string a = (i < actLines.size()) ? actLines[i] : "<no line>";

    if (e != a) {
      std::cout << "❌ Difference found at line " << (i + 1) << ":\n";
      std::cout << "Expected: \"" << e << "\"\n";
      std::cout << "Actual:   \"" << a << "\"\n";
      return false;
    }
  }
  return true;
}

}  // namespace

TEST(ShaderWriter, PathWithSolidColor) {
  auto path = MakePath();
  skity::Paint paint;
  paint.SetColor(0xff00ff00);
  skity::Color4f color = paint.GetColor4f();
  skity::WGSLPathGeometry geometry{path, paint, false};
  skity::WGSLSolidColor fragment{color};
  skity::HWWGSLShaderWriter shader_writer{&geometry, &fragment};
  std::string vs = shader_writer.GenVSSourceWGSL();
  std::string fs = shader_writer.GenFSSourceWGSL();
  ASSERT_TRUE(geometry.IsSnippet());
  ASSERT_TRUE(fragment.IsSnippet());
  ASSERT_FALSE(geometry.AffectsFragment());
  ASSERT_FALSE(fragment.AffectsVertex());
  ASSERT_EQ(shader_writer.GetVSShaderName(), "VS_Path");
  ASSERT_EQ(shader_writer.GetFSShaderName(), "FS_SolidColor");
  ASSERT_TRUE(CompareShader(vs, GetPathGeometryVS()));
  ASSERT_TRUE(CompareShader(fs, GetSolidColorFS()));
}

TEST(ShaderWriter, PathAAWithSolidColor) {
  auto path = MakePath();
  skity::Paint paint;
  paint.SetColor(0xff00ff00);
  skity::Color4f color = paint.GetColor4f();
  skity::WGSLPathAAGeometry geometry{path, paint};
  skity::WGSLSolidColor fragment{color};
  skity::HWWGSLShaderWriter shader_writer{&geometry, &fragment};
  std::string vs = shader_writer.GenVSSourceWGSL();
  std::string fs = shader_writer.GenFSSourceWGSL();
  ASSERT_TRUE(geometry.IsSnippet());
  ASSERT_TRUE(fragment.IsSnippet());
  ASSERT_TRUE(geometry.AffectsFragment());
  ASSERT_FALSE(fragment.AffectsVertex());
  ASSERT_EQ(shader_writer.GetVSShaderName(), "VS_PathAA");
  ASSERT_EQ(shader_writer.GetFSShaderName(), "FS_SolidColor_AA");
  ASSERT_TRUE(CompareShader(vs, GetPathAAGeometryVS()));
  ASSERT_TRUE(CompareShader(fs, GetSolidColorAAFS()));
}

TEST(ShaderWriter, TessPathFillWithSolidColor) {
  auto path = MakePath();
  skity::Paint paint;
  paint.SetColor(0xff00ff00);
  skity::Color4f color = paint.GetColor4f();
  skity::WGSLTessPathFillGeometry geometry{path, paint};
  skity::WGSLSolidColor fragment{color};
  skity::HWWGSLShaderWriter shader_writer{&geometry, &fragment};
  std::string vs = shader_writer.GenVSSourceWGSL();
  std::string fs = shader_writer.GenFSSourceWGSL();
  ASSERT_TRUE(geometry.IsSnippet());
  ASSERT_TRUE(fragment.IsSnippet());
  ASSERT_FALSE(geometry.AffectsFragment());
  ASSERT_FALSE(fragment.AffectsVertex());
  ASSERT_EQ(shader_writer.GetVSShaderName(), "VS_TessPathFill");
  ASSERT_EQ(shader_writer.GetFSShaderName(), "FS_SolidColor");
  ASSERT_TRUE(CompareShader(vs, GetTessPathFillVS()));
  ASSERT_TRUE(CompareShader(fs, GetSolidColorFS()));
}

TEST(ShaderWriter, TessPathStrokeWithSolidColor) {
  auto path = MakePath();
  skity::Paint paint;
  paint.SetColor(0xff00ff00);
  skity::Color4f color = paint.GetColor4f();
  skity::WGSLTessPathStrokeGeometry geometry{path, paint};
  skity::WGSLSolidColor fragment{color};
  skity::HWWGSLShaderWriter shader_writer{&geometry, &fragment};
  std::string vs = shader_writer.GenVSSourceWGSL();
  std::string fs = shader_writer.GenFSSourceWGSL();
  ASSERT_TRUE(geometry.IsSnippet());
  ASSERT_TRUE(fragment.IsSnippet());
  ASSERT_FALSE(geometry.AffectsFragment());
  ASSERT_FALSE(fragment.AffectsVertex());
  ASSERT_EQ(shader_writer.GetVSShaderName(), "VS_TessPathStroke");
  ASSERT_EQ(shader_writer.GetFSShaderName(), "FS_SolidColor");
  ASSERT_TRUE(CompareShader(vs, GetTessPathStrokeVS()));
  ASSERT_TRUE(CompareShader(fs, GetSolidColorFS()));
}

TEST(ShaderWriter, PathWithLinearGradient) {
  auto path = MakePath();
  skity::Paint paint;
  skity::Vec4 colors[] = {
      skity::Vec4{0.f, 0.f, 1.f, 1.f},
      skity::Vec4{1.f, 0.f, 0.f, 1.f},
  };
  std::vector<skity::Point> pts = {
      skity::Point{0.f, 0.f, 0.f, 1.f},
      skity::Point{256.f, 0.f, 0.f, 1.f},
  };
  paint.SetShader(skity::Shader::MakeLinear(pts.data(), colors, nullptr, 2));

  skity::WGSLPathGeometry geometry{path, paint, false};
  skity::Shader::GradientInfo gradient_info;
  auto gradient_type = paint.GetShader()->AsGradient(&gradient_info);
  skity::WGSLGradientFragment fragment{gradient_info, gradient_type,
                                       paint.GetAlphaF(), skity::Matrix{}};
  skity::HWWGSLShaderWriter shader_writer{&geometry, &fragment};
  std::string vs = shader_writer.GenVSSourceWGSL();
  std::string fs = shader_writer.GenFSSourceWGSL();
  ASSERT_TRUE(geometry.IsSnippet());
  ASSERT_TRUE(fragment.IsSnippet());
  ASSERT_FALSE(geometry.AffectsFragment());
  ASSERT_TRUE(fragment.AffectsVertex());
  ASSERT_EQ(shader_writer.GetVSShaderName(), "VS_Path_Gradient");
  ASSERT_EQ(shader_writer.GetFSShaderName(),
            "FS_GradientLinear2OffsetFastColorFast");
  ASSERT_TRUE(CompareShader(vs, GetPathGeometryGradientVS()));
  ASSERT_TRUE(CompareShader(fs, GetLinearGradientFS()));
}

TEST(ShaderWriter, PathAAWithLinearGradient) {
  auto path = MakePath();
  skity::Paint paint;
  skity::Vec4 colors[] = {
      skity::Vec4{0.f, 0.f, 1.f, 1.f},
      skity::Vec4{1.f, 0.f, 0.f, 1.f},
  };
  std::vector<skity::Point> pts = {
      skity::Point{0.f, 0.f, 0.f, 1.f},
      skity::Point{256.f, 0.f, 0.f, 1.f},
  };
  paint.SetShader(skity::Shader::MakeLinear(pts.data(), colors, nullptr, 2));
  skity::WGSLPathAAGeometry geometry{path, paint};
  skity::Shader::GradientInfo gradient_info;
  auto gradient_type = paint.GetShader()->AsGradient(&gradient_info);
  skity::WGSLGradientFragment fragment{gradient_info, gradient_type,
                                       paint.GetAlphaF(), skity::Matrix{}};

  skity::HWWGSLShaderWriter shader_writer{&geometry, &fragment};
  std::string vs = shader_writer.GenVSSourceWGSL();
  std::string fs = shader_writer.GenFSSourceWGSL();
  ASSERT_TRUE(geometry.IsSnippet());
  ASSERT_TRUE(fragment.IsSnippet());
  ASSERT_TRUE(geometry.AffectsFragment());
  ASSERT_TRUE(fragment.AffectsVertex());
  ASSERT_EQ(shader_writer.GetVSShaderName(), "VS_PathAA_Gradient");
  ASSERT_EQ(shader_writer.GetFSShaderName(),
            "FS_GradientLinear2OffsetFastColorFast_AA");
  ASSERT_TRUE(CompareShader(vs, GetPathAAGeometryGradientVS()));
  ASSERT_TRUE(CompareShader(fs, GetLinearGradientAAFS()));
}

TEST(ShaderWriter, PathWithTexture) {
  auto path = MakePath();
  skity::Paint paint;
  skity::WGSLPathGeometry geometry{path, paint, false};
  std::shared_ptr<skity::Pixmap> pixmap = std::make_shared<skity::Pixmap>(
      500, 500, skity::AlphaType::kUnpremul_AlphaType, skity::ColorType::kRGBA);
  auto image = skity::Image::MakeImage(pixmap);
  auto shader = std::make_shared<skity::PixmapShader>(
      image, skity::SamplingOptions{}, skity::TileMode::kClamp,
      skity::TileMode::kClamp, skity::Matrix{});
  skity::WGSLTextureFragment fragment{shader,          nullptr, nullptr, 1.0f,
                                      skity::Matrix{}, 500,     500};
  skity::HWWGSLShaderWriter shader_writer{&geometry, &fragment};

  std::string vs = shader_writer.GenVSSourceWGSL();
  std::string fs = shader_writer.GenFSSourceWGSL();
  ASSERT_TRUE(geometry.IsSnippet());
  ASSERT_TRUE(fragment.IsSnippet());
  ASSERT_FALSE(geometry.AffectsFragment());
  ASSERT_TRUE(fragment.AffectsVertex());
  ASSERT_EQ(shader_writer.GetVSShaderName(), "VS_Path_Texture");
  ASSERT_EQ(shader_writer.GetFSShaderName(), "FS_Texture");
  ASSERT_TRUE(CompareShader(vs, GetPathTextureVS()));
  ASSERT_TRUE(CompareShader(fs, GetTextureFS()));
}

TEST(ShaderWriter, PathAAWithTexture) {
  auto path = MakePath();
  skity::Paint paint;
  skity::WGSLPathAAGeometry geometry{path, paint};
  std::shared_ptr<skity::Pixmap> pixmap = std::make_shared<skity::Pixmap>(
      500, 500, skity::AlphaType::kUnpremul_AlphaType, skity::ColorType::kRGBA);
  auto image = skity::Image::MakeImage(pixmap);
  auto shader = std::make_shared<skity::PixmapShader>(
      image, skity::SamplingOptions{}, skity::TileMode::kClamp,
      skity::TileMode::kClamp, skity::Matrix{});
  skity::WGSLTextureFragment fragment{shader,          nullptr, nullptr, 1.0f,
                                      skity::Matrix{}, 500,     500};
  skity::HWWGSLShaderWriter shader_writer{&geometry, &fragment};
  std::string vs = shader_writer.GenVSSourceWGSL();
  std::string fs = shader_writer.GenFSSourceWGSL();
  ASSERT_TRUE(geometry.IsSnippet());
  ASSERT_TRUE(fragment.IsSnippet());
  ASSERT_TRUE(geometry.AffectsFragment());
  ASSERT_TRUE(fragment.AffectsVertex());
  ASSERT_EQ(shader_writer.GetVSShaderName(), "VS_PathAA_Texture");
  ASSERT_EQ(shader_writer.GetFSShaderName(), "FS_Texture_AA");
  ASSERT_TRUE(CompareShader(vs, GetPathAATextureVS()));
  ASSERT_TRUE(CompareShader(fs, GetTextureAAFS()));
}

TEST(ShaderWriter, PathAAWithSolidColorAndColorFilter) {
  auto path = MakePath();
  skity::Paint paint;
  paint.SetColor(0xff00ff00);
  skity::Color4f color = paint.GetColor4f();
  skity::WGSLPathAAGeometry geometry{path, paint};
  skity::WGSLSolidColor fragment{color};
  float color_matrix[20] = {0, 1, 0, 0, 0,  //
                            0, 0, 1, 0, 0,  //
                            1, 0, 0, 0, 0,  //
                            0, 0, 0, 1, 0};
  auto filter = skity::ColorFilters::Matrix(color_matrix);
  fragment.SetFilter(skity::WGXFilterFragment::Make(filter.get()));
  skity::HWWGSLShaderWriter shader_writer{&geometry, &fragment};
  std::string vs = shader_writer.GenVSSourceWGSL();
  std::string fs = shader_writer.GenFSSourceWGSL();
  ASSERT_TRUE(geometry.IsSnippet());
  ASSERT_TRUE(fragment.IsSnippet());
  ASSERT_TRUE(geometry.AffectsFragment());
  ASSERT_FALSE(fragment.AffectsVertex());
  ASSERT_EQ(shader_writer.GetVSShaderName(), "VS_PathAA");
  ASSERT_EQ(shader_writer.GetFSShaderName(), "FS_SolidColor_AA_MatrixFilter");
  ASSERT_TRUE(CompareShader(vs, GetPathAAGeometryVS()));
  ASSERT_TRUE(CompareShader(fs, GetSolidColorAAWithCFFS()));
}
