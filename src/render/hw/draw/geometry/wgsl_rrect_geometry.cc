// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/geometry/wgsl_rrect_geometry.hpp"

#include "src/render/hw/draw/hw_wgsl_geometry.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/tracing.hpp"

namespace skity {

namespace {

constexpr static float kCornerTopLeft = 0.f;
constexpr static float kCornerTopRight = 1.f;
constexpr static float kCornerBottomRight = 2.f;
constexpr static float kCornerBottomLeft = 3.f;

constexpr static float kRegionCorner = -1.f;
constexpr static float kRegionEdge = 0.f;
constexpr static float kRegionCenter = 1.f;

constexpr static float kOutside = 1.f;
constexpr static float kInside = 0.f;

std::vector<GPUVertexBufferLayout> InitVertexBufferLayout() {
  std::vector<GPUVertexBufferLayout> layout = {
      // vertex buffer
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
      // instance buffer
      GPUVertexBufferLayout{
          22 * sizeof(float),
          GPUVertexStepMode::kInstance,
          {
              GPUVertexAttribute{
                  GPUVertexFormat::kFloat32x4,
                  0,
                  1,
              },
              GPUVertexAttribute{
                  GPUVertexFormat::kFloat32x2,
                  4 * sizeof(float),
                  2,
              },
              GPUVertexAttribute{
                  GPUVertexFormat::kFloat32x2,
                  6 * sizeof(float),
                  3,
              },
              GPUVertexAttribute{
                  GPUVertexFormat::kFloat32x4,
                  8 * sizeof(float),
                  4,
              },
              GPUVertexAttribute{
                  GPUVertexFormat::kFloat32x4,
                  12 * sizeof(float),
                  5,
              },
              GPUVertexAttribute{
                  GPUVertexFormat::kFloat32x2,
                  16 * sizeof(float),
                  6,
              },
              GPUVertexAttribute{
                  GPUVertexFormat::kFloat32x4,
                  18 * sizeof(float),
                  7,
              },
          },
      },
  };

  return layout;
}

struct Instance {
  Vec4 rect;
  Vec2 radii;
  Vec2 stroke;
  Vec4 m;
  Vec4 transform0;
  Vec2 transform1;
  Vec4 color;
};

static_assert(sizeof(Instance) == 88);

}  // namespace

WGSLRRectGeometry::WGSLRRectGeometry(
    const std::vector<BatchGroup<RRect>>& batch_group)
    : HWWGSLGeometry(Flags::kSnippet | Flags::kAffectsFragment),
      batch_group_(batch_group),
      layout_(InitVertexBufferLayout()) {}

const std::vector<GPUVertexBufferLayout>& WGSLRRectGeometry::GetBufferLayout()
    const {
  return layout_;
}

void WGSLRRectGeometry::WriteVSFunctionsAndStructs(
    std::stringstream& ss) const {
  ss << CommonVertexWGSL();
  ss << R"(
// 0 => (-1, -1)
// 1 => ( 1, -1)
// 2 => ( 1,  1)
// 3 => (-1,  1)
fn get_corner_sign(corner_idx: i32) -> vec2<f32> {
  let x: f32 = f32(((corner_idx + 1) & 2) - 1);
  let y: f32 = f32((corner_idx & 2) - 1);
  return vec2<f32>(x, y);
}

fn inverse_grid_length(g: vec2<f32>, j : mat2x2<f32>) -> f32 {
  var grid: vec2<f32> = j * g;
  return 1.0 / sqrt(dot(grid, grid));
}

fn in_corner_region(pos_to_corner: vec2<f32>, corner_sign: vec2<f32>) -> bool {
  let p_sign: vec2<f32> = sign(pos_to_corner);
  let s: vec2<f32> = p_sign * corner_sign;
  return s.x > 0.0 && s.y > 0.0;
}
)";
}

void WGSLRRectGeometry::WriteVSUniforms(std::stringstream& ss) const {
  ss << "@group(0) @binding(0) var<uniform> common_slot  : CommonSlot;\n";
}

void WGSLRRectGeometry::WriteVSInput(std::stringstream& ss) const {
  ss << R"(
struct VSInput {
  @location(0)  packed        :   vec4<f32>,
  @location(1)  rect          :   vec4<f32>,
  @location(2)  radii         :   vec2<f32>,
  @location(3)  stroke        :   vec2<f32>,
  @location(4)  j             :   vec4<f32>,
  @location(5)  transform0    :   vec4<f32>,
  @location(6)  transform1    :   vec2<f32>,
  @location(7)  color         :   vec4<f32>,
};
)";
}

void WGSLRRectGeometry::WriteVSMain(std::stringstream& ss) const {
  ss << R"(
  var offset: vec2<f32> = input.packed.xy;
  var corner_and_outside: i32 = i32(input.packed.z);
  var corner_idx: i32 = corner_and_outside & 0x3;
  var outside: f32 = f32(corner_and_outside >> 2);
  var region: f32 = input.packed.w;

  var corner_sign: vec2<f32> = get_corner_sign(corner_idx);
  var rect_x: vec4<f32> = input.rect.xzzx;
  var rect_y: vec4<f32> = input.rect.yyww;
  var center: vec2<f32> = (input.rect.xy + input.rect.zw) * 0.5;
  var harf_wh: vec2<f32> = center - input.rect.xy;
  var j: mat2x2<f32> = mat2x2<f32>(input.j.xy, input.j.zw);
  var aa: vec2<f32> = abs(j * vec2<f32>(1.0, 1.0));
  var stroke_vec: vec2<f32> = vec2<f32>(input.stroke.x);

  var r_outer: vec2<f32> = harf_wh + stroke_vec + aa;
  var r_inner: vec2<f32> = max(harf_wh - stroke_vec - aa, 0.0);
  var r_core: vec2<f32> = harf_wh - input.radii;

  var diff: vec2<f32> = input.radii - (stroke_vec + aa);


  let mask_outside: f32 = outside;
  let mask_diff: vec2<f32> = vec2<f32>(
    select(0.0, 1.0, diff.x < 0.0),
    select(0.0, 1.0, diff.y < 0.0)
  );

  var r1: vec2<f32> = mix(r_inner, r_outer, vec2<f32>(mask_outside));
  var r2: vec2<f32> = mix(r_core, r_inner, mask_diff);
  var r_final: vec2<f32> = mix(r2, r1, offset);

  var pos: vec2<f32> = center + corner_sign * r_final;

  let is_center: bool = input.stroke.x == 0.0 && region > 0.0;
  if (is_center) {
    pos = center;
  }

  var inv_grid: vec2<f32> = vec2<f32>(
    inverse_grid_length(vec2<f32>(1.0, 0.0), j),
    inverse_grid_length(vec2<f32>(0.0, 1.0), j)
  );

  local_pos = pos;
  var fs_packed : vec4<f32> = vec4<f32>(local_pos, f32(corner_idx), region);
  var transform: mat4x4<f32> = mat4x4<f32>(
    input.transform0.x, input.transform0.y, 0.0, 0.0,
    input.transform0.z, input.transform0.w, 0.0, 0.0, 
                   0.0,                0.0, 1.0, 0.0, 
    input.transform1.x, input.transform1.y, 0.0, 1.0
  );
  var common_slot_clone: CommonSlot = common_slot;
  common_slot_clone.userTransform = transform;
  output.pos = get_vertex_position(pos, common_slot_clone);
  output.v_fs_packed = fs_packed;
  output.v_rect = input.rect;
  output.v_radii = input.radii;
  output.v_stroke = input.stroke;
  output.v_j = vec4<f32>(j[0], j[1]);
  output.v_inv_grid = inv_grid;
 )";
}

std::optional<std::vector<std::string>> WGSLRRectGeometry::GetVarings() const {
  return std::vector<std::string>{"v_fs_packed: vec4<f32>",  //
                                  "v_rect: vec4<f32>",       //
                                  "v_radii: vec2<f32>",      //
                                  "v_stroke: vec2<f32>",     //
                                  "v_j: vec4<f32>",          //
                                  "v_inv_grid: vec2<f32>"};
}

void WGSLRRectGeometry::WriteFSFunctionsAndStructs(
    std::stringstream& ss) const {
  ss << R"(
fn inverse_grid_length(g: vec2<f32>, j : mat2x2<f32>) -> f32 {
  var grid: vec2<f32> = j * g;
  return 1.0 / sqrt(dot(grid, grid));
}
    
fn ellipse_sdf(p: vec2<f32>,ab: vec2<f32>, j:mat2x2<f32>) -> f32 {
  var inv_a2b2: vec2<f32> = 1.0 / (ab * ab);
  var x2y2: vec2<f32> = p * p;
  var k1: f32 = dot(x2y2, inv_a2b2) - 1.0;
  var k2: f32 = inverse_grid_length(2.0 * p * inv_a2b2, j);
  return k1 * k2;
}

fn linearstep(edge0: f32, edge1: f32, x: f32) -> f32 {
  return clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
}
 
// 0 => (-1, -1)
// 1 => ( 1, -1)
// 2 => ( 1,  1)
// 3 => (-1,  1)
fn get_corner_sign(corner_idx: i32) -> vec2<f32> {
  let x: f32 = f32(((corner_idx + 1) & 2) - 1);
  let y: f32 = f32((corner_idx & 2) - 1);
  return vec2<f32>(x, y);
}

fn in_corner_region(pos_to_corner: vec2<f32>, corner_sign: vec2<f32>) -> bool {
  let p_sign: vec2<f32> = sign(pos_to_corner);
  let s: vec2<f32> = p_sign * corner_sign;
  return s.x > 0.0 && s.y > 0.0;
}

fn calculate_mask_alpha(v_pos: vec2<f32>, corner_idx: i32, v_region: f32, v_rect: vec4<f32>, v_radii: vec2<f32>, v_stroke: vec2<f32>, v_j: vec4<f32>, v_inv_grid: vec2<f32>) -> f32 {
  if (v_region > 0.0 && v_stroke.x == 0.0) {
    return 1.0;
  } else {
    var alpha: f32 = 0.0;
    var d_inner: f32 = 1.0;
    let is_rect: bool = v_radii.x == 0.0 && v_radii.y == 0.0;
    var j : mat2x2<f32> = mat2x2<f32>(v_j.xy, v_j.zw);
    var edge_distances : vec4<f32> = v_rect - vec4<f32>(v_pos, v_pos);
    edge_distances.zw = -edge_distances.zw;
    var outer_distances: vec4<f32> = edge_distances - v_stroke.x;
    var max_outer_d2 : vec2<f32> = max(outer_distances.xy, outer_distances.zw);
    var need_handle_join : bool = v_stroke.x > 0.0 && is_rect &&
                                  max_outer_d2.x > -v_stroke.x &&
                                  max_outer_d2.y > -v_stroke.x;

    max_outer_d2 = max_outer_d2 * v_inv_grid;
    var d_outer: f32 = max(max_outer_d2.x, max_outer_d2.y);

    if v_stroke.x > 0.0 {
      var inner_distances: vec4<f32> = edge_distances + v_stroke.x;
      var max_inner_d2 : vec2<f32> = max(inner_distances.xy, inner_distances.zw);
      max_inner_d2 = max_inner_d2 * v_inv_grid;
      d_inner = max(max_inner_d2.x, max_inner_d2.y);
    }
      
    // corner_idx is valid only if v_reion < 0.0
    if (v_region < 0.0) {
      var core_rect: vec4<f32> = vec4<f32>(v_rect.xy + v_radii, v_rect.zw - v_radii);
      var core_rect_x: vec4<f32> = core_rect.xzzx;
      var core_rect_y: vec4<f32> = core_rect.yyww;
      var corner_origin: vec2<f32> = vec2<f32>(core_rect_x[corner_idx], core_rect_y[corner_idx]);
      var pos_to_corner: vec2<f32> = v_pos - corner_origin;
      var corner_sign: vec2<f32> = get_corner_sign(corner_idx);
      var in_corner: bool = in_corner_region(pos_to_corner, corner_sign);
      var may_has_round_corner: bool = in_corner && !is_rect;
      var needs_handle_inner_ellipse: bool = may_has_round_corner && v_stroke.x > 0.0;
      var needs_handle_outer_ellipse: bool = may_has_round_corner || (v_stroke.y == 1.0 && need_handle_join);
      let needs_handle_bevel: bool = v_stroke.y == 2.0 && need_handle_join;

      if needs_handle_bevel {
        var pos_to_corner_abs: vec2<f32> = abs(pos_to_corner);
        var d_outer_bevel: f32 = pos_to_corner_abs.x + pos_to_corner_abs.y - v_stroke.x;
        d_outer_bevel = d_outer_bevel * inverse_grid_length(corner_sign ,j);
        d_outer = max(d_outer,  d_outer_bevel);
      }
    

      if needs_handle_outer_ellipse {
        d_outer = max(ellipse_sdf(pos_to_corner, v_radii + vec2<f32>(v_stroke.x), j), d_outer);
      }
                           
      if needs_handle_inner_ellipse {
        d_inner = max(ellipse_sdf(pos_to_corner, v_radii - vec2<f32>(v_stroke.x), j), d_inner);
      }
    }
  
    alpha = linearstep(0.5, -0.5, max(d_outer, -d_inner));
    return alpha;
  }
}
)";
}

void WGSLRRectGeometry::WriteFSAlphaMask(std::stringstream& ss) const {
  ss << R"(
  mask_alpha = calculate_mask_alpha(input.v_fs_packed.xy, i32(input.v_fs_packed.z), input.v_fs_packed.w, input.v_rect, input.v_radii, input.v_stroke, input.v_j, input.v_inv_grid);
)";
}

std::string WGSLRRectGeometry::GetShaderName() const { return "RRect"; }

void WGSLRRectGeometry::PrepareCMD(Command* cmd, HWDrawContext* context,
                                   const Matrix& transform, float clip_depth,
                                   Command* stencil_cmd) {
  SKITY_TRACE_EVENT(WGSLOvalGeometry_PrepareCMD);

  if (cmd->pipeline == nullptr) {
    return;
  }

  cmd->vertex_buffer = context->static_buffer->GetRRectVertexBufferView();

  cmd->index_buffer = context->static_buffer->GetRRectIndexBufferView();

  cmd->index_count = cmd->index_buffer.range / sizeof(uint32_t);

  context->stageBuffer->BeginWritingInstance(
      batch_group_.size() * sizeof(Instance), alignof(Instance));
  for (auto& element : batch_group_) {
    const RRect& rrect = element.item;
    const Paint& paint = element.paint;
    const Matrix& transform = element.transform;

    const Vec2& scale = context->scale;
    Matrix scaled_m = Matrix::Scale(scale.x, scale.y) * transform;
    Matrix inv_m;
    scaled_m.Invert(&inv_m);

    auto m = Vec4(inv_m.GetScaleX(), inv_m.GetSkewY(), inv_m.GetSkewX(),
                  inv_m.GetScaleY());

    const Rect& rect = rrect.GetRect();
    const float stroke_radius =
        paint.GetStyle() == Paint::kStroke_Style
            ? std::max(paint.GetStrokeWidth() / 2.0f, 0.5f)
            : 0.0f;

    static_assert(static_cast<float>(Paint::kMiter_Join) == 0.0f);
    static_assert(static_cast<float>(Paint::kRound_Join) == 1.0f);
    static_assert(static_cast<float>(Paint::kBevel_Join) == 2.0f);

    float join = 0.0f;
    if (paint.GetStyle() == Paint::kStroke_Style && rrect.IsRect()) {
      if (paint.GetStrokeJoin() == Paint::kMiter_Join &&
          paint.GetStrokeMiter() < FloatSqrt2) {
        join = static_cast<float>(Paint::kBevel_Join);
      } else {
        join = static_cast<float>(paint.GetStrokeJoin());
      }
    }

    Instance instance = {
        Vec4{rect.Left(), rect.Top(), rect.Right(), rect.Bottom()},
        rrect.GetSimpleRadii(),
        Vec2{stroke_radius, join},
        m,
        Vec4{transform.GetScaleX(), transform.GetSkewY(), transform.GetSkewX(),
             transform.GetScaleY()},
        Vec2{transform.GetTranslateX(), transform.GetTranslateY()},
        Vec4{paint.GetColor4f()}};
    context->stageBuffer->AppendInstance<Instance>(instance);
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
}

namespace {

constexpr float PackCornerIdxAndOutside(float corner_idx, float outside) {
  return static_cast<float>(static_cast<uint32_t>(corner_idx) |
                            (static_cast<uint32_t>(outside) << 2));
}

struct Vertex {
  // offset => {1,0},{0,1},{1,1}
  // corner_idx => top_left:0, top_right:1, bottom_right:2, bottom_left:3
  // outside => outside:1, inside:0
  // region => corner:-1, edge:0, center:1,
  Vertex(Vec2 offset, float corner_idx, float outside, float region)
      : packed_data{offset.x, offset.y,
                    PackCornerIdxAndOutside(corner_idx, outside), region} {}

  Vec4 packed_data;
};

static_assert(sizeof(Vertex) == 16);

}  // namespace

GPUBufferView WGSLRRectGeometry::CreateVertexBufferView(
    HWStageBuffer* stage_bufer) {
  auto vertex_array = std::array<Vertex, 24>{
      // Left top corner
      Vertex{Vec2{1, 0}, kCornerTopLeft, kOutside, kRegionEdge},    //
      Vertex{Vec2{0, 1}, kCornerTopLeft, kOutside, kRegionEdge},    //
      Vertex{Vec2{1, 1}, kCornerTopLeft, kOutside, kRegionCorner},  //
      Vertex{Vec2{1, 0}, kCornerTopLeft, kInside, kRegionEdge},     //
      Vertex{Vec2{0, 1}, kCornerTopLeft, kInside, kRegionEdge},     //
      Vertex{Vec2{1, 0}, kCornerTopLeft, kInside, kRegionCenter},

      // Right top corner
      Vertex{Vec2{1, 0}, kCornerTopRight, kOutside, kRegionEdge},    //
      Vertex{Vec2{0, 1}, kCornerTopRight, kOutside, kRegionEdge},    //
      Vertex{Vec2{1, 1}, kCornerTopRight, kOutside, kRegionCorner},  //
      Vertex{Vec2{1, 0}, kCornerTopRight, kInside, kRegionEdge},     //
      Vertex{Vec2{0, 1}, kCornerTopRight, kInside, kRegionEdge},     //
      Vertex{Vec2{0, 1}, kCornerTopRight, kInside, kRegionCenter},

      // Right bottom corner
      Vertex{Vec2{1, 0}, kCornerBottomRight, kOutside, kRegionEdge},    //
      Vertex{Vec2{0, 1}, kCornerBottomRight, kOutside, kRegionEdge},    //
      Vertex{Vec2{1, 1}, kCornerBottomRight, kOutside, kRegionCorner},  //
      Vertex{Vec2{1, 0}, kCornerBottomRight, kInside, kRegionEdge},     //
      Vertex{Vec2{0, 1}, kCornerBottomRight, kInside, kRegionEdge},     //
      Vertex{Vec2{1, 0}, kCornerBottomRight, kInside, kRegionCenter},

      // Left bottom corner
      Vertex{Vec2{1, 0}, kCornerBottomLeft, kOutside, kRegionEdge},    //
      Vertex{Vec2{0, 1}, kCornerBottomLeft, kOutside, kRegionEdge},    //
      Vertex{Vec2{1, 1}, kCornerBottomLeft, kOutside, kRegionCorner},  //
      Vertex{Vec2{1, 0}, kCornerBottomLeft, kInside, kRegionEdge},     //
      Vertex{Vec2{0, 1}, kCornerBottomLeft, kInside, kRegionEdge},     //
      Vertex{Vec2{0, 1}, kCornerBottomLeft, kInside, kRegionCenter},
  };

  return stage_bufer->Push(reinterpret_cast<float*>(vertex_array.data()),
                           vertex_array.size() * sizeof(Vertex));
}

GPUBufferView WGSLRRectGeometry::CreateIndexBufferView(
    HWStageBuffer* stage_bufer) {
  std::vector<uint32_t> index_array;

  constexpr uint32_t kCornerVertexCount = 6;

  // 4 corners
  for (uint32_t i = 0; i < 4; i++) {
    uint32_t base_index = i * kCornerVertexCount;
    index_array.push_back(base_index + 0);
    index_array.push_back(base_index + 2);
    index_array.push_back(base_index + 3);

    index_array.push_back(base_index + 1);
    index_array.push_back(base_index + 2);
    index_array.push_back(base_index + 4);

    index_array.push_back(base_index + 2);
    index_array.push_back(base_index + 3);
    index_array.push_back(base_index + 4);

    // Center
    index_array.push_back(base_index + 3);
    index_array.push_back(base_index + 4);
    index_array.push_back(base_index + 5);
  }

  // 4 edges
  for (uint32_t i = 0; i < 4; i++) {
    // base_index_offset = 0 for horizontal edges, 1 for vertical edges
    uint32_t base_index_offset = i % 2;
    uint32_t prev_base_index =
        (i == 0 ? 3 : (i - 1)) * kCornerVertexCount + base_index_offset;
    uint32_t curr_base_index = i * kCornerVertexCount + base_index_offset;
    uint32_t curr_center_index = (i + 1) * kCornerVertexCount - 1;

    index_array.push_back(prev_base_index + 0);
    index_array.push_back(prev_base_index + 3);
    index_array.push_back(curr_base_index + 0);

    index_array.push_back(prev_base_index + 3);
    index_array.push_back(curr_base_index + 0);
    index_array.push_back(curr_base_index + 3);

    index_array.push_back(prev_base_index + 3);
    index_array.push_back(curr_base_index + 3);
    index_array.push_back(curr_center_index);
  }

  return stage_bufer->PushIndex(const_cast<uint32_t*>(index_array.data()),
                                index_array.size() * sizeof(uint32_t));
}

}  // namespace skity
