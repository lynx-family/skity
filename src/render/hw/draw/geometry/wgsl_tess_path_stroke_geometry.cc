// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/geometry/wgsl_tess_path_stroke_geometry.hpp"

#include <algorithm>
#include <optional>
#include <vector>

#include "src/geometry/conic.hpp"
#include "src/geometry/math.hpp"
#include "src/geometry/wangs_formula.hpp"
#include "src/graphic/path_visitor.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_path_aa_outline.hpp"
#include "src/render/hw/hw_path_raster.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/tracing.hpp"

namespace skity {

namespace {

constexpr static float kPrecision = 4.0f;
constexpr static int32_t kMaxNumSegmentsPerInstance = 16;

std::vector<GPUVertexBufferLayout> InitVertexBufferLayout() {
  std::vector<GPUVertexBufferLayout> layout = {
      // vertex
      GPUVertexBufferLayout{
          2 * sizeof(float),
          GPUVertexStepMode::kVertex,
          {
              GPUVertexAttribute{
                  GPUVertexFormat::kFloat32,
                  0,
                  0,
              },
              GPUVertexAttribute{
                  GPUVertexFormat::kFloat32,
                  sizeof(float),
                  1,
              },
          },
      },
      // instance
      GPUVertexBufferLayout{
          20 * sizeof(float),
          GPUVertexStepMode::kInstance,
          {
              GPUVertexAttribute{
                  GPUVertexFormat::kFloat32x4,
                  0,
                  2,
              },
              GPUVertexAttribute{
                  GPUVertexFormat::kFloat32x4,
                  4 * sizeof(float),
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
                  GPUVertexFormat::kFloat32x4,
                  16 * sizeof(float),
                  6,
              },
          },
      },
  };

  return layout;
}

struct Instance {
  Vec4 p0p1;
  Vec4 p2p3;
  Vec4 j0j1;
  Vec4 j2j3;
  float index_offset;
  float num_segments;
  float stroke_radius;
  float is_circle;
};

static_assert(sizeof(Instance) == 20 * sizeof(float));

struct TessPathStrokeVisitor : public PathVisitor {
  explicit TessPathStrokeVisitor(const Matrix& matrix, const Paint& paint,
                                 HWStageBuffer* stage_buffer)
      : PathVisitor(false, matrix),

        xform_(wangs_formula::VectorXform(matrix)),
        stroke_radius_(std::max(0.5f, paint.GetStrokeWidth() * 0.5f)),
        stroke_miter_(paint.GetStrokeMiter()),
        join_(paint.GetStrokeJoin()),
        cap_(paint.GetStrokeCap()),
        stage_buffer_(stage_buffer) {}

  void OnBeginPath() override {}

  void OnEndPath() override { HandleCaps(); }

  void HandleCaps() {
    if (only_has_move_to_ || is_closed_ || cap_ == Paint::kButt_Cap) {
      return;
    }

    if (cap_ == Paint::kRound_Cap) {
      AddCircleInstances(first_point_);

      if (last_point_ != first_point_) {
        AddCircleInstances(last_point_);
      }
    } else if (cap_ == Paint::kSquare_Cap) {
      if (first_segment_offset_ >= 0) {
        // start cap
        {
          Instance& instance =
              *stage_buffer_->ToInstance<Instance>(first_segment_offset_);
          const Vec2& p0 = instance.p0p1.xy();
          const Vec2& p1 = instance.p0p1.zw();
          const Vec2& p2 = instance.p2p3.xy();
          const Vec2& p3 = instance.p2p3.zw();
          auto out_dir = (p0 - GetTangentPoint(p0, p1, p2, p3)).Normalize();
          AddLineInstance(p0, p0 + out_dir * stroke_radius_, false);
        }
        // end cap
        {
          auto out_dir = (last_point_ - join_point_).Normalize();
          AddLineInstance(last_point_, last_point_ + out_dir * stroke_radius_,
                          false);
        }
      } else {
        // This case only happens when the path is a point
        AddLineInstance(first_point_ - Vec2{stroke_radius_, 0},
                        first_point_ + Vec2{stroke_radius_, 0}, false);
      }
    }
  }

  void OnMoveTo(Vec2 const& p) override {
    HandleCaps();
    only_has_move_to_ = true;
    first_point_ = p;
    last_point_ = p;
    first_segment_offset_ = -1;
    is_closed_ = false;
  }

  void OnLineTo(Vec2 const& p0, Vec2 const& p1) override {
    only_has_move_to_ = false;
    if (p0 == p1) {
      return;
    }

    uint32_t segment_offset =
        AddLineInstance(p0, p1, first_segment_offset_ >= 0);
    if (first_segment_offset_ < 0) {
      first_segment_offset_ = segment_offset;
    }
    join_point_ = p0;
    last_point_ = p1;
  }

  void OnQuadTo(Vec2 const& p0, Vec2 const& p1, Vec2 const& p2) override {
    auto ctrl1 = (p0 + 2 * p1) / 3.f;
    auto ctrl2 = (2 * p1 + p2) / 3.f;
    OnCubicTo(p0, ctrl1, ctrl2, p2);
  }

  void OnConicTo(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3,
                 float weight) override {
    Point start = {p1.x, p1.y, 0.f, 1.f};
    Point control = {p2.x, p2.y, 0.f, 1.f};
    Point end = {p3.x, p3.y, 0.f, 1.f};

    std::array<Point, 5> quads{};
    Conic conic{start, control, end, weight};
    conic.ChopIntoQuadsPOW2(quads.data(), 1);
    quads[0] = start;
    OnQuadTo(Vec2{quads[0]}, Vec2{quads[1]}, Vec2{quads[2]});
    OnQuadTo(Vec2{quads[2]}, Vec2{quads[3]}, Vec2{quads[4]});
  }

  void OnCubicTo(Vec2 const& p0, Vec2 const& p1, Vec2 const& p2,
                 Vec2 const& p3) override {
    only_has_move_to_ = false;
    if (p0 == p1 && p1 == p2 && p2 == p3) {
      return;
    }

    arc_[0] = p0;
    arc_[1] = p1;
    arc_[2] = p2;
    arc_[3] = p3;
    uint32_t num = std::ceil(wangs_formula::Cubic(kPrecision, arc_, xform_));
    num = std::max(num, 1u);

    uint32_t count = DivCeil(num, kMaxNumSegmentsPerInstance);

    auto segment_offset =
        AddCubicInstance(p0, p1, p2, p3, 0.f, num, first_segment_offset_ >= 0);
    for (uint32_t i = 1; i < count; i++) {
      AddCubicInstance(p0, p1, p2, p3,
                       static_cast<float>(i * kMaxNumSegmentsPerInstance), num,
                       false);
    }

    if (first_segment_offset_ < 0) {
      first_segment_offset_ = segment_offset;
    }

    last_point_ = p3;
    join_point_ = GetTangentPoint(p3, p2, p1, p0);
  }

  void OnClose() override {
    if (only_has_move_to_) {
      return;
    }

    if (first_segment_offset_ >= 0) {
      auto& instance =
          *stage_buffer_->ToInstance<Instance>(first_segment_offset_);
      const Vec2& p0 = instance.p0p1.xy();
      const Vec2& p1 = instance.p0p1.zw();
      const Vec2& p2 = instance.p2p3.xy();
      const Vec2& p3 = instance.p2p3.zw();
      GenerateJoin(join_point_, p0, GetTangentPoint(p0, p1, p2, p3), instance);
    }

    is_closed_ = true;
  }

  inline Vec2 GetTangentPoint(const Vec2& p0, const Vec2& p1, const Vec2& p2,
                              const Vec2& p3) const {
    if (p1 != p0) {
      return p1;
    }
    if (p2 != p1) {
      return p2;
    }
    return p3;
  }

 private:
  void AddCircleInstances(const Vec2& center) {
    uint32_t num;
    if (!semicircle_segments_num_.has_value()) {
      arc_[0] = center + Vec2{stroke_radius_, 0};
      arc_[1] = center;
      arc_[2] = center + Vec2{0, stroke_radius_};
      num = std::ceil(2 *
                      wangs_formula::Conic(kPrecision, arc_, FloatRoot2Over2));
      num = std::max(num, 1u);
      semicircle_segments_num_ = num;
    } else {
      num = semicircle_segments_num_.value();
    }
    uint32_t count = DivCeil(num, kMaxNumSegmentsPerInstance);
    Vec2 left = center - Vec2{stroke_radius_, 0};
    for (uint32_t i = 0; i < count; i++) {
      auto offset = stage_buffer_->AppendInstance<Instance>();
      Instance& circle = *stage_buffer_->ToInstance<Instance>(offset);

      circle.p0p1 = Vec4{center, center};
      circle.p2p3 = Vec4{center, center};
      circle.j0j1 = Vec4{left, left};
      circle.j2j3 = Vec4{left, left};
      circle.index_offset = i * kMaxNumSegmentsPerInstance;
      circle.num_segments = num;
      circle.is_circle = 1.f;
      circle.stroke_radius = stroke_radius_;
    }
  }

  uint32_t AddLineInstance(const Vec2& p0, const Vec2& p1, bool needs_join) {
    auto offset = stage_buffer_->AppendInstance<Instance>();
    Instance& line = *stage_buffer_->ToInstance<Instance>(offset);

    Vec2 ctrl1 = 2.f / 3.f * p0 + 1.f / 3.f * p1;
    Vec2 ctrl2 = 2.f / 3.f * p1 + 1.f / 3.f * p0;
    line.p0p1 = Vec4{p0, ctrl1};
    line.p2p3 = Vec4{ctrl2, p1};

    line.index_offset = 0.f;
    line.num_segments = 1.f;
    line.is_circle = 0.f;
    line.stroke_radius = stroke_radius_;
    if (needs_join) {
      GenerateJoin(join_point_, p0, p1, line);
    } else {
      line.j0j1 = Vec4{p0, p0};
      line.j2j3 = Vec4{p0, p0};
    }
    return offset;
  }

  uint32_t AddCubicInstance(const Vec2& p0, const Vec2& p1, const Vec2& p2,
                            const Vec2& p3, float index_offset,
                            float num_segments, bool needs_join) {
    auto offset = stage_buffer_->AppendInstance<Instance>();
    Instance& cubic = *stage_buffer_->ToInstance<Instance>(offset);
    cubic.p0p1 = Vec4{p0, p1};
    cubic.p2p3 = Vec4{p2, p3};
    cubic.index_offset = index_offset;
    cubic.num_segments = num_segments;
    cubic.is_circle = 0.f;
    cubic.stroke_radius = stroke_radius_;

    if (needs_join) {
      GenerateJoin(join_point_, p0, GetTangentPoint(p0, p1, p2, p3), cubic);
    } else {
      cubic.j0j1 = Vec4{p0, p0};
      cubic.j2j3 = Vec4{p0, p0};
    }
    return offset;
  }

  void GenMiterJoin(const Vec2& center, const Vec2& p1, const Vec2& p2,
                    float stroke_radius, float stroke_miter,
                    Instance& instance) {
    auto pp1 = p1 - center;
    auto pp2 = p2 - center;

    auto out_dir = pp1 + pp2;

    float k = 2.f * stroke_radius * stroke_radius /
              (out_dir.x * out_dir.x + out_dir.y * out_dir.y);

    auto pe = k * out_dir;

    if (pe.Length() >= stroke_miter * stroke_radius) {
      // fallback to bevel_join
      instance.j0j1 = Vec4{center, p1};
      instance.j2j3 = Vec4{p2, center};
      return;
    }

    auto join = center + pe;

    instance.j0j1 = Vec4{center, p1};
    instance.j2j3 = Vec4{join, p2};
  }

  void GenerateJoin(Vec2 prev, Vec2 curr, Vec2 next, Instance& instance) {
    auto orientation = CalculateOrientation(prev, curr, next);

    auto cross_pr = CrossProductResult(prev, curr, next);
    if (orientation == Orientation::kLinear && cross_pr > 0) {
      instance.j0j1 = Vec4{curr, curr};
      instance.j2j3 = Vec4{curr, curr};
      return;
    }

    auto prev_dir = (curr - prev).Normalize();
    auto curr_dir = (next - curr).Normalize();

    auto prev_normal = Vec2{-prev_dir.y, prev_dir.x};
    auto current_normal = Vec2{-curr_dir.y, curr_dir.x};

    Vec2 prev_join = {};
    Vec2 curr_join = {};

    if (orientation == Orientation::kAntiClockWise ||
        (orientation == Orientation::kLinear && cross_pr < 0)) {
      prev_join = curr - prev_normal * stroke_radius_;
      curr_join = curr - current_normal * stroke_radius_;
    } else {
      prev_join = curr + prev_normal * stroke_radius_;
      curr_join = curr + current_normal * stroke_radius_;
    }

    if ((orientation == Orientation::kLinear && join_ != Paint::kRound_Join) ||
        join_ == Paint::kBevel_Join) {
      instance.j0j1 = Vec4{curr, prev_join};
      instance.j2j3 = Vec4{curr_join, curr};
      return;
    }

    if (join_ == Paint::kMiter_Join) {
      GenMiterJoin(curr, prev_join, curr_join, stroke_radius_, stroke_miter_,
                   instance);
    } else if (join_ == Paint::kRound_Join) {
      float delta = (prev_join - curr_join).Length();
      if (delta < 1.f) {
        instance.j0j1 = Vec4{curr, prev_join};
        instance.j2j3 = Vec4{curr_join, curr};
      } else {
        instance.j0j1 = Vec4{curr, curr};
        instance.j2j3 = Vec4{curr, curr};
        AddCircleInstances(curr);
      }
    }
  }

  wangs_formula::VectorXform xform_;
  Vec2 arc_[4];

  Vec2 first_point_;
  Vec2 last_point_;
  Vec2 join_point_;
  bool only_has_move_to_ = true;
  int32_t first_segment_offset_ = -1;
  bool is_closed_ = false;

  const float stroke_radius_;
  const float stroke_miter_;
  const Paint::Join join_;
  const Paint::Cap cap_;
  std::optional<uint32_t> semicircle_segments_num_;
  HWStageBuffer* stage_buffer_;
};

}  // namespace

WGSLTessPathStrokeGeometry::WGSLTessPathStrokeGeometry(const Path& path,
                                                       const Paint& paint)
    : HWWGSLGeometry(Flags::kSnippet),
      path_(path),
      paint_(paint),
      layout_(InitVertexBufferLayout()) {}

const std::vector<GPUVertexBufferLayout>&
WGSLTessPathStrokeGeometry::GetBufferLayout() const {
  return layout_;
}

void WGSLTessPathStrokeGeometry::WriteVSFunctionsAndStructs(
    std::stringstream& ss) const {
  ss << CommonVertexWGSL();
  ss << R"(
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
)";
}

void WGSLTessPathStrokeGeometry::WriteVSUniforms(std::stringstream& ss) const {
  ss << "@group(0) @binding(0) var<uniform> common_slot: CommonSlot;\n";
}

void WGSLTessPathStrokeGeometry::WriteVSInput(std::stringstream& ss) const {
  ss << R"(
struct VSInput {
  @location(0) index: f32,
  @location(1) offset : f32,
  @location(2) p0p1 : vec4<f32>,
  @location(3) p2p3 : vec4<f32>,
  @location(4) j0j1 : vec4<f32>,
  @location(5) j2j3 : vec4<f32>,
  @location(6) pack : vec4<f32>,
};
)";
}

void WGSLTessPathStrokeGeometry::WriteVSMain(std::stringstream& ss) const {
  ss << R"(
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
)";
}

std::string WGSLTessPathStrokeGeometry::GetShaderName() const {
  std::string name = "TessPathStroke";
  return name;
}

void WGSLTessPathStrokeGeometry::PrepareCMD(Command* cmd,
                                            HWDrawContext* context,
                                            const Matrix& transform,
                                            float clip_depth,
                                            Command* stencil_cmd) {
  SKITY_TRACE_EVENT(WGSLTessPathStrokeGeometry_PrepareCMD);

  // check the stencil cmd to determine if this is inside a coverage step
  // but this may be changed when implement draw call mergeing in dynamic shader
  // pipeline.
  if (stencil_cmd) {
    cmd->index_buffer = stencil_cmd->index_buffer;
    cmd->vertex_buffer = stencil_cmd->vertex_buffer;
    cmd->index_count = stencil_cmd->index_count;
    cmd->uniform_bindings = stencil_cmd->uniform_bindings.Clone();
    cmd->instance_count = stencil_cmd->instance_count;
    cmd->instance_buffer = stencil_cmd->instance_buffer;
    return;
  }

  if (cmd->pipeline == nullptr) {
    return;
  }

  const Vec2& scale = context->scale;

  auto pipeline = cmd->pipeline;
  cmd->vertex_buffer =
      context->static_buffer->GetTessPathStrokeVertexBufferView();
  cmd->index_buffer =
      context->static_buffer->GetTessPathStrokeIndexBufferView();
  cmd->index_count = cmd->index_buffer.range / sizeof(uint32_t);
  context->stageBuffer->BeginWritingInstance(
      2 * path_.CountVerbs() * sizeof(Instance), alignof(Instance));
  TessPathStrokeVisitor path_visitor(
      Matrix::Scale(scale.x, scale.y) * transform, paint_,
      context->stageBuffer);
  path_visitor.VisitPath(path_, false);
  auto instance_buffer_view = context->stageBuffer->EndWritingInstance();
  cmd->instance_count = instance_buffer_view.range / sizeof(Instance);
  cmd->instance_buffer = instance_buffer_view;

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

GPUBufferView WGSLTessPathStrokeGeometry::CreateVertexBufferView(
    HWStageBuffer* stage_bufer) {
  std::vector<float> vertex_array;

  for (int i = 0; i < 2 * (kMaxNumSegmentsPerInstance + 1); i++) {
    auto offset = i / (kMaxNumSegmentsPerInstance + 1) == 0 ? 1.f : -1.f;
    vertex_array.push_back(i % (kMaxNumSegmentsPerInstance + 1));  // index
    vertex_array.push_back(offset);                                // offset
  }

  // Join Vertex
  for (int i = 1; i <= 4; i++) {
    vertex_array.push_back(-i);  // index
    vertex_array.push_back(0);   // offset
  }

  return stage_bufer->Push(const_cast<float*>(vertex_array.data()),
                           vertex_array.size() * sizeof(float));
}

GPUBufferView WGSLTessPathStrokeGeometry::CreateIndexBufferView(
    HWStageBuffer* stage_bufer) {
  std::vector<uint32_t> index_array;
  for (int i = 0; i < kMaxNumSegmentsPerInstance; i++) {
    uint32_t outer_curr = i;
    uint32_t outer_next = i + 1;
    uint32_t inner_curr = outer_curr + (kMaxNumSegmentsPerInstance + 1);
    uint32_t inner_next = outer_next + (kMaxNumSegmentsPerInstance + 1);

    index_array.push_back(outer_curr);
    index_array.push_back(outer_next);
    index_array.push_back(inner_next);

    index_array.push_back(outer_curr);
    index_array.push_back(inner_next);
    index_array.push_back(inner_curr);
  }

  uint32_t join_index_base = 2 * (kMaxNumSegmentsPerInstance + 1);

  index_array.push_back(join_index_base);      // -1
  index_array.push_back(join_index_base + 1);  // -2
  index_array.push_back(join_index_base + 2);  // -3

  index_array.push_back(join_index_base);      // -1
  index_array.push_back(join_index_base + 2);  // -3
  index_array.push_back(join_index_base + 3);  // -4

  return stage_bufer->PushIndex(const_cast<uint32_t*>(index_array.data()),
                                index_array.size() * sizeof(uint32_t));
}

}  // namespace skity
