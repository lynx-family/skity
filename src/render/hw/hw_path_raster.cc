// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_path_raster.hpp"

#include "src/geometry/conic.hpp"
#include "src/logging.hpp"

namespace skity {

constexpr static float kPrecision = 4.0f;

void HWPathFillRaster::OnMoveTo(const Vec2& p) {
  first_pt_ = p;

  first_pt_index_ = AppendLineVertex(p);
}

void HWPathFillRaster::OnLineTo(const Vec2& p1, const Vec2& p2) {
  if (p1 == first_pt_ || p2 == first_pt_) {
    return;
  }

  auto orientation = CalculateOrientation(first_pt_, p1, p2);

  if (orientation == Orientation::kLinear) {
    return;
  }

  auto i1 = AppendLineVertex(p1);
  auto i2 = AppendLineVertex(p2);

  if (orientation == Orientation::kAntiClockWise) {
    AppendFrontTriangle(first_pt_index_, i1, i2);
  } else {
    AppendBackTriangle(first_pt_index_, i1, i2);
  }
}

void HWPathStrokeRaster::OnEndPath() { HandleJoinAndCap(); }

void HWPathStrokeRaster::OnMoveTo(const Vec2& p) {
  HandleJoinAndCap();

  stroke_pts_.emplace_back(StrokePoint{p, true});

  only_has_move_to_ = true;
}

void HWPathStrokeRaster::OnLineTo(const Vec2& p1, const Vec2& p2) {
  only_has_move_to_ = false;
  if (!FloatNearlyZero(p2.x - p1.x) || !FloatNearlyZero(p2.y - p1.y)) {
    auto aabb = ExpandLine(p1, p2);

    uint32_t a = AppendLineVertex(aabb[0]);
    uint32_t b = AppendLineVertex(aabb[1]);
    uint32_t c = AppendLineVertex(aabb[2]);
    uint32_t d = AppendLineVertex(aabb[3]);

    AppendRect(a, b, c, d);
  }

  if (stroke_pts_.back().xy != p1) {
    stroke_pts_.emplace_back(StrokePoint{p1, true});
  }

  if (p2 != p1) {
    stroke_pts_.emplace_back(StrokePoint{p2, true});
  }
}

void HWPathStrokeRaster::OnQuadTo(const Vec2& p1, const Vec2& p2,
                                  const Vec2& p3) {
  only_has_move_to_ = false;
  std::array<Vec2, 3> arc{p1, p2, p3};

  auto num = glm::ceil(wangs_formula::Quadratic(4.f, arc.data(), xform_));

  if (num <= 1.f) {
    OnLineTo(p1, p3);
    return;
  }

  QuadCoeff coeff(arc);

  int num_step = static_cast<int>(num) + 1;

  if (num_step > GEOMETRY_CURVE_RASTER_LIMIT) {
    num_step = GEOMETRY_CURVE_RASTER_LIMIT;
  }

  float step = 1.f / (num_step - 1.f);
  float u = 0.f;

  std::array<size_t, GEOMETRY_CURVE_RASTER_LIMIT> indexes1{};
  std::array<size_t, GEOMETRY_CURVE_RASTER_LIMIT> indexes2{};

  for (int i = 0; i < num_step; i++) {
    auto p = coeff.eval(u);

    auto t = QuadCoeff::EvalQuadTangentAt(p1, p2, p3, u);
    auto n = Vec2(t.y, -t.x);

    auto up = p + n * stroke_radius_;
    auto dp = p - n * stroke_radius_;

    indexes1[i] = AppendLineVertex(up);
    indexes2[i] = AppendLineVertex(dp);

    u += step;
  }

  for (int i = 0; i < num_step - 1; i++) {
    AppendFrontTriangle(indexes1[i], indexes2[i], indexes1[i + 1]);
    AppendFrontTriangle(indexes2[i], indexes1[i + 1], indexes2[i + 1]);
  }

  if (stroke_pts_.back().xy != p1) {
    stroke_pts_.emplace_back(StrokePoint{p1, true});
  }

  stroke_pts_.emplace_back(StrokePoint{p2, false});
  stroke_pts_.emplace_back(StrokePoint{p3, true});
}

void HWPathStrokeRaster::OnConicTo(const Vec2& p1, const Vec2& p2,
                                   const Vec2& p3, float weight) {
  only_has_move_to_ = false;
  std::array<Vec2, 3> arc{p1, p2, p3};

  float num = glm::ceil(wangs_formula::Conic(4.f, arc.data(), weight, xform_));

  if (num <= 1.f) {
    OnLineTo(p1, p3);
    return;
  }

  int num_step = static_cast<int>(num) + 1;

  if (num_step >= GEOMETRY_CURVE_RASTER_LIMIT) {
    num_step = GEOMETRY_CURVE_RASTER_LIMIT;
  }

  float step = 1.f / (num_step - 1.f);
  float u = 0.f;

  Conic coeff;
  coeff.Set(Point{p1.x, p1.y, 0.f, 1.f}, Point{p2.x, p2.y, 0.f, 1.f},
            Point{p3.x, p3.y, 0.f, 1.f}, weight);

  std::array<size_t, GEOMETRY_CURVE_RASTER_LIMIT> indexes1{};
  std::array<size_t, GEOMETRY_CURVE_RASTER_LIMIT> indexes2{};

  for (int i = 0; i < num_step; i++) {
    Vec2 p = Vec2{coeff.EvalAt(u)};

    Vec2 t = Vec2{coeff.evalTangentAt(u)};
    t = t.Normalize();
    auto n = Vec2(t.y, -t.x);

    auto up = p + n * stroke_radius_;
    auto dp = p - n * stroke_radius_;

    indexes1[i] = AppendLineVertex(up);
    indexes2[i] = AppendLineVertex(dp);

    u += step;
  }

  for (int i = 0; i < num_step - 1; i++) {
    AppendFrontTriangle(indexes1[i], indexes2[i], indexes1[i + 1]);
    AppendFrontTriangle(indexes2[i], indexes1[i + 1], indexes2[i + 1]);
  }

  if (stroke_pts_.back().xy != p1) {
    stroke_pts_.emplace_back(StrokePoint{p1, true});
  }

  stroke_pts_.emplace_back(StrokePoint{p2, false});
  stroke_pts_.emplace_back(StrokePoint{p3, true});
}

void HWPathStrokeRaster::OnCubicTo(const Vec2& p1, const Vec2& p2,
                                   const Vec2& p3, const Vec2& p4) {
  only_has_move_to_ = false;

  std::array<Vec2, 4> arc{p1, p2, p3, p4};

  auto num = glm::ceil(wangs_formula::Cubic(4.f, arc.data(), xform_));

  if (num <= 1.f) {
    OnLineTo(p1, p4);
    return;
  }

  int num_step = static_cast<int>(num) + 1;

  if (num_step >= GEOMETRY_CURVE_RASTER_LIMIT) {
    num_step = GEOMETRY_CURVE_RASTER_LIMIT;
  }

  float step = 1.f / (num_step - 1.f);
  float u = 0.f;

  CubicCoeff coeff{arc};

  std::array<size_t, GEOMETRY_CURVE_RASTER_LIMIT> indexes1{};
  std::array<size_t, GEOMETRY_CURVE_RASTER_LIMIT> indexes2{};

  for (int i = 0; i < num_step; i++) {
    Vec2 p = Vec2{coeff.EvalAt(u)};

    Vec2 t = coeff.EvalTangentAt(u);
    t = t.Normalize();

    if (glm::isnan(t.x) || glm::isnan(t.y)) {
      if (FloatNearlyZero((p2 - p1).Length())) {
        t = (p3 - p1).Normalize();
      } else {
        t = (p2 - p1).Normalize();
      }
    }

    auto n = Vec2(t.y, -t.x);

    auto up = p + n * stroke_radius_;
    auto dp = p - n * stroke_radius_;

    indexes1[i] = AppendLineVertex(up);
    indexes2[i] = AppendLineVertex(dp);

    u += step;
  }

  for (int i = 0; i < num_step - 1; i++) {
    AppendFrontTriangle(indexes1[i], indexes2[i], indexes1[i + 1]);
    AppendFrontTriangle(indexes2[i], indexes1[i + 1], indexes2[i + 1]);
  }

  if (stroke_pts_.back().xy != p1) {
    stroke_pts_.emplace_back(StrokePoint{p1, true});
  }

  stroke_pts_.emplace_back(StrokePoint{p2, false});
  stroke_pts_.emplace_back(StrokePoint{p3, false});
  stroke_pts_.emplace_back(StrokePoint{p4, true});
}

void HWPathStrokeRaster::OnClose() {
  if (stroke_pts_.empty()) {
    return;
  }

  if (stroke_pts_.back().xy != stroke_pts_.front().xy) {
    stroke_pts_.emplace_back(stroke_pts_.front());
  }

  stroke_pts_.back().closed = true;

  HandleJoinAndCap();
}

void HWPathStrokeRaster::HandleJoinAndCap() {
  if (stroke_pts_.empty()) {
    return;
  }

  if (only_has_move_to_) {
    stroke_pts_.clear();
    return;
  }

  HandleLineCap();

  HandleLineJoin();

  stroke_pts_.clear();
}

void HWPathStrokeRaster::HandleLineCap() {
  if (stroke_pts_.back().closed) {
    // path close, no cap need to handle
    return;
  }

  if (cap_ == Paint::kButt_Cap) {
    // butt cap nothing need to do
    return;
  }

  auto p1 = stroke_pts_.front();

  Vec2 p1_out{};
  if (stroke_pts_.size() == 1 || stroke_pts_[1].xy == p1.xy) {
    p1_out.x = 1.f;
    p1_out.y = 0.f;
  } else {
    p1_out = (p1.xy - stroke_pts_[1].xy).Normalize();
  }

  auto p2 = stroke_pts_.back();

  Vec2 p2_out{};
  if (stroke_pts_.size() == 1 ||
      stroke_pts_[stroke_pts_.size() - 2].xy == p2.xy) {
    p2_out.x = -1.f;
    p2_out.y = 0.f;
  } else {
    p2_out = (p2.xy - stroke_pts_[stroke_pts_.size() - 2].xy).Normalize();
  }

  if (cap_ == Paint::kRound_Cap) {
    GenerateCircleMesh(p1.xy);
    if (p1.xy != p2.xy) {
      GenerateCircleMesh(p2.xy);
    }
  } else {
    GenSquareCap(p1.xy, p1_out);
    GenSquareCap(p2.xy, p2_out);
  }
}

void HWPathStrokeRaster::HandleLineJoin() {
  if (stroke_pts_.size() < 3) {
    return;
  }

  for (size_t i = 1; i < stroke_pts_.size(); i++) {
    if (!stroke_pts_[i].physical) {
      continue;
    }

    if (i == stroke_pts_.size() - 1 && !stroke_pts_[i].closed) {
      continue;
    }

    auto curr = stroke_pts_[i].xy;
    auto prev = stroke_pts_[i - 1].xy;

    size_t next_i = i + 1;
    if (i == stroke_pts_.size() - 1) {
      next_i = 1;
    }

    auto next = stroke_pts_[next_i].xy;

    auto orientation = CalculateOrientation(prev, curr, next);

    auto cross_pr = CrossProductResult(prev, curr, next);
    if (orientation == Orientation::kLinear && cross_pr > 0) {
      continue;
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
      GenBevelJoin(curr, prev_join, curr_join);
      continue;
    }

    if (join_ == Paint::kMiter_Join) {
      GenMiterJoin(curr, prev_join, curr_join);
    } else if (join_ == Paint::kRound_Join) {
      float delta = (prev_join - curr_join).Length();

      if (delta < 1.f) {
        GenBevelJoin(curr, prev_join, curr_join);
      } else {
        GenerateCircleMesh(curr);
      }
    }
  }
}

std::array<Vec2, 4> HWPathStrokeRaster::ExpandLine(Vec2 const& p0,
                                                   Vec2 const& p1) const {
  std::array<Vec2, 4> ret = {};

  Vec2 dir = (p1 - p0).Normalize();
  Vec2 normal = {-dir.y, dir.x};

  ret[0] = p0 + normal * stroke_radius_;
  ret[1] = p0 - normal * stroke_radius_;

  ret[2] = p1 + normal * stroke_radius_;
  ret[3] = p1 - normal * stroke_radius_;

  return ret;
}

void HWPathStrokeRaster::GenSquareCap(const Vec2& center, const Vec2& out_dir) {
  Vec2 norm{out_dir.y, -out_dir.x};

  Vec2 p1 = center + norm * stroke_radius_;
  Vec2 p2 = center - norm * stroke_radius_;

  GenerateSquareMesh(p1, p2, out_dir);
}

void HWPathStrokeRaster::GenMiterJoin(const Vec2& center, const Vec2& p1,
                                      const Vec2& p2) {
  auto pp1 = p1 - center;
  auto pp2 = p2 - center;

  auto out_dir = pp1 + pp2;

  float k = 2.f * stroke_radius_ * stroke_radius_ /
            (out_dir.x * out_dir.x + out_dir.y * out_dir.y);

  auto pe = k * out_dir;

  if (pe.Length() >= stroke_miter_ * stroke_radius_) {
    // fallback to bevel_join
    GenBevelJoin(center, p1, p2);
    return;
  }

  auto join = center + pe;

  auto c = AppendLineVertex(center);

  auto cp1 = AppendLineVertex(p1);
  auto cp2 = AppendLineVertex(p2);

  auto e = AppendLineVertex(join);

  AppendFrontTriangle(c, cp1, e);
  AppendFrontTriangle(c, cp2, e);
}

void HWPathStrokeRaster::GenBevelJoin(const Vec2& center, const Vec2& p1,
                                      const Vec2& p2) {
  auto a = AppendLineVertex(center);
  auto b = AppendLineVertex(p1);
  auto c = AppendLineVertex(p2);

  AppendFrontTriangle(a, b, c);
}

void HWPathStrokeRaster::GenerateCircleMesh(Vec2 const& center) {
  if (circle_mesh_points_.empty()) {
    Vec2 p1 = center + Vec2{stroke_radius_, 0};
    Vec2 p2 = center + Vec2{0, stroke_radius_};
    std::array<const Vec2, 3> arc{p1, center, p2};
    uint32_t semicircle_segments_num =
        std::ceil(2 * wangs_formula::Conic(kPrecision, arc.data(),
                                           FloatRoot2Over2, xform_));
    semicircle_segments_num = std::max(semicircle_segments_num, 2u);
    float angle = FloatPI / semicircle_segments_num;
    circle_mesh_points_.reserve(2 * semicircle_segments_num + 1);
    for (int i = 0; i <= 2 * semicircle_segments_num; i++) {
      circle_mesh_points_.push_back(
          Vec2{std::cos(angle * i), std::sin(angle * i)} * stroke_radius_);
    }
    DEBUG_CHECK(circle_mesh_points_.size() == 2 * semicircle_segments_num + 1);
  }

  auto c = AppendLineVertex(center);
  auto prev = AppendLineVertex(center + circle_mesh_points_[0]);
  for (int i = 1; i < circle_mesh_points_.size(); i++) {
    auto curr = AppendLineVertex(center + circle_mesh_points_[i]);
    AppendFrontTriangle(c, prev, curr);
    prev = curr;
  }
}

void HWPathStrokeRaster::GenerateSquareMesh(Vec2 const& p1, Vec2 const& p2,
                                            Vec2 const& out_dir) {
  auto out_vec = out_dir * stroke_radius_;
  auto expand_p1 = p1 + out_vec;
  auto expand_p2 = p2 + out_vec;
  AppendRect(AppendLineVertex(p1), AppendLineVertex(p2),
             AppendLineVertex(expand_p1), AppendLineVertex(expand_p2));
}

}  // namespace skity
