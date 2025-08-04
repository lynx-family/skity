/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_PORTS_SCALER_CONTEXT_FREETYPE_HPP
#define SRC_TEXT_PORTS_SCALER_CONTEXT_FREETYPE_HPP

#include "src/text/ports/color_freetype.hpp"
#include "src/text/ports/freetype_face.hpp"
#include "src/text/ports/path_freetype.hpp"
#include "src/text/ports/typeface_freetype.hpp"
#include "src/text/scaler_context.hpp"
namespace skity {
class ScalerContextFreetype : public ScalerContext {
 public:
  ScalerContextFreetype(std::shared_ptr<TypefaceFreeType> typeface,
                        const ScalerContextDesc *desc);
  ~ScalerContextFreetype() override;

 protected:
  void GenerateMetrics(GlyphData *glyph) override;
  void GenerateImage(GlyphData *glyph, const StrokeDesc &stroke_desc) override;
  void GenerateImageInfo(GlyphData *glyph,
                         const StrokeDesc &stroke_desc) override;
  bool GeneratePath(GlyphData *glyph) override;
  void GenerateFontMetrics(FontMetrics *metrics) override;
  uint16_t OnGetFixedSize() override;

 private:
  FT_Error SetupSize();
  bool GetCBoxForLetter(char letter, FT_BBox *bbox);
  bool GeneratePathLock(GlyphData *glyph);
  void EmboldenIfNeeded(GlyphID id);

 private:
  FreetypeFace *ft_face_;
  FT_Face face_ = nullptr;     // Borrowed face from fFaceRec.
  FT_Size ft_size_ = nullptr;  // The size to apply to the fFace.
  FT_Int strike_index_ =
      -1;  // The bitmap strike for the fFace (or -1 if none).
  Vec2 text_scale_;
  Matrix22 transform_matrix_;
  FT_Matrix ft_transform_matrix_;
  uint32_t load_glyph_flags_;
  //  bool fDoLinearMetrics = false;

  std::unique_ptr<PathFreeType> path_utils_;
  std::unique_ptr<ColorFreeType> color_utils_;
};
}  // namespace skity
#endif  // SRC_TEXT_PORTS_SCALER_CONTEXT_FREETYPE_HPP
