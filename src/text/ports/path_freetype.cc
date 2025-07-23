// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/ports/path_freetype.hpp"

#include <freetype/ftoutln.h>
#include <freetype/ftsizes.h>

#include "src/base/fixed_types.hpp"
#include "src/utils/function_wrapper.hpp"

namespace skity {

namespace {

struct FTOutlineExtractInfo {
  Path* path = nullptr;
  FT_Face ft_face;
  //  float screen_width = 0.f;
  //  float screen_height = 0.f;
};

static int HandleMoveTo(const FT_Vector* to, void* user) {
  auto* info = static_cast<FTOutlineExtractInfo*>(user);
  info->path->MoveTo(FixedDot6ToFloat(to->x), -FixedDot6ToFloat(to->y));
  return 0;
}

static int HandleLineTo(const FT_Vector* to, void* user) {
  auto* info = static_cast<FTOutlineExtractInfo*>(user);
  info->path->LineTo(FixedDot6ToFloat(to->x), -FixedDot6ToFloat(to->y));
  return 0;
}

static int HandleConicTo(const FT_Vector* control, const FT_Vector* to,
                         void* user) {
  auto* info = static_cast<FTOutlineExtractInfo*>(user);
  info->path->QuadTo(FixedDot6ToFloat(control->x),
                     -FixedDot6ToFloat(control->y), FixedDot6ToFloat(to->x),
                     -FixedDot6ToFloat(to->y));
  return 0;
}

static int HandleCubicTo(const FT_Vector* control1, const FT_Vector* control2,
                         const FT_Vector* to, void* user) {
  auto* info = static_cast<FTOutlineExtractInfo*>(user);
  info->path->CubicTo(
      FixedDot6ToFloat(control1->x), -FixedDot6ToFloat(control1->y),
      FixedDot6ToFloat(control2->x), -FixedDot6ToFloat(control2->y),
      FixedDot6ToFloat(to->x), -FixedDot6ToFloat(to->y));
  return 0;
}

}  // namespace

bool PathFreeType::GenerateGlyphPath(FT_Face face, Path* path) {
  FT_Outline_Funcs callback;
  callback.move_to = HandleMoveTo;
  callback.line_to = HandleLineTo;
  callback.conic_to = HandleConicTo;
  callback.cubic_to = HandleCubicTo;

  callback.shift = 0;
  callback.delta = 0;

  FTOutlineExtractInfo outline_info;
  outline_info.path = path;
  outline_info.ft_face = face;

  auto error =
      FT_Outline_Decompose(&face->glyph->outline, &callback, &outline_info);

  if (error) {
    path->Reset();
    return false;
  }

  path->Close();
  return true;
}

bool PathFreeType::GenerateFacePath(FT_Face face, GlyphID glyph_id,
                                    Path* path) {
  uint32_t flags = 0;
  flags |= FT_LOAD_BITMAP_METRICS_ONLY;  // Don't decode any bitmaps.
  flags |= FT_LOAD_NO_BITMAP;            // Ignore embedded bitmaps.
  flags &= ~FT_LOAD_RENDER;              // Don't scan convert.
  flags &= ~FT_LOAD_COLOR;               // Ignore SVG.
  flags |= FT_LOAD_NO_HINTING;
  flags |= FT_LOAD_NO_AUTOHINT;
  flags |= FT_LOAD_IGNORE_TRANSFORM;

  using DoneFTSize = FunctionWrapper<decltype(FT_Done_Size), FT_Done_Size>;
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
    return false;
  }

  FT_Size old_ft_size = face->size;

  auto try_generate_path = [this, face, glyph_id, flags, path, &ft_size]() {
    FT_Error err = 0;

    err = FT_Activate_Size(ft_size.get());
    if (err != 0) {
      return false;
    }

    err = FT_Set_Char_Size(face, IntToFixedDot6(face->units_per_EM),
                           IntToFixedDot6(face->units_per_EM), 72, 72);
    if (err != 0) {
      return false;
    }

    err = FT_Load_Glyph(face, glyph_id, flags);
    if (err != 0) {
      path->Reset();
      return false;
    }

    if (!GenerateGlyphPath(face, path)) {
      path->Reset();
      return false;
    }

    return true;
  };

  bool path_generation_result = try_generate_path();

  FT_Activate_Size(old_ft_size);

  return path_generation_result;
}

}  // namespace skity
