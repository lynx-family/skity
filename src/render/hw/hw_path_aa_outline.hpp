// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_PATH_AA_OUTLINE_HPP
#define SRC_RENDER_HW_HW_PATH_AA_OUTLINE_HPP

#include "src/render/hw/hw_path_visitor.hpp"

namespace skity {

class HWPathAAOutline : public HWPathVisitor {
 public:
  HWPathAAOutline(const Matrix& matrix, VectorCache<float>* vertex_vector_cache,
                  VectorCache<uint32_t>* index_vector_cache,
                  float context_scale);

  ~HWPathAAOutline() override = default;

  void StrokeAAOutline(const Path& path);

 protected:
  void OnBeginPath() override;
  void OnEndPath() override;
  void OnMoveTo(Vec2 const& p) override;

  void OnLineTo(Vec2 const& p1, Vec2 const& p2) override;

  void OnClose() override;

  void OnQuadTo(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3) override {}

  void OnConicTo(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3,
                 float weight) override {}

  void OnCubicTo(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3,
                 Vec2 const& p4) override {}

 private:
  void AssembleEdgeAAPrimitive();

 private:
  float fringe_ = .5f;
  float aa_normal_dir_ = 1.f;
  bool is_convex_ = true;
  std::vector<Vec2> outline_pts_ = {};
};

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_PATH_AA_OUTLINE_HPP
