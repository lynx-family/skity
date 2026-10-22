// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Paths and Suits adapted from CanvasMark's MotionMark-derived generators.
// MotionMark 2faa1512e8696cf113548534de01a7ce4c4700ab, Apple 2015-2024.
// CanvasMark paths.h, suits.h, suit_shapes.h, extra_scenes.h (scenes /3).
// Fixed seed 42; Paths load 4096 / frame 120, Suits load 2048 / frame 599.
// Frame indices are zero based.
// Simulation and numeric inputs are prepared once; OnDraw rebuilds native
// paths. See canvasmark_LICENSE.txt for the retained redistribution terms.

#include "test/bench/case/draw_canvasmark.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <vector>

namespace skity {
namespace {
constexpr int width = 1600, height = 800;
constexpr int kPathsComplexity = 4096;
constexpr int kSuitsComplexity = 2048;
constexpr uint32_t kSeed = 42;
constexpr double pi = 3.14159265358979323846, tau = 2 * pi;
struct PathCommand {
  char verb;
  float x = 0, y = 0, cx = 0, cy = 0, dx = 0, dy = 0;
};
struct PaintedPath {
  std::vector<PathCommand> commands;
  uint32_t color = 0xff000000;
  float width = 1;
  bool stroke = true;
  float tx = 0, ty = 0, rotation = 0, scale = 1;
  bool clip = false;
  std::array<uint32_t, 2> colors{};
  std::array<float, 4> gradient_line{};
};
struct Random {
  uint32_t state;
  double Next() {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return double(state) / 4294967296.;
  }
  int Integer(int n) { return int(Next() * n); }
};
// Replay a constant load through frame 120. Split flags consume RNG each frame,
// so evaluating only the final frame would produce different path boundaries.
std::vector<PaintedPath> MakePaths() {
  struct Object {
    uint32_t color;
    double width;
    bool split;
    PathCommand command;
  };
  Random random{kSeed};
  std::vector<Object> objects;
  int gx = 40, gy = 20;
  for (int i = 0; i < kPathsComplexity; ++i) {
    int type = random.Integer(4);
    constexpr uint32_t colors[] = {0xff101010, 0xff808080, 0xffc0c0c0,
                                   0xff101010, 0xff808080, 0xffc0c0c0,
                                   0xffe01040};
    Object o{};
    o.color = colors[random.Integer(7)];
    o.width = std::pow(random.Next(), 5) * 20 + 1;
    o.split = random.Next() > .5;
    auto point = [&]() {
      if (i != 0) {
        constexpr int offsets[][2] = {{-4, 0}, {2, 0}, {1, -2}, {1, 2}};
        const auto& d = offsets[random.Integer(4)];
        gx += d[0];
        gy += d[1];
        if (gx < 0 || gx > 80) gx -= 2 * d[0];
        if (gy < 0 || gy > 40) gy -= 2 * d[1];
      }
      return std::array<float, 2>{float((gx + .5) * width / 81),
                                  float((gy + .5) * height / 41)};
    };
    auto a = point(), b = a, c = a;
    if (type >= 2) b = point();
    if (type == 3)
      c = point();
    else
      c = b;
    o.command = {type < 2    ? 'L'
                 : type == 2 ? 'Q'
                             : 'C',
                 c[0],
                 c[1],
                 type >= 2 ? a[0] : 0,
                 type >= 2 ? a[1] : 0,
                 type == 3 ? b[0] : 0,
                 type == 3 ? b[1] : 0};
    objects.push_back(o);
  }
  std::vector<PaintedPath> result;
  for (int frame = 0; frame <= 120; ++frame) {
    result.clear();
    for (int i = 0; i < kPathsComplexity; ++i) {
      auto& o = objects[i];
      auto start = [&]() {
        PaintedPath p;
        p.color = o.color;
        p.width = o.width;
        p.commands.push_back({'M', o.command.x, o.command.y});
        result.push_back(std::move(p));
      };
      if (i == 0)
        start();
      else {
        result.back().commands.push_back(o.command);
        if (o.split) start();
        if (random.Next() > .995) o.split = !o.split;
      }
    }
  }
  return result;
}
inline const std::vector<std::vector<PathCommand>> suit_shapes = {
    {{'M', 0.5f, 0.214f},
     {'C', 0.267f, 0.092f, 0.466f, 0.164f, 0.369f, 0.09f},
     {'C', -0.0f, 0.345f, 0.137f, 0.093f, -0.0f, 0.186f},
     {'C', 0.5f, 0.938f, -0.0f, 0.659f, 0.395f, 0.655f},
     {'C', 1.0f, 0.345f, 0.605f, 0.655f, 1.0f, 0.659f},
     {'C', 0.733f, 0.092f, 1.0f, 0.186f, 0.863f, 0.093f},
     {'C', 0.5f, 0.214f, 0.631f, 0.09f, 0.534f, 0.164f},
     {'Z'}},
    {{'M', 0.5f, 0.018f},
     {'C', 0.718f, 0.235f, 0.62f, 0.018f, 0.718f, 0.115f},
     {'C', 0.618f, 0.418f, 0.718f, 0.31f, 0.679f, 0.377f},
     {'C', 0.749f, 0.381f, 0.657f, 0.393f, 0.703f, 0.382f},
     {'C', 0.967f, 0.588f, 0.869f, 0.381f, 0.967f, 0.468f},
     {'C', 0.749f, 0.806f, 0.967f, 0.709f, 0.869f, 0.806f},
     {'C', 0.542f, 0.677f, 0.664f, 0.805f, 0.578f, 0.756f},
     {'C', 0.708f, 0.982f, 0.538f, 0.799f, 0.605f, 0.918f},
     {'C', 0.292f, 0.982f, 0.593f, 0.941f, 0.407f, 0.941f},
     {'C', 0.459f, 0.676f, 0.397f, 0.917f, 0.461f, 0.799f},
     {'C', 0.251f, 0.806f, 0.422f, 0.756f, 0.337f, 0.804f},
     {'C', 0.033f, 0.588f, 0.131f, 0.806f, 0.033f, 0.709f},
     {'C', 0.251f, 0.381f, 0.033f, 0.468f, 0.131f, 0.381f},
     {'C', 0.382f, 0.418f, 0.298f, 0.38f, 0.342f, 0.395f},
     {'C', 0.282f, 0.235f, 0.319f, 0.378f, 0.284f, 0.309f},
     {'C', 0.5f, 0.018f, 0.282f, 0.115f, 0.38f, 0.018f},
     {'Z'}},
    {{'M', 0.301f, 0.982f},
     {'C', 0.469f, 0.72f, 0.374f, 0.941f, 0.469f, 0.804f},
     {'C', 0.049f, 0.563f, 0.374f, 0.857f, 0.039f, 0.825f},
     {'C', 0.5f, 0.018f, 0.059f, 0.28f, 0.406f, 0.269f},
     {'C', 0.951f, 0.563f, 0.594f, 0.269f, 0.941f, 0.28f},
     {'C', 0.531f, 0.72f, 0.961f, 0.825f, 0.626f, 0.857f},
     {'C', 0.699f, 0.982f, 0.531f, 0.804f, 0.626f, 0.941f},
     {'C', 0.301f, 0.982f, 0.584f, 0.941f, 0.416f, 0.941f}},
    {{'M', 0.495f, 0.0f},
     {'C', 0.128f, 0.5f, 0.424f, 0.153f, 0.199f, 0.439f},
     {'C', 0.495f, 1.0f, 0.199f, 0.561f, 0.424f, 0.847f},
     {'C', 0.862f, 0.5f, 0.566f, 0.847f, 0.791f, 0.561f},
     {'C', 0.495f, 0.0f, 0.791f, 0.439f, 0.566f, 0.153f},
     {'Z'}},
};
struct SuitParticle {
  uint64_t id;
  int shape;
  bool clip;
  double interval, rotation_time = 0, size = 0, x = 0, y = 0, vx = 0, vy = 0;
  double hue = 0, hue2 = 0, gradient_angle = 0;
};
// Owns a shared random stream and the same ordered particle list as the web
// stage. Replay is determined by seed + ordered complexity-per-frame history.
class SuitsSimulation {
 public:
  explicit SuitsSimulation(uint32_t seed) : random_{seed ? seed : 1} {
    emission_spin = Range(0, 3);
    emit_steps = Integer(4, 6);
    color_offset = Integer(0, 359);
  }
  void Advance(int count) {
    assert(count >= 0);
    const double time = frame_ * 1000. / 60;
    if (count < int(particles.size()))
      particles.erase(particles.begin(),
                      particles.begin() + (particles.size() - count));
    while (int(particles.size()) < count) {
      SuitParticle p{};
      p.id = ++created_;
      p.clip = p.id % 2;
      p.interval = Range(1000, 10000);
      p.shape = Integer(0, 3);
      Reset(p, time);
      particles.push_back(p);
    }
    constexpr double dt = (1000. / 60) / 4;
    for (auto& p : particles) {
      p.rotation_time = std::fmod(p.rotation_time + dt, p.interval);
      p.x += p.vx * dt;
      p.y += p.vy * dt;
      p.vy += .03;
      double lo = p.size / 2, rx = width - lo, by = height - lo;
      if (p.x > rx) {
        if (p.vx > 0) p.vx *= -1;
        p.x = rx;

      } else if (p.x < lo) {
        if (p.vx < 0) p.vx *= -1;
        p.x = lo;
      }
      if (p.y > by) {
        Redistribute(p);
        if (std::abs(p.vy) < .7) {
          Reset(p, time);

        } else {
          if (p.vy > 0) p.vy *= -.999;
          p.y = by;
        }

      } else if (p.y < lo) {
        Redistribute(p);
        if (p.vy < 0) p.vy *= -.998;
        p.y = lo;
      }
    }
    ++frame_;
  }
  std::vector<SuitParticle> particles;
  double emission_spin, color_offset;
  int emit_steps;

 private:
  Random random_;
  uint32_t frame_ = 0;
  uint64_t created_ = 0;
  double Range(double a, double b) { return a + random_.Next() * (b - a); }
  int Integer(int a, int b) { return int(std::floor(Range(a, b + 1))); }
  void Reset(SuitParticle& p, double time) {
    p.size = std::round(std::pow(random_.Next(), 4) * 40 + 30);
    int emitter = Integer(0, 2);
    p.x = width * (emitter + 1) / 4.;
    p.y = height * (emitter == 1 ? .25 : .333);
    double speed = Range(.5, 2.5);
    double angle = Integer(0, emit_steps) / double(emit_steps) * tau +
                   time / 1000 * emission_spin + speed;
    p.vx = std::sin(angle) * speed;
    p.vy = std::cos(angle) * speed;
    color_offset = std::fmod(color_offset + .5, 360.);
    p.hue = color_offset;
    p.gradient_angle = Integer(0, 359);
    p.hue2 = std::fmod(color_offset + Integer(50, 100), 360.);
  }
  static void Redistribute(SuitParticle& p) {
    double magnitude = std::sqrt(p.vx * p.vx + p.vy * p.vy);
    p.vx *= 1.5 + .005 * p.size;
    double length = std::sqrt(p.vx * p.vx + p.vy * p.vy);
    p.vx = p.vx / length * magnitude;
    p.vy = p.vy / length * magnitude;
  }
};
// Exact cubic extrema, needed for SVG objectBoundingBox gradient semantics.
inline const std::array<std::array<double, 4>, 4>& SuitBounds() {
  static const auto bounds = [] {
    std::array<std::array<double, 4>, 4> all{};
    for (int i = 0; i < 4; ++i) {
      double minx = 1e9, miny = 1e9, maxx = -1e9, maxy = -1e9, x = 0, y = 0;
      auto point = [&](double a, double b) {
        minx = std::min(minx, a);
        maxx = std::max(maxx, a);
        miny = std::min(miny, b);
        maxy = std::max(maxy, b);
      };
      for (auto c : suit_shapes[i]) {
        if (c.verb == 'M') {
          x = c.x;
          y = c.y;
          point(x, y);
        } else if (c.verb == 'C') {
          auto eval = [&](double t) {
            double u = 1 - t;
            point(u * u * u * x + 3 * u * u * t * c.cx + 3 * u * t * t * c.dx +
                      t * t * t * c.x,
                  u * u * u * y + 3 * u * u * t * c.cy + 3 * u * t * t * c.dy +
                      t * t * t * c.y);
          };
          eval(0);
          eval(1);
          for (auto v : {std::array<double, 4>{x, c.cx, c.dx, c.x},
                         std::array<double, 4>{y, c.cy, c.dy, c.y}}) {
            double a = -v[0] + 3 * v[1] - 3 * v[2] + v[3],
                   b = 2 * (v[0] - 2 * v[1] + v[2]), d = v[1] - v[0];
            if (std::abs(a) < 1e-12) {
              if (std::abs(b) > 1e-12) {
                double t = -d / b;
                if (t > 0 && t < 1) eval(t);
              }
            } else {
              double disc = b * b - 4 * a * d;
              if (disc >= 0)
                for (double t : {(-b + std::sqrt(disc)) / (2 * a),
                                 (-b - std::sqrt(disc)) / (2 * a)})
                  if (t > 0 && t < 1) eval(t);
            }
          }
          x = c.x;
          y = c.y;
        }
      }
      all[i] = {minx, miny, maxx - minx, maxy - miny};
    }
    return all;
  }();
  return bounds;
}
inline uint32_t HSL(double h, double s, double l, double alpha = 1) {
  h = std::fmod(h, 360.) / 60;
  if (h < 0) h += 6;
  double c = (1 - std::abs(2 * l - 1)) * s,
         x = c * (1 - std::abs(std::fmod(h, 2.) - 1)), m = l - c / 2;
  double rgb[6][3] = {{c, x, 0}, {x, c, 0}, {0, c, x},
                      {0, x, c}, {x, 0, c}, {c, 0, x}};
  auto b = [](double v) {
    return uint32_t(std::round(std::clamp(v, 0., 1.) * 255));
  };
  return b(alpha) << 24 | b(rgb[int(h)][0] + m) << 16 |
         b(rgb[int(h)][1] + m) << 8 | b(rgb[int(h)][2] + m);
}
std::vector<PaintedPath> MakeSuits() {
  SuitsSimulation simulation(kSeed);
  // Constant load from frame 0 through 599, including collisions and respawns.
  for (int frame = 0; frame <= 599; ++frame)
    simulation.Advance(kSuitsComplexity);
  std::vector<PaintedPath> result;
  for (const auto& item : simulation.particles) {
    PaintedPath p;
    p.stroke = false;
    p.commands = suit_shapes[item.shape];
    p.tx = item.x;
    p.ty = item.y;
    p.scale = item.size;
    p.rotation = std::floor(360 * item.rotation_time / item.interval);
    p.clip = item.clip;
    p.colors = {HSL(item.hue, .7, .45), HSL(item.hue2, .7, .65)};
    auto b = item.clip ? std::array<double, 4>{0, 0, 1, 1}
                       : SuitBounds()[item.shape];
    double angle = item.gradient_angle * pi / 180;
    double nx = std::cos(angle) / b[2], ny = std::sin(angle) / b[3];
    double norm = nx * nx + ny * ny;
    p.gradient_line = {float(b[0]), float(b[1]), float(b[0] + nx / norm),
                       float(b[1] + ny / norm)};
    result.push_back(std::move(p));
  }
  return result;
}

class CanvasMarkBenchmark : public Benchmark {
 public:
  enum class Scene { kPaths, kSuits };

  explicit CanvasMarkBenchmark(Scene scene)
      : scene_(scene),
        paths_(scene == Scene::kSuits ? MakeSuits() : MakePaths()) {}
  Size GetSize() override { return {width, height}; }
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
  std::vector<PaintedPath> paths_;
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
