/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/ports/typeface_freetype.hpp"

#include <freetype/ftcolor.h>
#include <freetype/ftmm.h>
#include <freetype/ftoutln.h>
#include <freetype/tttables.h>

#include <algorithm>
#include <skity/text/font_manager.hpp>

#include "src/text/ports/scaler_context_freetype.hpp"

namespace skity {

template <typename T>
static constexpr const T& clamp(const T& x, const T& lo, const T& hi) {
  return std::max(lo, std::min(x, hi));
}

std::unique_ptr<FreetypeFaceHolder> FreetypeFaceHolder::Make(
    std::shared_ptr<Data> data, const FontArguments& font_args) {
  std::unique_ptr<FreetypeFaceHolder> holder;
  std::unique_ptr<FreetypeFace> font_face =
      std::make_unique<FreetypeFace>(data, font_args);
  holder.reset(new FreetypeFaceHolder(std::move(font_face)));
  return holder;
}

static VariationPosition VariationFromFontArguments(
    const std::vector<VariationAxis>& axes, const VariationPosition& current,
    const FontArguments& args) {
  VariationPosition expected;
  size_t axis_count = axes.size();

  for (size_t i = 0; i < axis_count; i++) {
    const VariationAxis& axis = axes[i];
    const float axis_min = axis.min;
    const float axis_max = axis.max;

    float value = axis.def;

    if (current.GetCoordinates().size() > i) {
      if (axis.tag == current.GetCoordinates()[i].axis) {
        value = current.GetCoordinates()[i].value;
      } else {
        for (size_t j = 0; j < current.GetCoordinates().size(); ++j) {
          if (axis.tag == current.GetCoordinates()[j].axis) {
            value = current.GetCoordinates()[j].value;
            break;
          }
        }
      }
    }

    const VariationPosition position = args.GetVariationDesignPosition();
    for (int j = position.GetCoordinates().size(); j-- > 0;) {
      if (axis.tag == position.GetCoordinates()[j].axis) {
        value = clamp(position.GetCoordinates()[j].value, axis_min, axis_max);
        break;
      }
    }

    expected.AddCoordinate(axis.tag, value);
  }

  return expected;
}
class AutoFTAccess {
 public:
  explicit AutoFTAccess(const TypefaceFreeType* tf) : face_(nullptr) {
    FreetypeFace::f_t_mutex().lock();
    face_ = tf->GetFTFace();
  }
  ~AutoFTAccess() { FreetypeFace::f_t_mutex().unlock(); }

  FT_Face Face() { return face_ ? face_->Face() : nullptr; }

 private:
  FreetypeFace* face_;
};

std::shared_ptr<TypefaceFreeType> TypefaceFreeType::Make(
    std::shared_ptr<Data> data, const FontArguments& font_args) {
  std::unique_ptr<FreetypeFace> face =
      std::make_unique<FreetypeFace>(data, font_args);
  if (!face->Valid()) {
    return nullptr;
  }
  FontStyle font_style = face->GetFontStyle();
  return std::make_shared<TypefaceFreeTypeData>(std::move(data), font_args,
                                                font_style);
}

TypefaceFreeType::TypefaceFreeType(const class FontStyle& style)
    : Typeface(style) {}

int TypefaceFreeType::OnGetTableTags(FontTableTag tags[]) const {
  AutoFTAccess fta(this);
  FT_Face face = fta.Face();
  if (!face) {
    return 0;
  }

  FT_ULong tableCount = 0;
  FT_Error error;

  // When 'tag' is nullptr, returns number of tables in 'length'.
  error = FT_Sfnt_Table_Info(face, 0, nullptr, &tableCount);
  if (error) {
    return 0;
  }

  if (tags) {
    for (FT_ULong tableIndex = 0; tableIndex < tableCount; ++tableIndex) {
      FT_ULong tableTag;
      FT_ULong tablelength;
      error = FT_Sfnt_Table_Info(face, tableIndex, &tableTag, &tablelength);
      if (error) {
        return 0;
      }
      tags[tableIndex] = static_cast<FontTableTag>(tableTag);
    }
  }
  return tableCount;
}

size_t TypefaceFreeType::OnGetTableData(FontTableTag tag, size_t offset,
                                        size_t length, void* data) const {
  AutoFTAccess fta(this);
  FT_Face face = fta.Face();
  if (!face) {
    return 0;
  }

  FT_ULong tableLength = 0;
  FT_Error error;

  // When 'length' is 0 it is overwritten with the full table length; 'offset'
  // is ignored.
  error = FT_Load_Sfnt_Table(face, tag, 0, nullptr, &tableLength);
  if (error) {
    return 0;
  }

  if (offset > tableLength) {
    return 0;
  }
  FT_ULong size = std::min((FT_ULong)length, tableLength - (FT_ULong)offset);
  if (data) {
    error = FT_Load_Sfnt_Table(face, tag, offset,
                               reinterpret_cast<FT_Byte*>(data), &size);
    if (error) {
      return 0;
    }
  }

  return size;
}

void TypefaceFreeType::OnCharsToGlyphs(const uint32_t* chars, int count,
                                       GlyphID glyphs[]) const {
  int i = 0;
  {
    std::lock_guard<std::mutex> ama(C2GCacheMutex_);
    for (i = 0; i < count; ++i) {
      auto iter = C2GCache_.find(chars[i]);
      if (iter == C2GCache_.end()) {
        break;
      }
      glyphs[i] = iter->second;
    }
    if (i == count) {
      return;
    }
  }
  std::lock_guard<std::mutex> ama(C2GCacheMutex_);
  AutoFTAccess fta(this);
  FT_Face face = fta.Face();
  if (!face) {
    if (count < 0) {
      memset(glyphs, 0, count * sizeof(glyphs[0]));
    }
    // return all 0s
    return;
  }

  for (; i < count; ++i) {
    Unichar c = chars[i];
    auto iter = C2GCache_.find(c);
    if (iter != C2GCache_.end()) {
      glyphs[i] = iter->second;
    } else {
      glyphs[i] = (uint16_t)(FT_Get_Char_Index(face, chars[i]));
      C2GCache_[chars[i]] = glyphs[i];
    }
  }
  if (C2GCache_.size() > 256) {
    C2GCache_.clear();
  }
}
std::shared_ptr<Data> TypefaceFreeType::OnGetData() {
  AutoFTAccess fta(this);
  FT_Face face = fta.Face();
  if (!face) {
    return nullptr;
  }
  return GetFTFace()->GetData();
}

bool TypefaceFreeType::OnContainsColorTable() const {
  AutoFTAccess fta(this);
  FT_Face face = fta.Face();
  if (!face) {
    return false;
  }
  return FT_HAS_COLOR(GetFTFace()->Face());
}

std::unique_ptr<ScalerContext> TypefaceFreeType::OnCreateScalerContext(
    const ScalerContextDesc* desc) const {
  auto c = std::make_unique<ScalerContextFreetype>(
      std::static_pointer_cast<TypefaceFreeType>(
          const_cast<TypefaceFreeType*>(this)->shared_from_this()),
      desc);
  return std::move(c);
}

uint32_t TypefaceFreeType::OnGetUPEM() const {
  AutoFTAccess fta(this);
  FT_Face face = fta.Face();
  if (!face) {
    return 0;
  }

  uint32_t upem = face->units_per_EM;
  // At least some versions of FreeType set face->units_per_EM to 0 for bitmap
  // only fonts.
  if (upem == 0) {
    TT_Header* ttHeader =
        reinterpret_cast<TT_Header*>(FT_Get_Sfnt_Table(face, ft_sfnt_head));
    if (ttHeader) {
      upem = ttHeader->Units_Per_EM;
    }
  }
  return upem;
}

VariationPosition TypefaceFreeType::OnGetVariationDesignPosition() const {
  AutoFTAccess fta(this);
  FT_Face face = fta.Face();
  if (!face) {
    return VariationPosition{};
  }
  return FontScanner::GetVariationDesignPositionLocked(face,
                                                       GetFTFace()->library());
}

std::vector<VariationAxis> TypefaceFreeType::OnGetVariationDesignParameters()
    const {
  AutoFTAccess fta(this);
  FT_Face face = fta.Face();
  if (!face) {
    return {};
  }

  return FontScanner::GetVariationDesignParametersLocked(
      face, GetFTFace()->library());
}

std::shared_ptr<Typeface> TypefaceFreeType::OnMakeVariation(
    const FontArguments& args) const {
  AutoFTAccess fta(this);
  FT_Face face = fta.Face();
  if (!face) {
    return {};
  }

  const VariationPosition position = VariationFromFontArguments(
      FontScanner::GetVariationDesignParametersLocked(face,
                                                      GetFTFace()->library()),
      FontScanner::GetVariationDesignPositionLocked(face,
                                                    GetFTFace()->library()),
      args);
  FontArguments expected_args;
  expected_args.SetVariationDesignPosition(position).SetCollectionIndex(
      args.GetCollectionIndex());

  FaceData face_data = GetFaceData();
  return TypefaceFreeType::Make(face_data.data, expected_args);
}

void TypefaceFreeType::OnGetFontDescriptor(FontDescriptor& desc) const {
  AutoFTAccess fta(this);
  FT_Face face = fta.Face();
  if (!face) {
    return;
  }

  desc.family_name = face->family_name;

  auto post_script_name = FT_Get_Postscript_Name(face);

  if (post_script_name) {
    desc.post_script_name = post_script_name;
  }

  desc.factory_id = kFontFactoryID;
}

FaceData TypefaceFreeType::GetFaceData() const { return OnGetFaceData(); }

FreetypeFace* TypefaceFreeType::GetFTFace() const {
  std::call_once(flag_, [this] {
    const FaceData face_data = this->GetFaceData();
    freetype_face_holder_ =
        FreetypeFaceHolder::Make(face_data.data, face_data.font_args);
  });
  return freetype_face_holder_ ? freetype_face_holder_->GetFreetypeFace()
                               : nullptr;
}

TypefaceFreeTypeData::TypefaceFreeTypeData(std::shared_ptr<Data> data,
                                           const FontArguments& font_args,
                                           const FontStyle& style)
    : TypefaceFreeType(style), data_(std::move(data)), font_args_(font_args) {}

FaceData TypefaceFreeTypeData::OnGetFaceData() const {
  return FaceData{data_, font_args_};
}

bool TypefaceFreeType::IsVariationTypeface() {
  AutoFTAccess fta(this);
  FT_Face face = fta.Face();
  if (!face) {
    return false;
  }

  std::vector<VariationAxis> axes =
      FontScanner::GetVariationDesignParametersLocked(face,
                                                      GetFTFace()->library());

  return axes.size() > 0;
}

}  // namespace skity
