// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_PATH_RASTER_HPP
#define SRC_RENDER_HW_HW_PATH_RASTER_HPP

#include <array>
#include <optional>
#include <vector>

#include "src/geometry/wangs_formula.hpp"
#include "src/render/hw/hw_path_visitor.hpp"

namespace skity {

enum class Orientation;

class HWPathFillRaster : public HWPathVisitor {
 public:
  HWPathFillRaster(const Paint& paint, const Matrix& matrix,
                   VectorCache<float>* vertex_vector_cache,
                   VectorCache<uint32_t>* index_vector_cache)
      : HWPathVisitor(paint, true, matrix, vertex_vector_cache,
                      index_vector_cache) {}

  ~HWPathFillRaster() override = default;

  void FillPath(const Path& path) { VisitPath(path, true); }

 protected:
  void OnBeginPath() override {}
  void OnEndPath() override {}
  void OnMoveTo(Vec2 const& p) override;

  void OnLineTo(Vec2 const& p1, Vec2 const& p2) override;

  void OnQuadTo(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3) override {}

  void OnConicTo(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3,
                 float weight) override {}

  void OnCubicTo(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3,
                 Vec2 const& p4) override {}

  void OnClose() override {}

 private:
  Vec2 first_pt_ = {};
  uint32_t first_pt_index_ = {};
};

class HWPathStrokeRaster : public HWPathVisitor {
  struct StrokePoint {
    Vec2 xy;
    bool physical = true;
    bool closed = false;
  };

 public:
  HWPathStrokeRaster(Paint const& paint, const Matrix& matrix,
                     VectorCache<float>* vertex_vector_cache,
                     VectorCache<uint32_t>* index_vector_cache)
      : HWPathVisitor(paint, false, matrix, vertex_vector_cache,
                      index_vector_cache),
        stroke_radius_(std::max(0.5f, paint.GetStrokeWidth()) * 0.5f),
        stroke_miter_(paint.GetStrokeMiter()),
        cap_(paint.GetStrokeCap()),
        join_(paint.GetStrokeJoin()),
        xform_(matrix) {}

  ~HWPathStrokeRaster() override = default;

  void StrokePath(Path const& path) { VisitPath(path, false); }

 protected:
  void OnBeginPath() override {}
  void OnEndPath() override;

  void OnMoveTo(Vec2 const& p) override;

  void OnLineTo(Vec2 const& p1, Vec2 const& p2) override;

  void OnQuadTo(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3) override;

  void OnConicTo(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3,
                 float weight) override;

  void OnCubicTo(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3,
                 Vec2 const& p4) override;

  void OnClose() override;

 private:
  void HandleJoinAndCap();
  void HandleLineJoin();
  void HandleLineCap();

  std::array<Vec2, 4> ExpandLine(const Vec2& p1, const Vec2& p2) const;

  void GenRoundCap(const Vec2& center, const Vec2& out_dir);

  void GenSquareCap(const Vec2& center, const Vec2& out_dir);

  void GenMiterJoin(const Vec2& center, const Vec2& p1, const Vec2& p2);

  void GenBevelJoin(const Vec2& center, const Vec2& p1, const Vec2& p2);

  void GenRoundJoin(const Vec2& center, const Vec2& p1, const Vec2& p2);

  void GenerateCircleMesh(Vec2 const& center, Vec2 const& p1, Vec2 const& p2,
                          int num);
  void GenerateSquareMesh(Vec2 const& p1, Vec2 const& p2, Vec2 const& out_dir);

 private:
  float stroke_radius_ = 0.25f;
  float stroke_miter_ = 4.f;
  bool only_has_move_to_ = true;

  Paint::Cap cap_ = Paint::Cap::kDefault_Cap;
  Paint::Join join_ = Paint::Join::kDefault_Join;
  wangs_formula::VectorXform xform_ = {};

  std::vector<StrokePoint> stroke_pts_ = {};
};

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_PATH_RASTER_HPP
