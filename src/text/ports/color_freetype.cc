/*
 * Copyright 2006-2012 The Android Open Source Project
 * Copyright 2012 Mozilla Foundation
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/ports/color_freetype.hpp"

#include <freetype/ftsizes.h>

#include <skity/effect/shader.hpp>
#include <unordered_set>

#include "src/base/fixed_types.hpp"
#include "src/geometry/math.hpp"
#include "src/render/auto_canvas.hpp"
#include "src/utils/function_wrapper.hpp"

namespace skity {

namespace {

constexpr float kColorStopShift =
    sizeof(FT_ColorStop::stop_offset) == sizeof(FT_F2Dot14) ? 1 << 14 : 1 << 16;
const uint16_t kForegroundColorPaletteIndex = 0xFFFF;

struct Hash {
  std::size_t operator()(FT_OpaquePaint const& key) const {
    size_t res = 17;
    res = res * 31 + std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(key.p));
    res = res * 31 +
          std::hash<bool>()(static_cast<bool>(key.insert_root_transform));
    return res;
  }
};

struct Equal {
  constexpr bool operator()(const FT_OpaquePaint& lhs,
                            const FT_OpaquePaint& rhs) const {
    return lhs.p == rhs.p &&
           lhs.insert_root_transform == rhs.insert_root_transform;
  }
};

using VisitedSet = std::unordered_set<FT_OpaquePaint, Hash, Equal>;

struct ColorContext {
  ColorFreeType* color_utils;
  PathFreeType* path_utils;
  Canvas* canvas;
  FT_Face face;
  std::vector<Color>* palette;
  Color foreground_color;
  VisitedSet* visited_set;
};

struct BoundsContext {
  ColorFreeType* color_utils;
  PathFreeType* path_utils;
  FT_Face face;
  Matrix* ctm;
  Rect* bounds;
  VisitedSet* visited_set;
};

inline TileMode ToTileMode(FT_PaintExtend extendMode) {
  switch (extendMode) {
    case FT_COLR_PAINT_EXTEND_REPEAT:
      return TileMode::kRepeat;
    case FT_COLR_PAINT_EXTEND_REFLECT:
      return TileMode::kMirror;
    default:
      return TileMode::kClamp;
  }
}

inline BlendMode ToBlendMode(FT_Composite_Mode compositeMode) {
  switch (compositeMode) {
    case FT_COLR_COMPOSITE_CLEAR:
      return BlendMode::kClear;
    case FT_COLR_COMPOSITE_SRC:
      return BlendMode::kSrc;
    case FT_COLR_COMPOSITE_DEST:
      return BlendMode::kDst;
    case FT_COLR_COMPOSITE_SRC_OVER:
      return BlendMode::kSrcOver;
    case FT_COLR_COMPOSITE_DEST_OVER:
      return BlendMode::kDstOver;
    case FT_COLR_COMPOSITE_SRC_IN:
      return BlendMode::kSrcIn;
    case FT_COLR_COMPOSITE_DEST_IN:
      return BlendMode::kDstIn;
    case FT_COLR_COMPOSITE_SRC_OUT:
      return BlendMode::kSrcOut;
    case FT_COLR_COMPOSITE_DEST_OUT:
      return BlendMode::kDstOut;
    case FT_COLR_COMPOSITE_SRC_ATOP:
      return BlendMode::kSrcATop;
    case FT_COLR_COMPOSITE_DEST_ATOP:
      return BlendMode::kDstATop;
    case FT_COLR_COMPOSITE_XOR:
      return BlendMode::kXor;
    case FT_COLR_COMPOSITE_PLUS:
      return BlendMode::kPlus;
    case FT_COLR_COMPOSITE_SCREEN:
      return BlendMode::kScreen;
    // case FT_COLR_COMPOSITE_OVERLAY:
    //   return BlendMode::kOverlay;
    // case FT_COLR_COMPOSITE_DARKEN:
    //   return BlendMode::kDarken;
    // case FT_COLR_COMPOSITE_LIGHTEN:
    //   return BlendMode::kLighten;
    // case FT_COLR_COMPOSITE_COLOR_DODGE:
    //   return BlendMode::kColorDodge;
    // case FT_COLR_COMPOSITE_COLOR_BURN:
    //   return BlendMode::kColorBurn;
    // case FT_COLR_COMPOSITE_HARD_LIGHT:
    //   return BlendMode::kHardLight;
    case FT_COLR_COMPOSITE_SOFT_LIGHT:
      return BlendMode::kSoftLight;
    // case FT_COLR_COMPOSITE_DIFFERENCE:
    //   return BlendMode::kDifference;
    // case FT_COLR_COMPOSITE_EXCLUSION:
    //   return BlendMode::kExclusion;
    // case FT_COLR_COMPOSITE_MULTIPLY:
    //   return BlendMode::kMultiply;
    // case FT_COLR_COMPOSITE_HSL_HUE:
    //   return BlendMode::kHue;
    // case FT_COLR_COMPOSITE_HSL_SATURATION:
    //   return BlendMode::kSaturation;
    // case FT_COLR_COMPOSITE_HSL_COLOR:
    //   return BlendMode::kColor;
    // case FT_COLR_COMPOSITE_HSL_LUMINOSITY:
    //   return BlendMode::kLuminosity;
    default:
      return BlendMode::kDst;
  }
}

inline Vec2 VectorProjection(Vec2 a, Vec2 b) {
  float length = glm::length(b);
  if (!length) {
    return Vec2(0, 0);
  }
  Vec2 b_normalized = glm::normalize(b);
  float scale = glm::dot(a, b) / length;
  b_normalized.x = b_normalized.x * scale;
  b_normalized.y = b_normalized.y * scale;
  return b_normalized;
}

bool start_glyph_bounds(BoundsContext context, GlyphID glyph_id,
                        FT_Color_Root_Transform root_transform);
bool traverse_paint_bounds(BoundsContext context, FT_OpaquePaint opaque_paint);

bool start_glyph(ColorContext context, GlyphID glyph_id,
                 FT_Color_Root_Transform root_transform);
bool traverse_paint(ColorContext context, FT_OpaquePaint opaque_paint);
void transform(const FT_COLR_Paint& colr_paint, Canvas* canvas,
               Matrix* out_transform = nullptr);
bool draw_paint(ColorContext context, const FT_COLR_Paint& colr_paint);
bool draw_glyph_with_path(ColorContext context,
                          const FT_COLR_Paint& glyph_paint,
                          const FT_COLR_Paint& fill_paint);
bool configure_paint(ColorContext context, const FT_COLR_Paint& colr_paint,
                     Paint* paint);
Path clip_box_path(ColorContext context, GlyphID glyph_id, bool untransformed);

bool start_glyph_bounds(BoundsContext context, GlyphID glyph_id,
                        FT_Color_Root_Transform root_transform) {
  FT_OpaquePaint opaque_paint{nullptr, 1};
  if (!FT_Get_Color_Glyph_Paint(context.face, glyph_id, root_transform,
                                &opaque_paint)) {
    return false;
  }
  if (!traverse_paint_bounds(context, opaque_paint)) {
    return false;
  }

  return true;
}

bool traverse_paint_bounds(BoundsContext context, FT_OpaquePaint opaque_paint) {
  if (context.visited_set->find(opaque_paint) != context.visited_set->end()) {
    return true;
  }

  context.visited_set->insert(opaque_paint);
  SK_AT_SCOPE_EXIT(context.visited_set->erase(opaque_paint));

  FT_COLR_Paint paint;
  if (!FT_Get_Paint(context.face, opaque_paint, &paint)) {
    return false;
  }

  Matrix restore_matrix = *context.ctm;
  SK_AT_SCOPE_EXIT(*context.ctm = restore_matrix);

  switch (paint.format) {
    case FT_COLR_PAINTFORMAT_COLR_LAYERS: {
      FT_LayerIterator& layer_iterator = paint.u.colr_layers.layer_iterator;
      FT_OpaquePaint layer_paint{nullptr, 1};
      while (FT_Get_Paint_Layers(context.face, &layer_iterator, &layer_paint)) {
        if (!traverse_paint_bounds(context, layer_paint)) {
          return false;
        }
      }
      return true;
    }
    case FT_COLR_PAINTFORMAT_GLYPH: {
      FT_UInt glyphID = paint.u.glyph.glyphID;
      Path path;
      if (!context.path_utils->GenerateFacePath(context.face, glyphID, &path)) {
        return false;
      }
      path = path.CopyWithMatrix(*context.ctm);
      context.bounds->Join(path.GetBounds());
      return true;
    }
    case FT_COLR_PAINTFORMAT_COLR_GLYPH: {
      GlyphID glyph_id = paint.u.colr_glyph.glyphID;
      return start_glyph_bounds(context, glyph_id, FT_COLOR_NO_ROOT_TRANSFORM);
    }
    case FT_COLR_PAINTFORMAT_TRANSFORM: {
      Matrix transform_matrix;
      transform(paint, nullptr, &transform_matrix);
      context.ctm->PreConcat(transform_matrix);
      FT_OpaquePaint& transform_paint = paint.u.transform.paint;
      return traverse_paint_bounds(context, transform_paint);
    }
    case FT_COLR_PAINTFORMAT_TRANSLATE: {
      Matrix transform_matrix;
      transform(paint, nullptr, &transform_matrix);
      context.ctm->PreConcat(transform_matrix);
      FT_OpaquePaint& translate_paint = paint.u.translate.paint;
      return traverse_paint_bounds(context, translate_paint);
    }
    case FT_COLR_PAINTFORMAT_SCALE: {
      Matrix transform_matrix;
      transform(paint, nullptr, &transform_matrix);
      context.ctm->PreConcat(transform_matrix);
      FT_OpaquePaint& scale_paint = paint.u.scale.paint;
      return traverse_paint_bounds(context, scale_paint);
    }
    case FT_COLR_PAINTFORMAT_ROTATE: {
      Matrix transform_matrix;
      transform(paint, nullptr, &transform_matrix);
      context.ctm->PreConcat(transform_matrix);
      FT_OpaquePaint& rotate_paint = paint.u.rotate.paint;
      return traverse_paint_bounds(context, rotate_paint);
    }
    case FT_COLR_PAINTFORMAT_SKEW: {
      Matrix transform_matrix;
      transform(paint, nullptr, &transform_matrix);
      context.ctm->PreConcat(transform_matrix);
      FT_OpaquePaint& skew_paint = paint.u.skew.paint;
      return traverse_paint_bounds(context, skew_paint);
    }
    case FT_COLR_PAINTFORMAT_COMPOSITE: {
      FT_OpaquePaint& backdrop_paint = paint.u.composite.backdrop_paint;
      FT_OpaquePaint& source_paint = paint.u.composite.source_paint;
      return traverse_paint_bounds(context, backdrop_paint) &&
             traverse_paint_bounds(context, source_paint);
    }
    case FT_COLR_PAINTFORMAT_SOLID:
    case FT_COLR_PAINTFORMAT_LINEAR_GRADIENT:
    case FT_COLR_PAINTFORMAT_RADIAL_GRADIENT:
    case FT_COLR_PAINTFORMAT_SWEEP_GRADIENT: {
      return true;
    }
    case FT_COLR_PAINT_FORMAT_MAX:
    case FT_COLR_PAINTFORMAT_UNSUPPORTED: {
      return false;
    }
  }
}

bool start_glyph(ColorContext context, GlyphID glyph_id,
                 FT_Color_Root_Transform root_transform) {
  FT_OpaquePaint opaque_paint{nullptr, 1};
  if (!FT_Get_Color_Glyph_Paint(context.face, glyph_id, root_transform,
                                &opaque_paint)) {
    return false;
  }

  bool untransformed = root_transform == FT_COLOR_NO_ROOT_TRANSFORM;
  Path path = clip_box_path(context, glyph_id, untransformed);
  if (!path.IsEmpty()) {
    context.canvas->ClipPath(path);
  }

  if (!traverse_paint(context, opaque_paint)) {
    return false;
  }

  return true;
}

bool traverse_paint(ColorContext context, FT_OpaquePaint opaque_paint) {
  if (context.visited_set->find(opaque_paint) != context.visited_set->end()) {
    return true;
  }

  context.visited_set->insert(opaque_paint);
  SK_AT_SCOPE_EXIT(context.visited_set->erase(opaque_paint));

  FT_COLR_Paint paint;
  if (!FT_Get_Paint(context.face, opaque_paint, &paint)) {
    return false;
  }

  AutoCanvasRestore auto_restore(context.canvas, true);
  switch (paint.format) {
    case FT_COLR_PAINTFORMAT_COLR_LAYERS: {
      FT_LayerIterator& layer_iterator = paint.u.colr_layers.layer_iterator;
      FT_OpaquePaint layer_paint{nullptr, 1};
      while (FT_Get_Paint_Layers(context.face, &layer_iterator, &layer_paint)) {
        if (!traverse_paint(context, layer_paint)) {
          return false;
        }
      }
      return true;
    }
    case FT_COLR_PAINTFORMAT_SOLID:
    case FT_COLR_PAINTFORMAT_LINEAR_GRADIENT:
    case FT_COLR_PAINTFORMAT_RADIAL_GRADIENT:
    case FT_COLR_PAINTFORMAT_SWEEP_GRADIENT: {
      return draw_paint(context, paint);
    }
    case FT_COLR_PAINTFORMAT_GLYPH: {
      FT_COLR_Paint fillPaint;
      if (!FT_Get_Paint(context.face, paint.u.glyph.paint, &fillPaint)) {
        return false;
      }
      if (fillPaint.format == FT_COLR_PAINTFORMAT_SOLID ||
          fillPaint.format == FT_COLR_PAINTFORMAT_LINEAR_GRADIENT ||
          fillPaint.format == FT_COLR_PAINTFORMAT_RADIAL_GRADIENT ||
          fillPaint.format == FT_COLR_PAINTFORMAT_SWEEP_GRADIENT) {
        return draw_glyph_with_path(context, paint, fillPaint);
      }
      if (!draw_paint(context, paint)) {
        return false;
      }
      return traverse_paint(context, paint.u.glyph.paint);
    }
    case FT_COLR_PAINTFORMAT_COLR_GLYPH: {
      return start_glyph(context, paint.u.colr_glyph.glyphID,
                         FT_COLOR_NO_ROOT_TRANSFORM);
    }
    case FT_COLR_PAINTFORMAT_TRANSFORM: {
      transform(paint, context.canvas);
      return traverse_paint(context, paint.u.transform.paint);
    }
    case FT_COLR_PAINTFORMAT_TRANSLATE: {
      transform(paint, context.canvas);
      return traverse_paint(context, paint.u.translate.paint);
    }
    case FT_COLR_PAINTFORMAT_SCALE: {
      transform(paint, context.canvas);
      return traverse_paint(context, paint.u.scale.paint);
    }
    case FT_COLR_PAINTFORMAT_ROTATE: {
      transform(paint, context.canvas);
      return traverse_paint(context, paint.u.rotate.paint);
    }
    case FT_COLR_PAINTFORMAT_SKEW: {
      transform(paint, context.canvas);
      return traverse_paint(context, paint.u.skew.paint);
    }
    case FT_COLR_PAINTFORMAT_COMPOSITE: {
      AutoCanvasRestore acr(context.canvas, false);
      context.canvas->SaveLayer(context.canvas->GetLocalClipBounds(), Paint{});
      if (!traverse_paint(context, paint.u.composite.backdrop_paint)) {
        return false;
      }
      Paint blend_paint;
      blend_paint.SetBlendMode(ToBlendMode(paint.u.composite.composite_mode));
      context.canvas->SaveLayer(context.canvas->GetLocalClipBounds(),
                                blend_paint);
      return traverse_paint(context, paint.u.composite.source_paint);
    }
    case FT_COLR_PAINT_FORMAT_MAX:
    case FT_COLR_PAINTFORMAT_UNSUPPORTED: {
      return false;
    }
  }
}

void transform(const FT_COLR_Paint& colr_paint, Canvas* canvas,
               Matrix* out_transform) {
  Matrix transform;

  switch (colr_paint.format) {
    case FT_COLR_PAINTFORMAT_TRANSFORM: {
      transform.Set(Matrix::kMScaleX,
                    FixedDot16ToFloat(colr_paint.u.transform.affine.xx));
      transform.Set(Matrix::kMSkewX,
                    -FixedDot16ToFloat(colr_paint.u.transform.affine.xy));
      transform.Set(Matrix::kMScaleY,
                    FixedDot16ToFloat(colr_paint.u.transform.affine.yy));
      transform.Set(Matrix::kMSkewY,
                    -FixedDot16ToFloat(colr_paint.u.transform.affine.yx));
      transform.Set(Matrix::kMTransX,
                    FixedDot16ToFloat(colr_paint.u.transform.affine.dx));
      transform.Set(Matrix::kMTransY,
                    -FixedDot16ToFloat(colr_paint.u.transform.affine.dy));
      break;
    }
    case FT_COLR_PAINTFORMAT_TRANSLATE: {
      transform =
          Matrix::Translate(FixedDot16ToFloat(colr_paint.u.translate.dx),
                            -FixedDot16ToFloat(colr_paint.u.translate.dy));
      break;
    }
    case FT_COLR_PAINTFORMAT_SCALE: {
      float scale_x = FixedDot16ToFloat(colr_paint.u.scale.scale_x);
      float scale_y = FixedDot16ToFloat(colr_paint.u.scale.scale_y);
      float center_x = FixedDot16ToFloat(colr_paint.u.scale.center_x);
      float center_y = -FixedDot16ToFloat(colr_paint.u.scale.center_y);
      float translate_x = center_x - scale_x * center_x;
      float translate_y = center_y - scale_y * center_y;
      transform.Set(Matrix::kMScaleX, scale_x);
      transform.Set(Matrix::kMScaleY, scale_y);
      transform.Set(Matrix::kMTransX, translate_x);
      transform.Set(Matrix::kMTransY, translate_y);
      break;
    }
    case FT_COLR_PAINTFORMAT_ROTATE: {
      transform = Matrix::RotateDeg(
          -FixedDot16ToFloat(colr_paint.u.rotate.angle) * 180.0f,
          Vec2(FixedDot16ToFloat(colr_paint.u.rotate.center_x),
               -FixedDot16ToFloat(colr_paint.u.rotate.center_y)));
      break;
    }
    case FT_COLR_PAINTFORMAT_SKEW: {
      // In the PAINTFORMAT_ROTATE implementation, Matrix setRotate
      // snaps to 0 for values very close to 0. Do the same here.
      float deg_x = FixedDot16ToFloat(colr_paint.u.skew.x_skew_angle) * 180.0f;
      float rad_x = glm::radians(deg_x);
      float tan_x = FloatTanSnapToZero(rad_x);

      float deg_y = FixedDot16ToFloat(colr_paint.u.skew.y_skew_angle) * 180.0f;
      // Negate y_skew_angle due to y-down coordinate system to achieve
      // counter-clockwise skew along the y-axis.
      float rad_y = glm::radians(-deg_y);
      float tan_y = FloatTanSnapToZero(rad_y);

      float center_x = FixedDot16ToFloat(colr_paint.u.skew.center_x);
      float center_y = -FixedDot16ToFloat(colr_paint.u.skew.center_y);
      float translate_x = -tan_x * center_x;
      float translate_y = -tan_y * center_y;
      transform.Set(Matrix::kMSkewX, tan_x);
      transform.Set(Matrix::kMSkewY, tan_y);
      transform.Set(Matrix::kMTransX, translate_x);
      transform.Set(Matrix::kMTransY, translate_y);
      break;
    }
    default: {
    }
  }

  if (canvas) {
    canvas->Concat(transform);
  }
  if (out_transform) {
    *out_transform = transform;
  }
}

bool draw_paint(ColorContext context, const FT_COLR_Paint& colr_paint) {
  switch (colr_paint.format) {
    case FT_COLR_PAINTFORMAT_GLYPH: {
      FT_UInt glyphID = colr_paint.u.glyph.glyphID;
      Path path;
      if (!context.path_utils->GenerateFacePath(context.face, glyphID, &path)) {
        return false;
      }

      context.canvas->ClipPath(path);
      return true;
    }
    case FT_COLR_PAINTFORMAT_SOLID:
    case FT_COLR_PAINTFORMAT_LINEAR_GRADIENT:
    case FT_COLR_PAINTFORMAT_RADIAL_GRADIENT:
    case FT_COLR_PAINTFORMAT_SWEEP_GRADIENT: {
      Paint paint;
      if (!configure_paint(context, colr_paint, &paint)) {
        return false;
      }
      context.canvas->DrawPaint(paint);
      return true;
    }
    default:
      return false;
  }
}

bool draw_glyph_with_path(ColorContext context,
                          const FT_COLR_Paint& glyph_paint,
                          const FT_COLR_Paint& fill_paint) {
  Paint paint;
  paint.SetAntiAlias(true);
  if (!configure_paint(context, fill_paint, &paint)) {
    return false;
  }

  FT_UInt glyphID = glyph_paint.u.glyph.glyphID;
  Path path;
  if (!context.path_utils->GenerateFacePath(context.face, glyphID, &path)) {
    return false;
  }
  context.canvas->DrawPath(path, paint);

  return true;
}

bool fetch_gradient_colors(ColorContext context,
                           const FT_ColorStopIterator& color_stop_iterator,
                           std::vector<Color4f>& colors,
                           std::vector<float>& stops) {
  const FT_UInt num_color_stops = color_stop_iterator.num_color_stops;
  if (num_color_stops == 0) {
    return false;
  }

  struct ColorStop {
    float pos;
    Color4f color;
  };
  std::vector<ColorStop> color_stops_sorted;
  color_stops_sorted.resize(num_color_stops);
  FT_ColorStop ft_stop;
  FT_ColorStopIterator mutable_color_stop_iterator = color_stop_iterator;
  while (FT_Get_Colorline_Stops(context.face, &ft_stop,
                                &mutable_color_stop_iterator)) {
    FT_UInt index = mutable_color_stop_iterator.current_color_stop - 1;
    ColorStop& skStop = color_stops_sorted[index];
    skStop.pos = ft_stop.stop_offset / kColorStopShift;
    FT_UInt16& palette_index = ft_stop.color.palette_index;
    if (palette_index == kForegroundColorPaletteIndex) {
      skStop.color = Color4fFromColor(context.foreground_color);
    } else if (palette_index >= context.palette->size()) {
      return false;
    } else {
      skStop.color = Color4fFromColor(context.palette->at(palette_index));
    }
    skStop.color.a *= FixedDot14ToFloat(ft_stop.color.alpha);
  }

  std::stable_sort(
      color_stops_sorted.begin(), color_stops_sorted.end(),
      [](const ColorStop& a, const ColorStop& b) { return a.pos < b.pos; });

  stops.resize(num_color_stops);
  colors.resize(num_color_stops);
  for (size_t i = 0; i < num_color_stops; ++i) {
    stops[i] = color_stops_sorted[i].pos;
    colors[i] = color_stops_sorted[i].color;
  }

  return true;
}

bool configure_paint(ColorContext context, const FT_COLR_Paint& colr_paint,
                     Paint* paint) {
  switch (colr_paint.format) {
    case FT_COLR_PAINTFORMAT_SOLID: {
      FT_PaintSolid solid = colr_paint.u.solid;
      Color4f color = Colors::kTransparent;
      if (solid.color.palette_index == kForegroundColorPaletteIndex) {
        color = Color4fFromColor(context.foreground_color);
      } else if (solid.color.palette_index >= context.palette->size()) {
        return false;
      } else {
        color =
            Color4fFromColor(context.palette->at(solid.color.palette_index));
      }
      color.a *= FixedDot14ToFloat(solid.color.alpha);
      paint->SetShader(nullptr);
      paint->SetColor(Color4fToColor(color));
      return true;
    }
    case FT_COLR_PAINTFORMAT_LINEAR_GRADIENT: {
      const FT_PaintLinearGradient& linear_gradient =
          colr_paint.u.linear_gradient;
      std::vector<Color4f> colors;
      std::vector<float> positions;

      if (!fetch_gradient_colors(context,
                                 linear_gradient.colorline.color_stop_iterator,
                                 colors, positions)) {
        return false;
      }

      if (positions.size() == 1) {
        paint->SetColor(Color4fToColor(colors[0]));
        return true;
      }

      TileMode tile_mode = ToTileMode(linear_gradient.colorline.extend);
      Vec2 p0{FixedDot16ToFloat(linear_gradient.p0.x),
              -FixedDot16ToFloat(linear_gradient.p0.y)};
      Vec2 p1{FixedDot16ToFloat(linear_gradient.p1.x),
              -FixedDot16ToFloat(linear_gradient.p1.y)};
      Vec2 p2{FixedDot16ToFloat(linear_gradient.p2.x),
              -FixedDot16ToFloat(linear_gradient.p2.y)};

      if (p1 == p0 || p2 == p0 || !CrossProduct(p1 - p0, p2 - p0)) {
        paint->SetColor(Color4fToColor(colors[0]));
        return true;
      }

      Vec2 p0p2 = p2 - p0;
      Vec2 perpendicular_to_p0p2 = Vec2(p0p2.y, -p0p2.x);
      Vec2 p3 = p0 + VectorProjection((p1 - p0), perpendicular_to_p0p2);

      float pos_range = positions.back() - positions.front();
      if ((pos_range != 1 || positions.front() != 0.f)) {
        Vec2 start_to_end = p3 - p0;
        float scale_factor = 1 / pos_range;
        float pos_start_offset = positions.front();

        Vec2 start_offset = start_to_end;
        start_offset.x *= positions.front();
        start_offset.y *= positions.front();
        Vec2 end_offset = start_to_end;
        end_offset.x *= positions.back();
        end_offset.y *= positions.back();

        p0 = p0 + start_offset;
        p1 = p0 + end_offset;

        for (auto& pos : positions) {
          pos = (pos - pos_start_offset) * scale_factor;
        }
      }

      Point line_points[2] = {Point(p0.x, p0.y, 0, 1), Point(p3.x, p3.y, 0, 1)};
      auto linear_shader = skity::Shader::MakeLinear(
          line_points, colors.data(), positions.data(), positions.size(),
          tile_mode);

      paint->SetShader(std::move(linear_shader));
      return true;
    }
    case FT_COLR_PAINTFORMAT_RADIAL_GRADIENT: {
      const FT_PaintRadialGradient& radial_gradient =
          colr_paint.u.radial_gradient;
      Point start{FixedDot16ToFloat(radial_gradient.c0.x),
                  -FixedDot16ToFloat(radial_gradient.c0.y), 0, 1};
      float start_radius = FixedDot16ToFloat(radial_gradient.r0);
      Point end{FixedDot16ToFloat(radial_gradient.c1.x),
                -FixedDot16ToFloat(radial_gradient.c1.y), 0, 1};
      float end_radius = FixedDot16ToFloat(radial_gradient.r1);

      std::vector<Color4f> colors;
      std::vector<float> positions;
      if (!fetch_gradient_colors(context,
                                 radial_gradient.colorline.color_stop_iterator,
                                 colors, positions)) {
        return false;
      }

      if (positions.size() == 1) {
        paint->SetColor(Color4fToColor(colors[0]));
        return true;
      }

      float pos_range = positions.back() - positions.front();
      if (pos_range != 1.f || positions.front() != 0.f) {
        Vec2 start_to_end = end - start;
        float scale_factor = 1 / pos_range;
        float pos_start_offset = positions.front();

        Vec2 start_offset = start_to_end;
        start_offset.x *= positions.front();
        start_offset.y *= positions.front();
        Vec2 end_offset = start_to_end;
        end_offset.x *= positions.back();
        end_offset.y *= positions.back();

        start.x += start_offset.x;
        start.y += start_offset.y;
        end.x = start.x + end_offset.x;
        end.y = start.y + end_offset.y;

        float radius_diff = end_radius - start_radius;
        start_radius = start_radius + positions.front() * radius_diff;
        end_radius = start_radius + positions.back() * radius_diff;

        for (auto& pos : positions) {
          pos = (pos - pos_start_offset) * scale_factor;
        }
      }

      TileMode tile_mode = ToTileMode(radial_gradient.colorline.extend);
      auto radial_shader = skity::Shader::MakeTwoPointConical(
          start, start_radius, end, end_radius, colors.data(), positions.data(),
          positions.size(), tile_mode);
      paint->SetShader(radial_shader);
      return true;
    }
    case FT_COLR_PAINTFORMAT_SWEEP_GRADIENT: {
      // TODO(jingle) Add support for sweep gradient
      paint->SetColor(Color4fToColor(Colors::kBlack));
      return true;
    }
    default: {
      return false;
    }
  }
}

Path clip_box_path(ColorContext context, GlyphID glyph_id, bool untransformed) {
  FT_Face face = context.face;
  Path result;
  using DoneFTSize = FunctionWrapper<decltype(FT_Done_Size), FT_Done_Size>;

  FT_Size old_size = face->size;
  FT_Matrix old_transform;
  FT_Vector old_delta;

  if (untransformed) {
    std::unique_ptr<std::remove_pointer_t<FT_Size>, DoneFTSize> ft_size(
        [face]() -> FT_Size {
          FT_Size size;
          FT_Error err = FT_New_Size(face, &size);
          if (err != 0) {
            return nullptr;
          }
          return size;
        }());

    if (nullptr == ft_size) {
      return result;
    }

    if (FT_Activate_Size(ft_size.get())) {
      return result;
    }
    if (FT_Set_Char_Size(face, IntToFixedDot6(face->units_per_EM), 0, 0, 0)) {
      return result;
    }

    FT_Get_Transform(face, &old_transform, &old_delta);
    FT_Set_Transform(face, nullptr, nullptr);
  }

  FT_ClipBox ft_clip_box;
  if (FT_Get_Color_Glyph_ClipBox(face, glyph_id, &ft_clip_box)) {
    result.MoveTo(FixedDot6ToFloat(ft_clip_box.bottom_left.x),
                  -FixedDot6ToFloat(ft_clip_box.bottom_left.y));
    result.LineTo(FixedDot6ToFloat(ft_clip_box.top_left.x),
                  -FixedDot6ToFloat(ft_clip_box.top_left.y));
    result.LineTo(FixedDot6ToFloat(ft_clip_box.top_right.x),
                  -FixedDot6ToFloat(ft_clip_box.top_right.y));
    result.LineTo(FixedDot6ToFloat(ft_clip_box.bottom_right.x),
                  -FixedDot6ToFloat(ft_clip_box.bottom_right.y));
    result.Close();
  }

  if (untransformed) {
    if (FT_Activate_Size(old_size)) {
      return result;
    }
    FT_Set_Transform(face, &old_transform, &old_delta);
  }

  return result;
}

}  // namespace

bool ColorFreeType::DrawColorV1Glyph(FT_Face face, const GlyphData& glyph) {
  PreparePalette(face);
  PrepareCanvas(glyph);
  VisitedSet visited_set;
  ColorContext context{this,      path_unitls_,      canvas_.get(), face,
                       &palette_, foreground_color_, &visited_set};
  context.canvas->Translate(-glyph.GetHoriBearingX(), glyph.GetHoriBearingY());
  return start_glyph(context, glyph.Id(), FT_COLOR_INCLUDE_ROOT_TRANSFORM);
}

bool ColorFreeType::ComputeColorV1Glyph(FT_Face face, const GlyphData& glyph,
                                        Rect* bounds) {
  VisitedSet visited_set;
  Matrix ctm;
  BoundsContext context{this, path_unitls_, face, &ctm, bounds, &visited_set};
  return start_glyph_bounds(context, glyph.Id(),
                            FT_COLOR_INCLUDE_ROOT_TRANSFORM);
}

void ColorFreeType::PreparePalette(FT_Face face) {
  if (palette_.empty()) {
    FT_Palette_Data palette_data;
    if (FT_Palette_Data_Get(face, &palette_data)) {
      return;
    }
    palette_count_ = palette_data.num_palette_entries;

    FT_Color* ft_palette = nullptr;
    if (FT_Palette_Select(face, 0, &ft_palette)) {
      return;
    }

    palette_.resize(palette_count_);
    for (size_t i = 0; i < palette_count_; ++i) {
      palette_[i] = ColorSetARGB(ft_palette[i].alpha, ft_palette[i].red,
                                 ft_palette[i].green, ft_palette[i].blue);
    }
  }
}

void ColorFreeType::PrepareCanvas(const GlyphData& glyph) {
  if (bitmap_) {
    // release previous bitmap
  }
  bitmap_ = std::make_unique<Bitmap>(glyph.GetWidth(), glyph.GetHeight(),
                                     skity::AlphaType::kPremul_AlphaType);
  canvas_ = skity::Canvas::MakeSoftwareCanvas(bitmap_.get());
}

}  // namespace skity
