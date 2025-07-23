// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/graphic/pathop/path_op_engine.hpp"

#include "src/graphic/path_visitor.hpp"
#include "src/graphic/pathop/clipper2/core.h"
#include "src/graphic/pathop/clipper2/engine.h"

namespace skity {

class Clipper2PathConvert : public PathVisitor {
 public:
  Clipper2PathConvert() : PathVisitor(true, Matrix{}) {}
  ~Clipper2PathConvert() override = default;

  void ConvertPath(Path const &path) { VisitPath(path, true); }

  const Clipper2Lib::PathsD &GetPaths() const { return paths_; }

 protected:
  void OnBeginPath() override {}

  void OnEndPath() override {}

  void OnMoveTo(Vec2 const &p) override {
    if (!paths_.empty()) {
      auto &path = paths_.back();

      if (path.size() > 1 && path.front() != path.back()) {
        path.push_back(path.front());
      }
    }

    // begin a new contour
    paths_.push_back({});

    paths_.back().push_back(Clipper2Lib::PointD{p.x, p.y});
  }

  void OnLineTo(Vec2 const &p1, Vec2 const &p2) override {
    if (paths_.empty()) {
      // If no move to in a new contour, we add a move to from (0, 0)
      paths_.push_back({});

      paths_.back().push_back(Clipper2Lib::PointD{0, 0});
    }

    paths_.back().push_back(Clipper2Lib::PointD{p2.x, p2.y});
  }

  void OnQuadTo(Vec2 const &p1, Vec2 const &p2, Vec2 const &p3) override {}

  void OnConicTo(Vec2 const &p1, Vec2 const &p2, Vec2 const &p3,
                 float weight) override {}

  void OnCubicTo(Vec2 const &p1, Vec2 const &p2, Vec2 const &p3,
                 Vec2 const &p4) override {}

  void OnClose() override {
    if (paths_.empty()) {
      return;
    }

    auto &path = paths_.back();

    if (path.size() > 1 && path.front() != path.back()) {
      path.push_back(path.front());
    }
  }

 private:
  Clipper2Lib::PathsD paths_ = {};
};

bool PathOpEngine::Union(const Path &one, const Path &two, Path *result) {
  if (one.IsEmpty() || two.IsEmpty() || result == nullptr) {
    return false;
  }

  op_type_ = PathOp::Op::kUnion;

  return ExecuteInternal(one, two, result);
}

bool PathOpEngine::Intersect(const Path &one, const Path &two, Path *result) {
  if (one.IsEmpty() || two.IsEmpty() || result == nullptr) {
    return false;
  }

  op_type_ = PathOp::Op::kIntersect;

  return ExecuteInternal(one, two, result);
}

bool PathOpEngine::Xor(const Path &one, const Path &two, Path *result) {
  if (one.IsEmpty() || two.IsEmpty() || result == nullptr) {
    return false;
  }

  op_type_ = PathOp::Op::kXor;

  return ExecuteInternal(one, two, result);
}

bool PathOpEngine::Difference(const Path &one, const Path &two, Path *result) {
  if (one.IsEmpty() || two.IsEmpty() || result == nullptr) {
    return false;
  }

  op_type_ = PathOp::Op::kDifference;

  return ExecuteInternal(one, two, result);
}

Path ConvertClipper2Path(const Clipper2Lib::PathsD &paths);

Clipper2Lib::PathsD ConvertPath(const Path &path);

Clipper2Lib::PathsD ConvertPathToNoZeroRule(const Path &path);

Clipper2Lib::FillRule FillTypeToClipper2(Path::PathFillType fill_type) {
  switch (fill_type) {
    case Path::PathFillType::kWinding:
      return Clipper2Lib::FillRule::NonZero;
    case Path::PathFillType::kEvenOdd:
      return Clipper2Lib::FillRule::EvenOdd;
  }
}

Clipper2Lib::ClipType PathOpToClipper2(PathOp::Op op) {
  switch (op) {
    case PathOp::Op::kIntersect:
      return Clipper2Lib::ClipType::Intersection;
    case PathOp::Op::kUnion:
      return Clipper2Lib::ClipType::Union;
    case PathOp::Op::kDifference:
      return Clipper2Lib::ClipType::Difference;
    case PathOp::Op::kXor:
      return Clipper2Lib::ClipType::Xor;
  }
}

bool PathOpEngine::ExecuteInternal(Path const &one, Path const &two,
                                   Path *result) {
  Clipper2Lib::FillRule rule = Clipper2Lib::FillRule::NonZero;

  Clipper2Lib::ClipperD clipper;
  if (one.GetFillType() == two.GetFillType()) {
    // same fill type
    rule = FillTypeToClipper2(one.GetFillType());

    clipper.AddSubject(ConvertPath(one));
    clipper.AddClip(ConvertPath(two));
  } else {
    rule = Clipper2Lib::FillRule::NonZero;

    clipper.AddSubject(ConvertPathToNoZeroRule(one));
    clipper.AddClip(ConvertPathToNoZeroRule(two));
  }

  Clipper2Lib::PathsD solution;

  auto clip_type = PathOpToClipper2(op_type_);

  auto success = clipper.Execute(clip_type, rule, solution);

  if (!success) {
    return false;
  }

  *result = ConvertClipper2Path(solution);

  return true;
}

Path ConvertClipper2Path(const Clipper2Lib::PathsD &paths) {
  Path result;

  for (auto const &path : paths) {
    result.MoveTo(path[0].x, path[0].y);

    for (size_t i = 1; i < path.size() - 1; i++) {
      result.LineTo(path[i].x, path[i].y);
    }

    if (path.back() != path.front()) {
      result.LineTo(path.back().x, path.back().y);
    }

    result.Close();
  }

  return result;
}

Clipper2Lib::PathsD ConvertPath(const Path &path) {
  Clipper2PathConvert convert;

  convert.ConvertPath(path);

  return convert.GetPaths();
}

Clipper2Lib::PathsD ConvertPathToNoZeroRule(const Path &path) {
  auto result = ConvertPath(path);

  if (path.GetFillType() == Path::PathFillType::kEvenOdd) {
    Clipper2Lib::ClipperD clip{};

    Clipper2Lib::ClipperD clipper{};
    clipper.AddSubject(result);

    result.clear();

    clipper.Execute(Clipper2Lib::ClipType::Union,
                    Clipper2Lib::FillRule::EvenOdd, result);
  }

  return result;
}

}  // namespace skity
