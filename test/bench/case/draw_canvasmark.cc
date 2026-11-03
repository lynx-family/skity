// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Paths and Suits adapted from CanvasMark's MotionMark-derived generators.
// MotionMark 2faa1512e8696cf113548534de01a7ce4c4700ab, Apple 2015-2024.
// CanvasMark paths.h, suits.h, suit_shapes.h, extra_scenes.h (scenes /3).
// Fixed seed 42; Paths load 4096 / frame 120, Suits load 2048 / frame 599.
// Frame indices are zero based.
// Simulation and numeric inputs are prepared once (canvasmark_scene.hpp,
// shared with the C-API twin case); OnDraw rebuilds native paths. See
// canvasmark_LICENSE.txt for the retained redistribution terms.

#include "test/bench/case/draw_canvasmark.hpp"

#include <memory>
#include <skity/skity.hpp>
#include <vector>

#include "test/bench/case/canvasmark_scene.hpp"

namespace skity {
namespace cm = canvasmark;
namespace {

class CanvasMarkBenchmark : public Benchmark {
 public:
  enum class Scene { kPaths, kSuits };

  explicit CanvasMarkBenchmark(Scene scene)
      : scene_(scene),
        paths_(scene == Scene::kSuits ? cm::MakeSuits() : cm::MakePaths()) {}
  Size GetSize() override { return {cm::width, cm::height}; }
  std::string GetName() override {
    return scene_ == Scene::kSuits ? "CanvasMark_Suits_2048_Seed42_Frame599"
                                   : "CanvasMark_Paths_4096_Seed42_Frame120";
  }

 private:
  void OnDraw(Canvas* canvas, int) override {
    canvas->Clear(0xfff1f1f1);
    for (const auto& item : paths_) {
      Paint paint;
      paint.SetAntiAlias(true);
      paint.SetColor(item.color);
      paint.SetStyle(item.stroke ? Paint::kStroke_Style : Paint::kFill_Style);
      paint.SetStrokeWidth(item.width);
      paint.SetStrokeCap(Paint::kButt_Cap);
      paint.SetStrokeJoin(Paint::kBevel_Join);
      if (scene_ == Scene::kSuits) {
        canvas->Save();
        canvas->Translate(item.tx, item.ty);
        canvas->Rotate(item.rotation);
        canvas->Scale(item.scale, item.scale);
        canvas->Translate(-.5f, -.5f);
        Point points[] = {{item.gradient_line[0], item.gradient_line[1], 0, 1},
                          {item.gradient_line[2], item.gradient_line[3], 0, 1}};
        Vec4 colors[] = {Color4fFromColor(item.colors[0]),
                         Color4fFromColor(item.colors[1])};
        float stops[] = {0, 1};
        paint.SetShader(
            Shader::MakeLinear(points, colors, stops, 2, TileMode::kClamp));
      }
      if (item.stroke && item.commands.size() == 2 &&
          item.commands[1].verb == 'L') {
        auto a = item.commands[0], b = item.commands[1];
        canvas->DrawLine(a.x, a.y, b.x, b.y, paint);
      } else {
        Path path;
        for (const auto& p : item.commands) {
          switch (p.verb) {
            case 'M':
              path.MoveTo(p.x, p.y);
              break;
            case 'L':
              path.LineTo(p.x, p.y);
              break;
            case 'Q':
              path.QuadTo(p.cx, p.cy, p.x, p.y);
              break;
            case 'C':
              path.CubicTo(p.cx, p.cy, p.dx, p.dy, p.x, p.y);
              break;
            case 'Z':
              path.Close();
              break;
          }
        }
        if (item.clip) {
          canvas->ClipPath(path);
          canvas->DrawRect(Rect::MakeXYWH(0, 0, 1, 1), paint);
        } else
          canvas->DrawPath(path, paint);
      }
      if (scene_ == Scene::kSuits) canvas->Restore();
    }
  }
  Scene scene_;
  std::vector<cm::PaintedPath> paths_;
};
}  // namespace
std::shared_ptr<Benchmark> MakeCanvasMarkPathsBenchmark() {
  return std::make_shared<CanvasMarkBenchmark>(
      CanvasMarkBenchmark::Scene::kPaths);
}
std::shared_ptr<Benchmark> MakeCanvasMarkSuitsBenchmark() {
  return std::make_shared<CanvasMarkBenchmark>(
      CanvasMarkBenchmark::Scene::kSuits);
}
}  // namespace skity
