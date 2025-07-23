// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_path_aa_outline.hpp"

#include "src/logging.hpp"

namespace skity {

HWPathAAOutline::HWPathAAOutline(const Matrix& matrix,
                                 VectorCache<float>* vertex_vector_cache,
                                 VectorCache<uint32_t>* index_vector_cache,
                                 float context_scale)
    : HWPathVisitor(Paint{}, true, matrix, vertex_vector_cache,
                    index_vector_cache) {
  float scaleX = matrix[1][0] != 0 ? sqrt(matrix[0][0] * matrix[0][0] +
                                          matrix[1][0] * matrix[1][0] +
                                          matrix[2][0] * matrix[2][0])
                                   : matrix.GetScaleX();

  if (scaleX > 0.f) {
    fringe_ = 1.f / scaleX;
  }

  fringe_ /= context_scale;
}

void HWPathAAOutline::StrokeAAOutline(const Path& path) {
  is_convex_ = path.IsConvex();

  if (is_convex_) {
    aa_normal_dir_ =
        path.GetFirstDirection() == Path::Direction::kCCW ? -1.f : 1.f;
  }

  VisitPath(path, true);
}

void HWPathAAOutline::OnBeginPath() {}

void HWPathAAOutline::OnEndPath() { AssembleEdgeAAPrimitive(); }

void HWPathAAOutline::OnMoveTo(Vec2 const& p) {
  AssembleEdgeAAPrimitive();

  outline_pts_.push_back(p);
}

void HWPathAAOutline::OnLineTo(Vec2 const& p1, Vec2 const& p2) {
  outline_pts_.push_back(p2);
}

void HWPathAAOutline::OnClose() { AssembleEdgeAAPrimitive(); }

void HWPathAAOutline::AssembleEdgeAAPrimitive() {
  if (outline_pts_.empty()) {
    return;
  }

  if (outline_pts_.front() == outline_pts_.back()) {
    outline_pts_.pop_back();
  }

  if (outline_pts_.size() < 3) {
    outline_pts_.clear();
    LOGE("AA outline parse error, subpath has less than 3 points");
    return;
  }

  for (size_t i = 0; i < outline_pts_.size(); i++) {
    auto prev_pt_i = i == 0 ? outline_pts_.size() - 1 : i - 1;
    auto from = outline_pts_[i];
    auto to = outline_pts_[(i + 1) % outline_pts_.size()];

    auto curr_dir = glm::normalize(to - from);
    auto vertical_line = Vec2(curr_dir.y, -curr_dir.x) * aa_normal_dir_;

    auto from_1 = from + vertical_line * fringe_;
    auto from_2 = from - vertical_line * fringe_;
    auto to_1 = to + vertical_line * fringe_;
    auto to_2 = to - vertical_line * fringe_;

    auto from_index = AppendLineVertex(from, 1.f);
    auto to_index = AppendLineVertex(to, 1.f);

    auto from_1_index = AppendLineVertex(from_1, 0.f);
    auto to_1_index = AppendLineVertex(to_1, 0.f);

    AppendFrontTriangle(from_1_index, from_index, to_index);
    AppendFrontTriangle(from_1_index, to_index, to_1_index);

    if (!is_convex_) {
      auto from_2_index = AppendLineVertex(from_2, 0.f);
      auto to_2_index = AppendLineVertex(to_2, 0.f);

      AppendFrontTriangle(from_2_index, from_index, to_index);
      AppendFrontTriangle(from_2_index, to_index, to_2_index);
    }

    auto prev = outline_pts_[prev_pt_i];
    auto prev_dir = glm::normalize(from - prev);
    auto prev_vertical_dir = Vec2(prev_dir.y, -prev_dir.x);
    auto out_dir = (prev_dir - curr_dir) * 0.5f;

    Vec2 aa_p1;
    if (glm::dot(out_dir, prev_vertical_dir) < 0.f) {
      aa_p1 = from - prev_vertical_dir * fringe_;
    } else {
      aa_p1 = from + prev_vertical_dir * fringe_;
    }

    Vec2 aa_p2 = from_1;
    if (glm::dot(out_dir, vertical_line) < 0) {
      aa_p2 = from_2;
    }

    auto p1_index = AppendLineVertex(aa_p1, 0.f);
    auto p2_index = AppendLineVertex(aa_p2, 0.f);

    AppendFrontTriangle(p1_index, p2_index, from_index);
  }

  outline_pts_.clear();
}

}  // namespace skity
