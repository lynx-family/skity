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

#include <cstdlib>
#include <cstring>
#include <limits>
#include <skity/geometry/stroke.hpp>
#include <vector>

#include "src/base/fixed_types.hpp"
#include "src/render/sw/sw_a8_drawable.hpp"
#include "src/render/text/glyph_position.hpp"
#include "src/tracing.hpp"

namespace skity {
namespace {

struct GlyphSubpixelOffset {
  float x = 0.f;
  float y = 0.f;
  FT_Pos ft_x = 0;
  FT_Pos ft_y = 0;
};

GlyphSubpixelOffset GetGlyphSubpixelOffset(const ScalerContextDesc& desc,
                                           PackedGlyphID id) {
  if (!desc.subpixel_positioning) {
    return {};
  }

  const uint8_t x_phase = id.GetSubpixelXPhase();
  const uint8_t y_phase = id.GetSubpixelYPhase();
  return {GlyphSubpixelPhase(x_phase), GlyphSubpixelPhase(y_phase),
          static_cast<FT_Pos>(x_phase) << 4,
          -(static_cast<FT_Pos>(y_phase) << 4)};
}

void ApplyGlyphSubpixelOffset(FT_GlyphSlot slot,
                              const GlyphSubpixelOffset& offset) {
  if (slot->format == FT_GLYPH_FORMAT_OUTLINE &&
      (offset.ft_x != 0 || offset.ft_y != 0)) {
    // Skia applies the packed device-space phase after FT_Load_Glyph. FreeType
    // uses an upward Y axis, hence the negated Y offset above.
    FT_Outline_Translate(&slot->outline, offset.ft_x, offset.ft_y);
  }
}

void SetGlyphBitmapOrigin(GlyphBitmapData* image, FT_Pos bitmap_left,
                          FT_Pos bitmap_top, const GlyphSubpixelOffset& offset,
                          float context_scale) {
  // The run position already contains the packed phase. Cancel it from the
  // bitmap origin so that a phase-specific mask is placed on the integer
  // physical-pixel grid instead of applying the phase twice.
  image->origin_x = (bitmap_left - offset.x) / context_scale;
  image->origin_y = (bitmap_top + offset.y) / context_scale;
}

bool RasterizeOutline(FT_GlyphSlot slot, const GlyphSubpixelOffset& offset,
                      float context_scale, GlyphBitmapData* image) {
  if (slot == nullptr || image == nullptr || context_scale <= 0.f ||
      slot->format != FT_GLYPH_FORMAT_OUTLINE) {
    return false;
  }

  FT_BBox bbox;
  FT_Outline_Get_CBox(&slot->outline, &bbox);
  const FT_Pos x_min = bbox.xMin + offset.ft_x;
  const FT_Pos x_max = bbox.xMax + offset.ft_x;
  const FT_Pos y_min = bbox.yMin + offset.ft_y;
  const FT_Pos y_max = bbox.yMax + offset.ft_y;
  const FT_Pos left = FDot6Floor(x_min);
  const FT_Pos right = FDot6Ceil(x_max);
  const FT_Pos bottom = FDot6Floor(y_min);
  const FT_Pos top = FDot6Ceil(y_max);
  if (right <= left || top <= bottom ||
      static_cast<uint64_t>(right - left) >
          std::numeric_limits<unsigned int>::max() ||
      static_cast<uint64_t>(top - bottom) >
          std::numeric_limits<unsigned int>::max()) {
    return false;
  }

  // Skia's A8 FreeType path applies the packed phase and aligns the
  // phase-shifted outline bounds to the destination bitmap in one translate.
  // This keeps negative coordinates on floor semantics and prevents
  // FT_Render_Glyph from choosing a different implicit mask extent.
  const FT_Pos x_shift = offset.ft_x - left * 64;
  const FT_Pos y_shift = offset.ft_y - bottom * 64;
  FT_Outline_Translate(&slot->outline, x_shift, y_shift);

  FT_Bitmap target{};
  target.width = static_cast<unsigned int>(right - left);
  target.rows = static_cast<unsigned int>(top - bottom);
  target.pitch = static_cast<int>(target.width);
  target.pixel_mode = FT_PIXEL_MODE_GRAY;
  target.num_grays = 256;

  const size_t row_bytes = static_cast<size_t>(target.pitch);
  if (target.rows > 0u &&
      row_bytes > std::numeric_limits<size_t>::max() / target.rows) {
    return false;
  }
  std::vector<uint8_t> pixels(row_bytes * target.rows, 0u);
  target.buffer = pixels.data();
  if (FT_Outline_Get_Bitmap(slot->library, &slot->outline, &target) != 0 ||
      !internal::CopyFreetypeBitmap(target, image)) {
    return false;
  }

  SetGlyphBitmapOrigin(image, left, top, offset, context_scale);
  return true;
}

Rect GetColorRasterBounds(const GlyphData& glyph,
                          const GlyphSubpixelOffset& offset) {
  const float left = glyph.GetHoriBearingX() + offset.x;
  const float top = -glyph.GetHoriBearingY() + offset.y;
  return Rect::MakeLTRB(std::floor(left), std::floor(top),
                        std::ceil(left + glyph.GetWidth()),
                        std::ceil(top + glyph.GetHeight()));
}

bool ShouldSubpixelBitmap(FT_Face face, const ScalerContextDesc& desc,
                          FT_GlyphSlot slot, const Matrix22& transform,
                          const GlyphSubpixelOffset& offset) {
  const bool mechanism =
      slot != nullptr && slot->format == FT_GLYPH_FORMAT_BITMAP &&
      desc.subpixel_positioning && (offset.ft_x != 0 || offset.ft_y != 0);
  // Match Skia's policy: bitmap-only faces always resample for phase, while a
  // scalable face's embedded strike is only phase-resampled when another
  // transform already requires filtering.
  const bool policy =
      face != nullptr && (!FT_IS_SCALABLE(face) || !transform.IsIdentity());
  return mechanism && policy;
}

bool IsAxisAlignedForHinting(const ScalerContextDesc& desc) {
  const Matrix22 transform = desc.GetTransformMatrix();
  const bool keeps_device_axes =
      transform.GetSkewX() == 0.f && transform.GetSkewY() == 0.f;
  const bool swaps_device_axes =
      transform.GetScaleX() == 0.f && transform.GetScaleY() == 0.f;
  // This is Skia's FreeType isAxisAligned(rec) predicate. Font skew is part of
  // the pre-transform and therefore also makes grid-aligned hinting unsafe.
  return desc.skew_x == 0.f && (keeps_device_axes || swaps_device_axes);
}

bool RasterizeBitmap(FT_GlyphSlot slot, Matrix bitmap_transform,
                     const GlyphSubpixelOffset& offset,
                     bool apply_subpixel_offset, float context_scale,
                     GlyphBitmapData* image) {
  if (slot == nullptr || image == nullptr || context_scale <= 0.f ||
      slot->format != FT_GLYPH_FORMAT_BITMAP) {
    return false;
  }

  if (apply_subpixel_offset) {
    bitmap_transform.PostTranslate(offset.x, offset.y);
  }

  if (bitmap_transform.IsIdentity()) {
    if (!internal::CopyFreetypeBitmap(slot->bitmap, image)) {
      return false;
    }
    // Skia leaves a scalable strike un-resampled when its adjusted bitmap
    // transform is identity, so the packed phase does not enter the mask
    // bounds. Skity's run position already contains that phase; cancel it in
    // the bitmap origin just as the phase-aware branches do, otherwise phase
    // 3 crosses the nearest-sampled pixel boundary and shifts the whole glyph.
    SetGlyphBitmapOrigin(image, slot->bitmap_left, slot->bitmap_top, offset,
                         context_scale);
    return true;
  }

  GlyphBitmapData source;
  if (!internal::CopyFreetypeBitmap(slot->bitmap, &source)) {
    return false;
  }

  // The atlas contract keeps native color glyph bytes in FreeType BGRA order
  // and applies the R/B swizzle in the emoji fragment. Use an RGBA software
  // surface here as a byte-preserving four-channel resampler; declaring the
  // intermediate BGRA would make the software canvas convert channels before
  // the atlas shader performs its existing swizzle.
  const ColorType color_type =
      source.format == BitmapFormat::kGray8 ? ColorType::kA8 : ColorType::kRGBA;
  auto source_pixmap = std::make_shared<Pixmap>(
      static_cast<uint32_t>(source.width), static_cast<uint32_t>(source.height),
      AlphaType::kPremul_AlphaType, color_type);
  if (source_pixmap->WritableAddr() == nullptr ||
      source_pixmap->RowBytes() < source.RowBytes()) {
    if (source.need_free) {
      std::free(source.buffer);
    }
    return false;
  }
  for (uint32_t row = 0; row < static_cast<uint32_t>(source.height); ++row) {
    std::memcpy(source_pixmap->WritableAddr8(0, row),
                source.buffer + row * source.RowBytes(), source.RowBytes());
  }
  if (source.need_free) {
    std::free(source.buffer);
  }

  const Rect source_bounds =
      Rect::MakeXYWH(static_cast<float>(slot->bitmap_left),
                     -static_cast<float>(slot->bitmap_top),
                     static_cast<float>(slot->bitmap.width),
                     static_cast<float>(slot->bitmap.rows));
  const Rect mapped_bounds = bitmap_transform.MapRect(source_bounds);
  if (!mapped_bounds.IsFinite() || mapped_bounds.IsEmpty()) {
    return false;
  }

  const float left = std::floor(mapped_bounds.Left());
  const float top = std::floor(mapped_bounds.Top());
  const float right = std::ceil(mapped_bounds.Right());
  const float bottom = std::ceil(mapped_bounds.Bottom());
  const double width = static_cast<double>(right) - left;
  const double height = static_cast<double>(bottom) - top;
  if (!(width > 0.0 && height > 0.0) ||
      width > std::numeric_limits<uint32_t>::max() ||
      height > std::numeric_limits<uint32_t>::max()) {
    return false;
  }

  Bitmap destination(static_cast<uint32_t>(width),
                     static_cast<uint32_t>(height),
                     AlphaType::kPremul_AlphaType, color_type);
  auto canvas = Canvas::MakeSoftwareCanvas(&destination);
  auto source_image = Image::MakeImage(source_pixmap, nullptr);
  if (!canvas || !source_image) {
    return false;
  }

  // This is Skia's native-bitmap path: round out the transformed mask bounds,
  // translate them to the destination origin, then linearly resample the
  // strike. A packed phase is a post-translation on the bitmap transform.
  canvas->Translate(-left, -top);
  canvas->Concat(bitmap_transform);
  canvas->Translate(slot->bitmap_left, -slot->bitmap_top);
  SamplingOptions options;
  options.filter = FilterMode::kLinear;
  options.mipmap = MipmapMode::kNearest;
  canvas->DrawImage(source_image, 0, 0, options);

  const size_t row_bytes = destination.RowBytes();
  if (destination.Height() > 0u &&
      row_bytes > std::numeric_limits<size_t>::max() / destination.Height()) {
    return false;
  }
  const size_t byte_count = row_bytes * destination.Height();
  auto* pixels = static_cast<uint8_t*>(std::malloc(byte_count));
  if (pixels == nullptr) {
    return false;
  }
  std::memcpy(pixels, destination.GetPixelAddr(), byte_count);

  image->buffer = pixels;
  image->need_free = true;
  image->width = destination.Width();
  image->height = destination.Height();
  image->row_bytes = row_bytes;
  image->format = source.format;
  const float phase_x = apply_subpixel_offset ? offset.x : 0.f;
  const float phase_y = apply_subpixel_offset ? offset.y : 0.f;
  image->origin_x = (left - phase_x) / context_scale;
  image->origin_y = (-top + phase_y) / context_scale;
  return true;
}

}  // namespace

namespace internal {

bool CopyFreetypeBitmap(const FT_Bitmap& source, GlyphBitmapData* target) {
  if (target == nullptr) {
    return false;
  }

  BitmapFormat format = BitmapFormat::kGray8;
  size_t bytes_per_pixel = 1u;
  size_t packed_source_row_bytes = 0u;
  switch (source.pixel_mode) {
    case FT_PIXEL_MODE_MONO:
      packed_source_row_bytes = (source.width + 7u) / 8u;
      break;
    case FT_PIXEL_MODE_GRAY:
      packed_source_row_bytes = source.width;
      break;
    case FT_PIXEL_MODE_BGRA:
      format = BitmapFormat::kBGRA8;
      bytes_per_pixel = 4u;
      packed_source_row_bytes = static_cast<size_t>(source.width) * 4u;
      break;
    default:
      return false;
  }

  const size_t source_pitch = source.pitch < 0
                                  ? static_cast<size_t>(-int64_t{source.pitch})
                                  : static_cast<size_t>(source.pitch);
  const size_t target_row_bytes =
      static_cast<size_t>(source.width) * bytes_per_pixel;
  if ((source.width > 0u && source.rows > 0u && source.buffer == nullptr) ||
      source_pitch < packed_source_row_bytes ||
      (source.rows > 0u &&
       target_row_bytes > std::numeric_limits<size_t>::max() / source.rows)) {
    return false;
  }

  const size_t byte_count = target_row_bytes * source.rows;
  uint8_t* pixels = byte_count == 0u
                        ? nullptr
                        : static_cast<uint8_t*>(std::malloc(byte_count));
  if (byte_count != 0u && pixels == nullptr) {
    return false;
  }

  if (byte_count == 0u) {
    target->width = source.width;
    target->height = source.rows;
    target->buffer = nullptr;
    target->row_bytes = target_row_bytes;
    target->format = format;
    target->need_free = false;
    return true;
  }

  // FreeType defines `buffer` as the first logical row and `pitch` as the
  // signed offset to the following row. This also covers upward-flow bitmaps.
  const uint8_t* source_row = source.buffer;
  for (size_t y = 0; y < source.rows; ++y) {
    uint8_t* target_row = pixels + y * target_row_bytes;
    switch (source.pixel_mode) {
      case FT_PIXEL_MODE_MONO:
        for (size_t x = 0; x < source.width; ++x) {
          target_row[x] =
              (source_row[x >> 3u] & (0x80u >> (x & 7u))) ? 0xFFu : 0u;
        }
        break;
      case FT_PIXEL_MODE_GRAY:
      case FT_PIXEL_MODE_BGRA:
        std::memcpy(target_row, source_row, target_row_bytes);
        break;
      default:
        std::free(pixels);
        return false;
    }
    source_row += source.pitch;
  }

  target->width = source.width;
  target->height = source.rows;
  target->buffer = pixels;
  target->row_bytes = target_row_bytes;
  target->format = format;
  target->need_free = pixels != nullptr;
  return true;
}

}  // namespace internal
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

static bool GetGlyphBounds(FT_GlyphSlot glyph, Rect* bounds) {
  if (glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
    DEBUG_CHECK(false);
    return false;
  }
  if (0 == glyph->outline.n_contours) {
    return false;
  }

  FT_BBox bbox;
  FT_Outline_Get_CBox(&glyph->outline, &bbox);
  *bounds =
      Rect::MakeLTRB(FixedDot6ToFloat(bbox.xMin), -FixedDot6ToFloat(bbox.yMax),
                     FixedDot6ToFloat(bbox.xMax), -FixedDot6ToFloat(bbox.yMin));
  return true;
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
  bool linear_metrics = desc->IsLinearMetrics();
  Font::FontHinting hinting = desc->GetHinting();
  if (!IsAxisAlignedForHinting(*desc)) {
    // Skia's FreeType typeface filters non-axis-aligned strikes to unhinted
    // outlines before constructing the scaler. Hinting against the original
    // pixel grid would otherwise distort rotated or skewed glyphs.
    hinting = Font::FontHinting::kNone;
  }

  // Keep the A8 load-flag mapping synchronized with Skia's
  // SkScalerContext_FreeType constructor.
  switch (hinting) {
    case Font::FontHinting::kNone:
      load_flags = FT_LOAD_NO_HINTING;
      linear_metrics = true;
      break;
    case Font::FontHinting::kSlight:
      load_flags = FT_LOAD_TARGET_LIGHT;
      linear_metrics = true;
      break;
    case Font::FontHinting::kNormal:
    case Font::FontHinting::kFull:
      load_flags = FT_LOAD_TARGET_NORMAL;
      break;
    default:
      DEBUG_CHECK(false);
      break;
  }

  if (desc->IsForceAutoHinting()) {
    load_flags |= FT_LOAD_FORCE_AUTOHINT;
  }
  if (!desc->IsEmbeddedBitmaps()) {
    load_flags |= FT_LOAD_NO_BITMAP;
  }

  load_flags |= FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH;

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
    load_glyph_flags_ |= FT_LOAD_COLOR;
    // FreeType does not provide linear metrics for bitmap fonts.
    linear_metrics = false;
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
  linear_metrics_ = linear_metrics;
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
  if (FT_IS_SCALABLE(face_)) {
    if (FT_Get_Color_Glyph_Paint(face_, glyph->Id(),
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
        bounds =
            Rect(FixedDot6ToFloat(bbox.xMin), -FixedDot6ToFloat(bbox.yMax),
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
      glyph->color_type_ = GlyphColorType::kColorV1;
    }

    if (!have_layers) {
      // ColorV0
      FT_LayerIterator layerIterator = {0, 0, nullptr};
      FT_UInt layerGlyphIndex;
      FT_UInt layerColorIndex;
      FT_Int32 flags = load_glyph_flags_;
      flags |= FT_LOAD_BITMAP_METRICS_ONLY;  // Don't decode any bitmaps.
      flags |= FT_LOAD_NO_BITMAP;            // Ignore embedded bitmaps.
      flags &= ~FT_LOAD_RENDER;              // Don't scan convert.
      flags &= ~FT_LOAD_COLOR;               // Ignore SVG.
      // For COLRv0 compute the glyph bounding box from the union of layer
      // bounding boxes.
      Rect bounds;
      while (FT_Get_Color_Glyph_Layer(face_, glyph->Id(), &layerGlyphIndex,
                                      &layerColorIndex, &layerIterator)) {
        have_layers = true;
        if (FT_Load_Glyph(face_, layerGlyphIndex, flags)) {
          glyph->ZeroMetrics();
          return;
        }

        Rect currentBounds;
        if (GetGlyphBounds(face_->glyph, &currentBounds)) {
          bounds.Join(currentBounds);
        }
      }
      if (have_layers) {
        glyph->hori_bearing_x_ = bounds.Left();
        glyph->hori_bearing_y_ = -bounds.Top();
        glyph->width_ = bounds.Width();
        glyph->height_ = bounds.Height();
        glyph->y_min_ = bounds.Top();
        glyph->y_max_ = bounds.Bottom();
        glyph->color_type_ = GlyphColorType::kColorV0;
      }
    }

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
    if (linear_metrics_) {
      const float advance_scalar =
          FixedDot16ToFloat(face_->glyph->linearVertAdvance);
      glyph->advance_x_ = transform_matrix_.GetSkewX() * advance_scalar;
      glyph->advance_y_ = transform_matrix_.GetScaleY() * advance_scalar;
    } else {
      glyph->advance_x_ = -FixedDot6ToFloat(face_->glyph->advance.x);
      glyph->advance_y_ = FixedDot6ToFloat(face_->glyph->advance.y);
    }
  } else {
    if (linear_metrics_) {
      const float advance_scalar =
          FixedDot16ToFloat(face_->glyph->linearHoriAdvance);
      glyph->advance_x_ = transform_matrix_.GetScaleX() * advance_scalar;
      glyph->advance_y_ = transform_matrix_.GetSkewY() * advance_scalar;
    } else {
      glyph->advance_x_ = FixedDot6ToFloat(face_->glyph->advance.x);
      glyph->advance_y_ = -FixedDot6ToFloat(face_->glyph->advance.y);
    }
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

void ScalerContextFreetype::GenerateImage(PackedGlyphID id, GlyphData* glyph,
                                          const StrokeDesc& stroke_desc) {
  SKITY_TRACE_EVENT(ScalerContextFreetype_GenerateImage);
  std::lock_guard<std::mutex> locker(FreetypeFace::f_t_mutex());
  if (this->SetupSize()) {
    return;
  }

  const GlyphSubpixelOffset subpixel_offset = GetGlyphSubpixelOffset(desc_, id);

  if (FT_IS_SCALABLE(face_) &&
      (glyph->color_type_ == GlyphColorType::kColorV0 ||
       glyph->color_type_ == GlyphColorType::kColorV1)) {
    const Rect raster_bounds = GetColorRasterBounds(*glyph, subpixel_offset);
    color_utils_->SetForegroundColor(desc_.foreground_color);
    const bool drew_glyph =
        glyph->color_type_ == GlyphColorType::kColorV0
            ? color_utils_->DrawColorV0Glyph(
                  face_, *glyph, load_glyph_flags_, raster_bounds,
                  {subpixel_offset.x, subpixel_offset.y})
            : color_utils_->DrawColorV1Glyph(
                  face_, *glyph, raster_bounds,
                  {subpixel_offset.x, subpixel_offset.y});
    if (!drew_glyph) {
      glyph->image_ = {};
      return;
    }

    GlyphBitmapData& info = glyph->image_;
    Bitmap* bitmap = color_utils_->GetBitmap();
    if (bitmap == nullptr || bitmap->Width() == 0 || bitmap->Height() == 0) {
      info = {};
      return;
    }

    info.buffer = bitmap->GetPixelAddr();
    info.width = bitmap->Width();
    info.height = bitmap->Height();
    info.origin_x =
        (raster_bounds.Left() - subpixel_offset.x) / desc_.context_scale;
    info.origin_y =
        (-raster_bounds.Top() + subpixel_offset.y) / desc_.context_scale;
    info.format = BitmapFormat::kRGBA8;

    return;
  }

  if (FT_Load_Glyph(face_, glyph->Id(), load_glyph_flags_)) {
    return;
  }
  EmboldenIfNeeded(glyph->Id());

  FT_Bitmap bitmap;
  GlyphBitmapData& info = glyph->image_;

  if (face_->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
    if (stroke_desc.is_stroke) {
      ApplyGlyphSubpixelOffset(face_->glyph, subpixel_offset);
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
      // Won't stroke bitmap glyph. Copy before releasing ft_glyph.
      if (!internal::CopyFreetypeBitmap(bitmap, &info)) {
        FT_Done_Glyph(ft_glyph);
        return;
      }
      SetGlyphBitmapOrigin(&info, bitmapGlyph->left, bitmapGlyph->top,
                           subpixel_offset, desc_.context_scale);
      FT_Done_Glyph(ft_glyph);
    } else {
      if (!RasterizeOutline(face_->glyph, subpixel_offset, desc_.context_scale,
                            &info)) {
        return;
      }
    }
  } else {
    const bool subpixel_bitmap = ShouldSubpixelBitmap(
        face_, desc_, face_->glyph, transform_matrix_, subpixel_offset);
    if (!RasterizeBitmap(face_->glyph, transform_matrix_.ToMatrix(),
                         subpixel_offset, subpixel_bitmap, desc_.context_scale,
                         &info)) {
      info = {};
    }
  }
}

void ScalerContextFreetype::GenerateImageInfo(PackedGlyphID, GlyphData* glyph,
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
