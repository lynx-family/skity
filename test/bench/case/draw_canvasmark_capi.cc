// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// C-API + RAII-wrapper twin of the CanvasMark benchmarks. This translation
// unit compiles against the wrapper header tree (module/capi/wrapper) instead
// of the legacy C++ headers: every Paint / Path / Shader / Canvas operation
// below crosses the C ABI exactly the way a unified-lib consumer would. The
// scene data comes from the shared API-neutral canvasmark_scene.hpp, and the
// borrowed canvas is bridged from the harness's legacy Canvas*, so the only
// difference from draw_canvasmark.cc is the API layer under the draw loop.

#include "test/bench/case/draw_canvasmark_capi.h"

#include <skity/hpp/bridge.hpp>  // skity_canvas_from_native
#include <skity/skity.hpp>       // RAII wrapper umbrella (wrapper tree)
#include <vector>

#include "test/bench/case/canvasmark_scene.hpp"

namespace {

namespace cm = skity::canvasmark;

// Same conversion as the legacy Color4fFromColor (RGBA float components).
skity::Color4f ToColor4f(uint32_t color) {
  return skity::Color4f{static_cast<float>((color >> 16) & 0xff) / 255.f,
                        static_cast<float>((color >> 8) & 0xff) / 255.f,
                        static_cast<float>(color & 0xff) / 255.f,
                        static_cast<float>((color >> 24) & 0xff) / 255.f};
}

// Scene data, materialized by the Prepare* entries before timing starts.
std::vector<cm::PaintedPath> g_paths;
std::vector<cm::PaintedPath> g_suits;

void DrawScene(skity::Canvas* canvas, const std::vector<cm::PaintedPath>& items,
               bool suits) {
  canvas->DrawColor(0xfff1f1f1, skity::BlendMode::kSrc);
  for (const auto& item : items) {
    skity::Paint paint;
    paint.SetAntiAlias(true);
    paint.SetColor(item.color);
    paint.SetStyle(item.stroke ? skity::Paint::kStroke_Style
                               : skity::Paint::kFill_Style);
    paint.SetStrokeWidth(item.width);
    paint.SetStrokeCap(skity::Paint::kButt_Cap);
    paint.SetStrokeJoin(skity::Paint::kBevel_Join);
    if (suits) {
      canvas->Save();
      canvas->Translate(item.tx, item.ty);
      canvas->Rotate(item.rotation);
      canvas->Scale(item.scale, item.scale);
      canvas->Translate(-.5f, -.5f);
      skity::Point points[] = {
          {item.gradient_line[0], item.gradient_line[1], 0, 1},
          {item.gradient_line[2], item.gradient_line[3], 0, 1}};
      skity::Color4f colors[] = {ToColor4f(item.colors[0]),
                                 ToColor4f(item.colors[1])};
      float stops[] = {0, 1};
      paint.SetShader(skity::Shader::MakeLinear(points, colors, stops, 2,
                                                skity::TileMode::kClamp));
    }
    if (item.stroke && item.commands.size() == 2 &&
        item.commands[1].verb == 'L') {
      auto a = item.commands[0], b = item.commands[1];
      canvas->DrawLine(a.x, a.y, b.x, b.y, paint);
    } else {
      skity::Path path;
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
        canvas->DrawRect(skity::Rect::MakeXYWH(0, 0, 1, 1), paint);
      } else {
        canvas->DrawPath(path, paint);
      }
    }
    if (suits) canvas->Restore();
  }
}

void Draw(void* native_canvas, bool suits) {
  skity_canvas handle = skity_canvas_from_native(native_canvas);
  if (handle == nullptr) {
    return;
  }
  skity::Canvas canvas{handle};  // non-owning view; the harness owns the canvas
  DrawScene(&canvas, suits ? g_suits : g_paths, suits);
}

// The same loop against the bare C entries — no wrapper objects, manual
// create/destroy — so legacy-vs-RawC isolates the C ABI layer and
// RawC-vs-CAPI isolates the wrapper veneer.
void DrawSceneRaw(skity_canvas canvas,
                  const std::vector<cm::PaintedPath>& items, bool suits) {
  skity_canvas_draw_color(canvas, 0xfff1f1f1, SKITY_BLEND_MODE_SRC);
  for (const auto& item : items) {
    skity_paint paint = skity_paint_create();
    skity_paint_set_anti_alias(paint, 1);
    skity_paint_set_color(paint, item.color);
    skity_paint_set_style(
        paint, item.stroke ? SKITY_PAINT_STYLE_STROKE : SKITY_PAINT_STYLE_FILL);
    skity_paint_set_stroke_width(paint, item.width);
    skity_paint_set_stroke_cap(paint, SKITY_PAINT_CAP_BUTT);
    skity_paint_set_stroke_join(paint, SKITY_PAINT_JOIN_BEVEL);
    if (suits) {
      skity_canvas_save(canvas);
      skity_canvas_translate(canvas, item.tx, item.ty);
      skity_canvas_rotate(canvas, item.rotation);
      skity_canvas_scale(canvas, item.scale, item.scale);
      skity_canvas_translate(canvas, -.5f, -.5f);
      skity_point points[] = {
          {item.gradient_line[0], item.gradient_line[1], 0, 1},
          {item.gradient_line[2], item.gradient_line[3], 0, 1}};
      skity_color4f colors[] = {ToColor4f(item.colors[0]),
                                ToColor4f(item.colors[1])};
      float stops[] = {0, 1};
      // The paint keeps a shared reference; ours can be released right away.
      skity_shader shader = skity_shader_create_linear(
          points, colors, stops, 2, SKITY_TILE_MODE_CLAMP, 0);
      skity_paint_set_shader(paint, shader);
      skity_shader_destroy(shader);
    }
    if (item.stroke && item.commands.size() == 2 &&
        item.commands[1].verb == 'L') {
      auto a = item.commands[0], b = item.commands[1];
      skity_canvas_draw_line(canvas, a.x, a.y, b.x, b.y, paint);
    } else {
      skity_path path = skity_path_create();
      for (const auto& p : item.commands) {
        switch (p.verb) {
          case 'M':
            skity_path_move_to(path, p.x, p.y);
            break;
          case 'L':
            skity_path_line_to(path, p.x, p.y);
            break;
          case 'Q':
            skity_path_quad_to(path, p.cx, p.cy, p.x, p.y);
            break;
          case 'C':
            skity_path_cubic_to(path, p.cx, p.cy, p.dx, p.dy, p.x, p.y);
            break;
          case 'Z':
            skity_path_close(path);
            break;
        }
      }
      if (item.clip) {
        skity_rect rect = {0, 0, 1, 1};
        skity_canvas_clip_path(canvas, path, SKITY_CLIP_OP_INTERSECT);
        skity_canvas_draw_rect(canvas, &rect, paint);
      } else {
        skity_canvas_draw_path(canvas, path, paint);
      }
      skity_path_destroy(path);
    }
    if (suits) skity_canvas_restore(canvas);
    skity_paint_destroy(paint);
  }
}

void DrawRaw(void* native_canvas, bool suits) {
  skity_canvas handle = skity_canvas_from_native(native_canvas);
  if (handle == nullptr) {
    return;
  }
  DrawSceneRaw(handle, suits ? g_suits : g_paths, suits);
}

}  // namespace

void CanvasMarkCAPI_PreparePaths(void) {
  if (g_paths.empty()) g_paths = cm::MakePaths();
}

void CanvasMarkCAPI_PrepareSuits(void) {
  if (g_suits.empty()) g_suits = cm::MakeSuits();
}

void CanvasMarkCAPI_DrawPaths(void* native_canvas) {
  Draw(native_canvas, false);
}

void CanvasMarkCAPI_DrawSuits(void* native_canvas) {
  Draw(native_canvas, true);
}

void CanvasMarkCAPI_DrawPathsRaw(void* native_canvas) {
  DrawRaw(native_canvas, false);
}

void CanvasMarkCAPI_DrawSuitsRaw(void* native_canvas) {
  DrawRaw(native_canvas, true);
}
