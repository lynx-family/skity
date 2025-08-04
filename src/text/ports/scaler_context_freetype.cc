/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/ports/scaler_context_freetype.hpp"

#include <freetype/ftbitmap.h>
#include <freetype/ftcolor.h>
#include <freetype/ftoutln.h>
#include <freetype/ftsizes.h>
#include <freetype/ftstroke.h>
#include <freetype/tttables.h>

#include <cstring>
#include <skity/geometry/stroke.hpp>

#include "src/base/fixed_types.hpp"
#include "src/render/sw/sw_a8_drawable.hpp"
#include "src/tracing.hpp"

namespace skity {
static BitmapFormat ft_pixel_mode_to_fmt(FT_Pixel_Mode mode) {
  switch (mode) {
    case FT_PIXEL_MODE_GRAY:
      return BitmapFormat::kGray8;
    case FT_PIXEL_MODE_BGRA:
      return BitmapFormat::kBGRA8;
    default:
      return BitmapFormat::kUnknown;
  }
}
/** Returns the bitmap strike equal to or just larger than the requested size.
 */
static FT_Int ChooseBitmapStrike(FT_Face face, FT_F26Dot6 scaleY) {
  if (face == nullptr) {
    return -1;
  }

  FT_Pos requestedPPEM = scaleY;  // FT_Bitmap_Size::y_ppem is in 26.6 format.
  FT_Int chosenStrikeIndex = -1;
  FT_Pos chosenPPEM = 0;
  for (FT_Int strikeIndex = 0; strikeIndex < face->num_fixed_sizes;
       ++strikeIndex) {
    FT_Pos strikePPEM = face->available_sizes[strikeIndex].y_ppem;
    if (strikePPEM == requestedPPEM) {
      // exact match - our search stops here
      return strikeIndex;
    } else if (chosenPPEM < requestedPPEM) {
      // attempt to increase chosenPPEM
      if (chosenPPEM < strikePPEM) {
        chosenPPEM = strikePPEM;
        chosenStrikeIndex = strikeIndex;
      }
    } else {
      // attempt to decrease chosenPPEM, but not below requestedPPEM
      if (requestedPPEM < strikePPEM && strikePPEM < chosenPPEM) {
        chosenPPEM = strikePPEM;
        chosenStrikeIndex = strikeIndex;
      }
    }
  }
  return chosenStrikeIndex;
}

ScalerContextFreetype::ScalerContextFreetype(
    std::shared_ptr<TypefaceFreeType> typeface, const ScalerContextDesc* desc)
    : ScalerContext(typeface, desc),
      strike_index_(-1),
      path_utils_(std::make_unique<PathFreeType>()),
      color_utils_(std::make_unique<ColorFreeType>(path_utils_.get())) {
  std::lock_guard<std::mutex> ac(FreetypeFace::f_t_mutex());
  ft_face_ = typeface->GetFTFace();
  if (nullptr == ft_face_) {
    return;
  }
  FT_Int32 load_flags = FT_LOAD_DEFAULT;

  load_flags |= FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH;
  load_flags |= FT_LOAD_COLOR;

  load_glyph_flags_ = load_flags;
  using DoneFTSize = FunctionWrapper<decltype(FT_Done_Size), FT_Done_Size>;
  std::unique_ptr<std::remove_pointer_t<FT_Size>, DoneFTSize> ftSize(
      [this]() -> FT_Size {
        FT_Size size;
        FT_Error err = FT_New_Size(ft_face_->Face(), &size);
        if (err != 0) {
          return nullptr;
        }
        return size;
      }());
  if (nullptr == ftSize) {
    return;
  }
  FT_Error err = FT_Activate_Size(ftSize.get());
  if (err != 0) {
    return;
  }
  // ft ports use non-uniform scale
  desc->DecomposeMatrix(PortScaleType::kFull, &text_scale_.x, &text_scale_.y,
                        &transform_matrix_);
  // scale text size by context_scale.
  text_scale_.x *= desc->context_scale;
  text_scale_.y *= desc->context_scale;
  if (FT_IS_SCALABLE(ft_face_->Face())) {
    err = FT_Set_Char_Size(ft_face_->Face(), ScalarToFDot6(text_scale_.x),
                           ScalarToFDot6(text_scale_.y), 72, 72);
    if (err != 0) {
      return;
    }
    if (desc->text_size < 1) {
      float upem = ft_face_->Face()->units_per_EM;
      FT_Size_Metrics& ftmetrics = ft_face_->Face()->size->metrics;
      float x_ppem = upem * FixedDot16ToFloat(ftmetrics.x_scale) / 64.0f;
      float y_ppem = upem * FixedDot16ToFloat(ftmetrics.y_scale) / 64.0f;
      // matrix_scale_.x = text_size_x / x_ppem;
      // matrix_scale_.y = text_size_y / y_ppem;
      transform_matrix_ =
          transform_matrix_ *
          Matrix22(text_scale_.x / x_ppem, 0, 0, text_scale_.y / y_ppem);
    }
  } else if (FT_HAS_FIXED_SIZES(ft_face_->Face())) {
    strike_index_ =
        ChooseBitmapStrike(ft_face_->Face(), ScalarToFDot6(text_scale_.y));
    if (strike_index_ == -1) {
      return;
    }
    err = FT_Select_Size(ft_face_->Face(), strike_index_);
    if (err != 0) {
      strike_index_ = -1;
      return;
    }
    // matrix_scale_.x = text_size_x / ft_face_->Face()->size->metrics.x_ppem;
    // matrix_scale_.y = text_size_y / ft_face_->Face()->size->metrics.y_ppem;
    transform_matrix_ =
        transform_matrix_ *
        Matrix22(text_scale_.x / ft_face_->Face()->size->metrics.x_ppem, 0, 0,
                 text_scale_.y / ft_face_->Face()->size->metrics.y_ppem);
    load_glyph_flags_ &= ~FT_LOAD_NO_BITMAP;
  } else {
    return;
  }

  // non-uniform scaling and skewing will be here later.
  // We only support uniform scaling for now, as our software renderer cannnot
  // draw bitmap in an A8 canvas.
  ft_transform_matrix_.xx = FloatToFixedDot16(transform_matrix_.GetScaleX());
  ft_transform_matrix_.xy = FloatToFixedDot16(-transform_matrix_.GetSkewX());
  ft_transform_matrix_.yx = FloatToFixedDot16(-transform_matrix_.GetSkewY());
  ft_transform_matrix_.yy = FloatToFixedDot16(transform_matrix_.GetScaleY());

  FT_Palette_Select(ft_face_->Face(), 0, nullptr);

  ft_size_ = ftSize.release();
  face_ = ft_face_->Face();
}

ScalerContextFreetype::~ScalerContextFreetype() {
  std::lock_guard<std::mutex> ac(FreetypeFace::f_t_mutex());

  if (ft_size_ != nullptr) {
    FT_Done_Size(ft_size_);
  }

  ft_face_ = nullptr;
}
FT_Error ScalerContextFreetype::SetupSize() {
  FT_Error err = FT_Activate_Size(ft_size_);
  if (err != 0) {
    return err;
  }

  FT_Set_Transform(face_, &ft_transform_matrix_, nullptr);
  return 0;
}
bool ScalerContextFreetype::GetCBoxForLetter(char letter, FT_BBox* bbox) {
  FT_Face face = face_;
  const FT_UInt glyph_id = FT_Get_Char_Index(face, letter);
  if (!glyph_id) {
    return false;
  }
  if (FT_Load_Glyph(face, glyph_id, FT_LOAD_BITMAP_METRICS_ONLY)) {
    return false;
  }
  if (face->glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
    return false;
  }
  EmboldenIfNeeded(glyph_id);
  FT_Outline_Get_CBox(&face->glyph->outline, bbox);
  return true;
}
void ScalerContextFreetype::GenerateMetrics(GlyphData* glyph) {
  SKITY_TRACE_EVENT(ScalerContextFreetype_GenerateMetrics);
  std::lock_guard<std::mutex> locker(FreetypeFace::f_t_mutex());
  if (this->SetupSize()) {
    glyph->ZeroMetrics();
    return;
  }

  glyph->format_ = GlyphFormat::A8;
  FT_Bool have_layers = false;
  FT_OpaquePaint opaque_layer_paint{nullptr, 1};
  if (FT_IS_SCALABLE(face_) &&
      FT_Get_Color_Glyph_Paint(face_, glyph->Id(),
                               FT_COLOR_INCLUDE_ROOT_TRANSFORM,
                               &opaque_layer_paint)) {
    have_layers = true;
    Rect bounds;
    FT_ClipBox clip_box;
    if (FT_Get_Color_Glyph_ClipBox(face_, glyph->Id(), &clip_box)) {
      FT_BBox bbox;
      bbox.xMin = clip_box.bottom_left.x;
      bbox.xMax = clip_box.bottom_left.x;
      bbox.yMin = clip_box.bottom_left.y;
      bbox.yMax = clip_box.bottom_left.y;
      for (auto& corner :
           {clip_box.top_left, clip_box.top_right, clip_box.bottom_right}) {
        bbox.xMin = std::min(bbox.xMin, corner.x);
        bbox.yMin = std::min(bbox.yMin, corner.y);
        bbox.xMax = std::max(bbox.xMax, corner.x);
        bbox.yMax = std::max(bbox.yMax, corner.y);
      }
      bounds = Rect(FixedDot6ToFloat(bbox.xMin), -FixedDot6ToFloat(bbox.yMax),
                    FixedDot6ToFloat(bbox.xMax), -FixedDot6ToFloat(bbox.yMin));
    } else {
      color_utils_->ComputeColorV1Glyph(face_, *glyph, &bounds);
    }
    glyph->width_ = bounds.Width();
    glyph->height_ = bounds.Height();
    glyph->hori_bearing_x_ = bounds.Left();
    glyph->hori_bearing_y_ = -bounds.Top();
    glyph->y_min_ = bounds.Top();
    glyph->y_max_ = bounds.Bottom();

    if (have_layers) {
      glyph->format_ = GlyphFormat::RGBA32;
    }
  }

  auto load_flag = load_glyph_flags_ | FT_LOAD_BITMAP_METRICS_ONLY;
  FT_Error err;
  err = FT_Load_Glyph(face_, glyph->Id(), load_flag);
  if (err != 0) {
    glyph->ZeroMetrics();
    return;
  }
  if (!have_layers) {
    EmboldenIfNeeded(glyph->Id());
    if (face_->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
      auto scale = text_scale_.y / face_->units_per_EM;
      glyph->width_ = FixedDot6ToFloat(face_->glyph->metrics.width);
      glyph->height_ = FixedDot6ToFloat(face_->glyph->metrics.height);
      glyph->hori_bearing_x_ =
          FixedDot6ToFloat(face_->glyph->metrics.horiBearingX);
      glyph->hori_bearing_y_ =
          FixedDot6ToFloat(face_->glyph->metrics.horiBearingY);
      glyph->y_max_ = face_->bbox.yMax * scale;
      glyph->y_min_ = face_->bbox.yMin * scale;

      if (!transform_matrix_.IsIdentity()) {
        FT_BBox bbox;
        FT_Outline_Get_CBox(&face_->glyph->outline, &bbox);
        float left = FixedDot6ToFloat(bbox.xMin);
        float top = -FixedDot6ToFloat(bbox.yMax);
        float right = FixedDot6ToFloat(bbox.xMax);
        float bottom = -FixedDot6ToFloat(bbox.yMin);

        glyph->hori_bearing_x_ = left;
        glyph->hori_bearing_y_ = -top;
        glyph->width_ = right - left;
        glyph->height_ = bottom - top;
      }

      glyph->advance_x_ = FixedDot6ToFloat(face_->glyph->advance.x);
      glyph->advance_y_ = FixedDot6ToFloat(face_->glyph->advance.y);
    } else if (face_->glyph->format == FT_GLYPH_FORMAT_BITMAP) {
      if (face_->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
        glyph->image_.format = BitmapFormat::kBGRA8;
        glyph->format_ = GlyphFormat::BGRA32;
      }

      {
        Vec2 top_left{static_cast<float>(face_->glyph->bitmap_left),
                      -static_cast<float>(face_->glyph->bitmap_top)};
        Vec2 top_right{static_cast<float>(face_->glyph->bitmap_left +
                                          face_->glyph->bitmap.width),
                       -static_cast<float>(face_->glyph->bitmap_top)};
        Vec2 bottom_left{
            static_cast<float>(face_->glyph->bitmap_left),
            static_cast<float>(static_cast<int>(face_->glyph->bitmap.rows) -
                               face_->glyph->bitmap_top)};
        Vec2 bottom_right =
            Vec2(face_->glyph->bitmap_left + face_->glyph->bitmap.width,
                 static_cast<int>(face_->glyph->bitmap.rows) -
                     face_->glyph->bitmap_top);
        std::array<Vec2, 4> src{top_left, top_right, bottom_left, bottom_right};
        std::array<Vec2, 4> dst;
        transform_matrix_.MapPoints(dst.data(), src.data(), 4);
        float left = dst[0].x;
        float right = dst[0].x;
        float top = dst[0].y;
        float bottom = dst[0].y;
        for (size_t i = 1; i < 4; i++) {
          left = std::min(dst[i].x, left);
          right = std::max(dst[i].x, right);
          top = std::min(dst[i].y, top);
          bottom = std::max(dst[i].y, bottom);
        }

        glyph->width_ = right - left;
        glyph->height_ = bottom - top;
        glyph->hori_bearing_x_ = left;
        glyph->hori_bearing_y_ = -top;
        glyph->y_max_ = glyph->height_;
        glyph->y_min_ = 0;
        glyph->advance_x_ = glyph->width_;
        glyph->advance_y_ = glyph->height_;
      }
    }
  }

  if (this->IsVertical()) {
    glyph->advance_x_ = -FixedDot6ToFloat(face_->glyph->advance.x);
    glyph->advance_y_ = FixedDot6ToFloat(face_->glyph->advance.y);
  } else {
    glyph->advance_x_ = FixedDot6ToFloat(face_->glyph->advance.x);
    glyph->advance_y_ = -FixedDot6ToFloat(face_->glyph->advance.y);
  }
}

static FT_Stroker_LineCap ToFreetypeCap(Paint::Cap cap) {
  switch (cap) {
    case Paint::Cap::kButt_Cap:
      return FT_STROKER_LINECAP_BUTT;
    case Paint::Cap::kRound_Cap:
      return FT_STROKER_LINECAP_ROUND;
    case Paint::Cap::kSquare_Cap:
      return FT_STROKER_LINECAP_SQUARE;
  }
}

static FT_Stroker_LineJoin ToFreetypeJoin(Paint::Join join) {
  switch (join) {
    case Paint::Join::kBevel_Join:
      return FT_STROKER_LINEJOIN_BEVEL;
    case Paint::Join::kRound_Join:
      return FT_STROKER_LINEJOIN_ROUND;
    case Paint::Join::kMiter_Join:
      return FT_STROKER_LINEJOIN_MITER;
  }
}

void ScalerContextFreetype::GenerateImage(GlyphData* glyph,
                                          const StrokeDesc& stroke_desc) {
  SKITY_TRACE_EVENT(ScalerContextFreetype_GenerateImage);
  std::lock_guard<std::mutex> locker(FreetypeFace::f_t_mutex());
  if (this->SetupSize()) {
    return;
  }

  if (FT_IS_SCALABLE(face_)) {
    FT_OpaquePaint opaqueLayerPaint{nullptr, 1};
    if (FT_Get_Color_Glyph_Paint(face_, glyph->Id(),
                                 FT_COLOR_INCLUDE_ROOT_TRANSFORM,
                                 &opaqueLayerPaint)) {
      color_utils_->DrawColorV1Glyph(face_, *glyph);
      GlyphBitmapData& info = glyph->image_;
      Bitmap* bitmap = color_utils_->GetBitmap();
      info.buffer = bitmap->GetPixelAddr();
      info.width = bitmap->Width();
      info.height = bitmap->Height();
      info.origin_x = glyph->GetHoriBearingX() / desc_.context_scale;
      info.origin_y = glyph->GetHoriBearingY() / desc_.context_scale;
      info.format = BitmapFormat::kRGBA8;

      return;
    }
  }

  if (FT_Load_Glyph(face_, glyph->Id(), load_glyph_flags_)) {
    return;
  }
  EmboldenIfNeeded(glyph->Id());

  FT_Bitmap bitmap;
  GlyphBitmapData& info = glyph->image_;

  if (face_->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
    if (stroke_desc.is_stroke) {
      FT_Stroker stroker;

      int radius = static_cast<FT_Fixed>(
          stroke_desc.stroke_width * text_scale_.y / desc_.text_size / 2 * 64);
      FT_Stroker_New(ft_face_->library(), &stroker);
      FT_Stroker_Set(stroker, radius, ToFreetypeCap(stroke_desc.cap),
                     ToFreetypeJoin(stroke_desc.join),
                     static_cast<FT_Fixed>(stroke_desc.miter_limit * 64));
      FT_Glyph ft_glyph;
      FT_Get_Glyph(face_->glyph, &ft_glyph);
      FT_Glyph_Stroke(&ft_glyph, stroker, true);
      FT_Glyph_To_Bitmap(&ft_glyph, FT_RENDER_MODE_NORMAL, nullptr, true);
      FT_BitmapGlyph bitmapGlyph = reinterpret_cast<FT_BitmapGlyph>(ft_glyph);
      FT_Stroker_Done(stroker);

      glyph->hori_bearing_x_ = bitmapGlyph->left;
      glyph->hori_bearing_y_ = bitmapGlyph->top;
      bitmap = bitmapGlyph->bitmap;
      // won't stroke bitmap glyph
      uint8_t* copy_data = reinterpret_cast<uint8_t*>(
          std::malloc(bitmap.width * bitmap.rows * sizeof(uint8_t)));
      std::memcpy(copy_data, bitmap.buffer,
                  bitmap.width * bitmap.rows * sizeof(uint8_t));
      info.buffer = copy_data;
      info.need_free = true;
      info.width = bitmap.width;
      info.height = bitmap.rows;
      info.origin_x = bitmapGlyph->left / desc_.context_scale;
      info.origin_y = bitmapGlyph->top / desc_.context_scale;
      FT_Done_Glyph(ft_glyph);
    } else {
      if (FT_Render_Glyph(face_->glyph, FT_RENDER_MODE_NORMAL)) {
        return;
      }
      bitmap = face_->glyph->bitmap;
      uint8_t* copy_data = reinterpret_cast<uint8_t*>(
          std::malloc(bitmap.width * bitmap.rows * sizeof(uint8_t)));
      std::memcpy(copy_data, bitmap.buffer,
                  bitmap.width * bitmap.rows * sizeof(uint8_t));
      info.buffer = copy_data;
      info.need_free = true;
      info.width = bitmap.width;
      info.height = bitmap.rows;
      info.origin_x = glyph->GetHoriBearingX() / desc_.context_scale;
      info.origin_y = glyph->GetHoriBearingY() / desc_.context_scale;
    }
  } else {
    if (transform_matrix_.IsIdentity()) {
      bitmap = face_->glyph->bitmap;
      // Not use slot memory directly as slot in ft is temporary before next
      // loading.
      uint32_t bytes_count = bitmap.width * bitmap.rows * sizeof(uint32_t);
      uint8_t* copy_data = reinterpret_cast<uint8_t*>(std::malloc(bytes_count));
      std::memcpy(copy_data, bitmap.buffer, bytes_count);
      info.buffer = copy_data;
      info.need_free = true;
      info.width = bitmap.width;
      info.height = bitmap.rows;
      info.origin_x = glyph->GetHoriBearingX() / desc_.context_scale;
      info.origin_y = glyph->GetHoriBearingY() / desc_.context_scale;
    } else {
      // transform bitmap
      bitmap = face_->glyph->bitmap;
      auto pixmap = std::make_shared<Pixmap>(bitmap.width, bitmap.rows,
                                             AlphaType::kPremul_AlphaType,
                                             ColorType::kRGBA);
      std::memcpy(pixmap->WritableAddr(), bitmap.buffer,
                  bitmap.rows * bitmap.pitch);
      auto origin_image = Image::MakeImage(pixmap, nullptr);

      uint32_t dst_width = std::floor(glyph->GetWidth());
      uint32_t dst_height = std::floor(glyph->GetHeight());
      Bitmap dst_bitmap(dst_width, dst_height, AlphaType::kPremul_AlphaType);
      auto canvas = skity::Canvas::MakeSoftwareCanvas(&dst_bitmap);

      if (canvas) {
        canvas->Translate(-glyph->hori_bearing_x_, glyph->hori_bearing_y_);
        canvas->Concat(transform_matrix_.ToMatrix());
        canvas->Translate(face_->glyph->bitmap_left, -face_->glyph->bitmap_top);

        SamplingOptions options;
        options.filter = FilterMode::kLinear;
        options.mipmap = MipmapMode::kNearest;
        canvas->DrawImage(origin_image, 0, 0, options);

        // copy dst
        uint32_t bytes_count = dst_width * dst_height * sizeof(uint32_t);
        uint8_t* copy_data =
            reinterpret_cast<uint8_t*>(std::malloc(bytes_count));
        std::memcpy(copy_data, dst_bitmap.GetPixelAddr(), bytes_count);
        info.buffer = copy_data;
        info.need_free = true;

        info.width = dst_width;
        info.height = dst_height;
        info.origin_x = glyph->GetHoriBearingX() / desc_.context_scale;
        info.origin_y = glyph->GetHoriBearingY() / desc_.context_scale;
      } else {
        // transformed bitmap is invisible
        info.buffer = nullptr;
        info.width = 0;
        info.height = 0;
        info.origin_x = 0;
        info.origin_y = 0;
      }
    }
  }

  info.format =
      ft_pixel_mode_to_fmt(static_cast<FT_Pixel_Mode>(bitmap.pixel_mode));
}

void ScalerContextFreetype::GenerateImageInfo(GlyphData* glyph,
                                              const StrokeDesc& desc) {}

bool ScalerContextFreetype::GeneratePath(GlyphData* glyph_data) {
  SKITY_TRACE_EVENT(ScalerContextFreetype_GeneratePath);
  std::lock_guard<std::mutex> locker(FreetypeFace::f_t_mutex());
  return GeneratePathLock(glyph_data);
}

bool ScalerContextFreetype::GeneratePathLock(GlyphData* glyph_data) {
  auto* path = &glyph_data->path_;
  // FT_IS_SCALABLE is documented to mean the face contains outline glyphs.
  if (!FT_IS_SCALABLE(face_) || this->SetupSize()) {
    path->Reset();
    return false;
  }

  uint32_t flags = load_glyph_flags_;
  flags |= FT_LOAD_NO_BITMAP;  // ignore embedded bitmaps so we're sure to get
                               // the outline
  flags &= ~FT_LOAD_RENDER;    // don't scan convert (we just want the outline)

  FT_Error err = FT_Load_Glyph(face_, glyph_data->Id(), flags);
  if (err != 0 || face_->glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
    path->Reset();
    return false;
  }
  EmboldenIfNeeded(glyph_data->Id());
  path_utils_->GenerateGlyphPath(face_, path);

  // The path's origin from FreeType is always the horizontal layout origin.
  // Offset the path so that it is relative to the vertical origin if needed.
  if (this->IsVertical()) {
    FT_Vector vector;
    vector.x =
        face_->glyph->metrics.vertBearingX - face_->glyph->metrics.horiBearingX;
    vector.y = -face_->glyph->metrics.vertBearingY -
               face_->glyph->metrics.horiBearingY;
    FT_Vector_Transform(&vector, &ft_transform_matrix_);
    //    path->Offset(SkFDot6ToScalar(vector.x), -SkFDot6ToScalar(vector.y));
  }
  return true;
}
void ScalerContextFreetype::GenerateFontMetrics(FontMetrics* metrics) {
  SKITY_TRACE_EVENT(ScalerContextFreetype_GenerateFontMetrics);
  if (face_ == nullptr || metrics == nullptr) return;
  std::lock_guard<std::mutex> locker(FreetypeFace::f_t_mutex());
  if (this->SetupSize()) {
    memset(metrics, 0, sizeof(*metrics));
    return;
  }
  float upem = static_cast<float>(face_->units_per_EM);
  float x_height = 0.0f;
  float avgCharWidth = 0.0f;
  float cap_height = 0.0f;
  float strikeoutThickness = 0.0f, strikeoutPosition = 0.0f;
  TT_OS2* os2 =
      reinterpret_cast<TT_OS2*>(FT_Get_Sfnt_Table(face_, ft_sfnt_os2));
  if (os2) {
    x_height = static_cast<float>(os2->sxHeight) / upem * text_scale_.y;
    avgCharWidth = static_cast<float>(os2->xAvgCharWidth) / upem;
    strikeoutThickness = static_cast<float>(os2->yStrikeoutSize) / upem;
    strikeoutPosition = -static_cast<float>(os2->yStrikeoutPosition) / upem;
    //    metrics->fFlags |= FontMetrics::kStrikeoutThicknessIsValid_Flag;
    //    metrics->fFlags |= FontMetrics::kStrikeoutPositionIsValid_Flag;
    if (os2->version != 0xFFFF && os2->version >= 2) {
      cap_height = static_cast<float>(os2->sCapHeight) / upem * text_scale_.y;
    }
  }

  float ascent, descent, leading, xmin, xmax, ymin, ymax;
  float underlineThickness, underlinePosition;
  if (face_->face_flags & FT_FACE_FLAG_SCALABLE) {
    ascent = -static_cast<float>(face_->ascender) / upem;
    descent = -static_cast<float>(face_->descender) / upem;
    leading = static_cast<float>(face_->height +
                                 (face_->descender - face_->ascender)) /
              upem;

    xmin = static_cast<float>(face_->bbox.xMin) / upem;
    xmax = static_cast<float>(face_->bbox.xMax) / upem;
    ymin = -static_cast<float>(face_->bbox.yMin) / upem;
    ymax = -static_cast<float>(face_->bbox.yMax) / upem;
    underlineThickness = static_cast<float>(face_->underline_thickness) / upem;
    underlinePosition = -static_cast<float>(face_->underline_position +
                                            face_->underline_thickness / 2) /
                        upem;

    // we may be able to synthesize x_height and cap_height from outline
    if (!x_height) {
      FT_BBox bbox;
      if (GetCBoxForLetter('x', &bbox)) {
        x_height = static_cast<float>(bbox.yMax) / 64.0f;
      }
    }
    if (!cap_height) {
      FT_BBox bbox;
      if (GetCBoxForLetter('H', &bbox)) {
        cap_height = static_cast<float>(bbox.yMax) / 64.0f;
      }
    }
  } else if (strike_index_ != -1) {
    float xppem = static_cast<float>(face_->size->metrics.x_ppem);
    float yppem = static_cast<float>(face_->size->metrics.y_ppem);
    ascent =
        -static_cast<float>(face_->size->metrics.ascender) / (yppem * 64.0f);
    descent =
        -static_cast<float>(face_->size->metrics.descender) / (yppem * 64.0f);
    leading =
        (static_cast<float>(face_->size->metrics.height) / (yppem * 64.0f)) +
        ascent - descent;

    xmin = 0.0f;
    xmax =
        static_cast<float>(face_->available_sizes[strike_index_].width) / xppem;
    ymin = descent;
    ymax = ascent;
    // The actual bitmaps may be any size and placed at any offset.
    //    metrics->fFlags |= SkFontMetrics::kBoundsInvalid_Flag;

    underlineThickness = 0;
    underlinePosition = 0;
    //    metrics->fFlags &= ~SkFontMetrics::kUnderlineThicknessIsValid_Flag;
    //    metrics->fFlags &= ~SkFontMetrics::kUnderlinePositionIsValid_Flag;

    TT_Postscript* post = reinterpret_cast<TT_Postscript*>(
        FT_Get_Sfnt_Table(face_, ft_sfnt_post));
    if (post) {
      underlineThickness = static_cast<float>(post->underlineThickness) / upem;
      underlinePosition = -static_cast<float>(post->underlinePosition) / upem;
      //      metrics->fFlags |=
      //      SkFontMetrics::kUnderlineThicknessIsValid_Flag; metrics->fFlags
      //      |= SkFontMetrics::kUnderlinePositionIsValid_Flag;
    }
  } else {
    memset(metrics, 0, sizeof(*metrics));
    return;
  }

  if (!avgCharWidth) {
    avgCharWidth = xmax - xmin;
  }

  // disallow negative linespacing
  if (leading < 0.0f) {
    leading = 0.0f;
  }

  metrics->top_ = ymax * text_scale_.y;
  metrics->ascent_ = ascent * text_scale_.y;
  metrics->descent_ = descent * text_scale_.y;
  metrics->bottom_ = ymin * text_scale_.y;
  metrics->leading_ = leading * text_scale_.y;
  metrics->avg_char_width_ = avgCharWidth * text_scale_.y;
  metrics->x_min_ = xmin * text_scale_.y;
  metrics->x_max_ = xmax * text_scale_.y;
  metrics->max_char_width_ = metrics->x_max_ - metrics->x_min_;
  metrics->x_height_ = x_height;
  metrics->cap_height_ = cap_height;
  metrics->underline_thickness_ = underlineThickness * text_scale_.y;
  metrics->underline_position_ = underlinePosition * text_scale_.y;
  metrics->strikeout_thickness_ = strikeoutThickness * text_scale_.y;
  metrics->strikeout_position_ = strikeoutPosition * text_scale_.y;
}
uint16_t ScalerContextFreetype::OnGetFixedSize() {
  if (strike_index_ == -1) return 0;
  std::lock_guard<std::mutex> locker(FreetypeFace::f_t_mutex());
  if (this->SetupSize()) {
    return 0;
  }
  return ft_face_->Face()->size->metrics.y_ppem;
}

void ScalerContextFreetype::EmboldenIfNeeded(GlyphID id) {
  SKITY_TRACE_EVENT(ScalerContextFreetype_GenerateFontMetrics);
  if (desc_.fake_bold) {
    switch (face_->glyph->format) {
      case FT_GLYPH_FORMAT_OUTLINE: {
        float text_size = text_scale_.y;
        float ratio = 24.f;
        if (text_size > 36.f) {
          ratio = 32.f;
        } else if (text_size > 9.f) {
          float f = std::min((text_size - 9.f) / 27.f, 1.f);
          ratio = (1.f - f) * 24.f + f * 32.f;
        }
        FT_Pos strength =
            FT_MulFix(face_->units_per_EM, face_->size->metrics.y_scale) /
            ratio;
        FT_Outline_Embolden(&face_->glyph->outline, strength);
        break;
      }
      case FT_GLYPH_FORMAT_BITMAP:
        if (!face_->glyph->bitmap.buffer) {
          FT_Load_Glyph(face_, id, load_glyph_flags_);
        }
        FT_GlyphSlot_Own_Bitmap(face_->glyph);
        FT_Bitmap_Embolden(face_->glyph->library, &face_->glyph->bitmap, 1 << 6,
                           0);
        break;
      default:
        // do nothing for other formats
        break;
    }
  }
}

}  // namespace skity
