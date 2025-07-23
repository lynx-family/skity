// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/ports/freetype_face.hpp"

#include <freetype/ftmm.h>
#include <freetype/t1tables.h>
#include <freetype/tttables.h>

#include "src/base/fixed_types.hpp"
#include "src/geometry/math.hpp"
#include "src/logging.hpp"

namespace skity {
/// FreeTypeLibrary
FreeTypeLibrary::FreeTypeLibrary() : ft_library_(0) {
  FT_Error error = FT_Init_FreeType(&ft_library_);

  if (error) {
    // LOGE("Couldn't initialize the library: FT_Init_FreeType() failed");
  }
}

FreeTypeLibrary::~FreeTypeLibrary() { FT_Done_FreeType(ft_library_); }

static FreeTypeLibrary* global_freetype_library;

/// FreeTypeFace
FreetypeFace::FreetypeFace(const std::shared_ptr<Data>& stream,
                           const FontArguments& font_args)
    : data_(stream) {
  RefFreeTypeLibrary();
  const void* memoryBase = data_->RawData();
  if (memoryBase) {
    FT_Open_Args args;
    memset(&args, 0, sizeof(args));
    args.flags = FT_OPEN_MEMORY;
    args.memory_base = (const FT_Byte*)memoryBase;
    args.memory_size = data_->Size();

    FT_Face face;
    if (!FT_Open_Face(global_freetype_library->library(), &args,
                      font_args.GetCollectionIndex(), &face)) {
      ft_face_.reset(face);
    }
  }

  SetupVariation(font_args);
}

FreetypeFace::~FreetypeFace() {
  if (Valid()) {
    ft_face_.reset();  // Must release face before the library, the library
                       // frees existing faces.
  }

  UnrefFreeTypeLibrary();
}

void FreetypeFace::SetupVariation(const FontArguments& font_args) {
  if (!Valid()) {
    return;
  }
  if (!(ft_face_->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS)) {
    return;
  }

  // If a named variation is requested, don't overwrite the named variation's
  // position. if (data.getIndex() > 0xFFFF) {
  //     return;
  // }

  const std::vector<VariationPosition::Coordinate>& coordinates =
      font_args.GetVariationDesignPosition().GetCoordinates();
  std::vector<FT_Fixed> axis_values(coordinates.size());
  for (size_t i = 0; i < coordinates.size(); ++i) {
    axis_values[i] = FloatToFixedDot16(coordinates[i].value);
  }
  FT_Set_Var_Design_Coordinates(ft_face_.get(), axis_values.size(),
                                axis_values.data());
}

std::unique_ptr<FreetypeFace> FreetypeFace::MakeVariation(
    const FontArguments& args) const {
  return std::make_unique<FreetypeFace>(data_, args);
}

FontStyle FreetypeFace::GetFontStyle() {
  int weight = FontStyle::kNormal_Weight;
  int width = FontStyle::kNormal_Width;
  FontStyle::Slant slant = FontStyle::kUpright_Slant;
  if (ft_face_->style_flags & FT_STYLE_FLAG_BOLD) {
    weight = FontStyle::kBold_Weight;
  }
  if (ft_face_->style_flags & FT_STYLE_FLAG_ITALIC) {
    slant = FontStyle::kItalic_Slant;
  }

  TT_OS2* os2 =
      static_cast<TT_OS2*>(FT_Get_Sfnt_Table(ft_face_.get(), ft_sfnt_os2));
  if (os2 && os2->version != 0xffff) {
    weight = os2->usWeightClass;
    width = os2->usWidthClass;

    // OS/2::fsSelection bit 9 indicates oblique.
    if ((os2->fsSelection & (1u << 9)) != 0) {
      slant = FontStyle::kOblique_Slant;
    }
  }

  if (ft_face_->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS) {
    static constexpr FourByteTag wght_tag = SetFourByteTag('w', 'g', 'h', 't');
    static constexpr FourByteTag wdth_tag = SetFourByteTag('w', 'd', 't', 'h');
    static constexpr FourByteTag slnt_tag = SetFourByteTag('i', 't', 'a', 'l');

    VariationPosition position = FontScanner::GetVariationDesignPositionLocked(
        ft_face_.get(), library());
    for (auto& cordinate : position.GetCoordinates()) {
      if (cordinate.axis == wght_tag) {
        weight = RoundToInt(cordinate.value);
      } else if (cordinate.axis == wdth_tag) {
        width = FontStyle::WidthFromAxisWidth(cordinate.value);
      } else if (cordinate.axis == slnt_tag) {
        if (cordinate.value == 0) {
          slant = FontStyle::kUpright_Slant;
        } else {
          slant = FontStyle::kItalic_Slant;
        }
      }
    }
  }

  return FontStyle(weight, width, slant);
}

FT_Library FreetypeFace::library() {
  return global_freetype_library->library();
}

int FreetypeFace::library_ref_count_ = 0;

bool FreetypeFace::RefFreeTypeLibrary() {
  CHECK(library_ref_count_ >= 0);  // NOLINT
  if (0 == library_ref_count_) {
    global_freetype_library = new FreeTypeLibrary;
  }
  ++library_ref_count_;
  return global_freetype_library->library();
}

void FreetypeFace::UnrefFreeTypeLibrary() {
  CHECK(library_ref_count_ > 0);  // NOLINT
  --library_ref_count_;
  if (0 == library_ref_count_) {
    delete global_freetype_library;
  }
}

FontScanner::FontScanner() {
  FreetypeFace::RefFreeTypeLibrary();
  weight_map_.emplace("all", FontStyle::kNormal_Weight);
  weight_map_.emplace("black", FontStyle::kBlack_Weight);
  weight_map_.emplace("bold", FontStyle::kBold_Weight);
  weight_map_.emplace(
      "book", (FontStyle::kNormal_Weight + FontStyle::kLight_Weight) / 2);
  weight_map_.emplace("demi", FontStyle::kSemiBold_Weight);
  weight_map_.emplace("demibold", FontStyle::kSemiBold_Weight);
  weight_map_.emplace("extra", FontStyle::kExtraBold_Weight);
  weight_map_.emplace("extrabold", FontStyle::kExtraBold_Weight);
  weight_map_.emplace("extralight", FontStyle::kExtraLight_Weight);
  weight_map_.emplace("hairline", FontStyle::kThin_Weight);
  weight_map_.emplace("heavy", FontStyle::kBlack_Weight);
  weight_map_.emplace("light", FontStyle::kLight_Weight);
  weight_map_.emplace("medium", FontStyle::kMedium_Weight);
  weight_map_.emplace("normal", FontStyle::kNormal_Weight);
  weight_map_.emplace("plain", FontStyle::kNormal_Weight);
  weight_map_.emplace("regular", FontStyle::kNormal_Weight);
  weight_map_.emplace("roman", FontStyle::kNormal_Weight);
  weight_map_.emplace("semibold", FontStyle::kSemiBold_Weight);
  weight_map_.emplace("standard", FontStyle::kNormal_Weight);
  weight_map_.emplace("thin", FontStyle::kThin_Weight);
  weight_map_.emplace("ultra", FontStyle::kExtraBold_Weight);
  weight_map_.emplace("ultrablack", FontStyle::kExtraBlack_Weight);
  weight_map_.emplace("ultrabold", FontStyle::kExtraBold_Weight);
  weight_map_.emplace("ultraheavy", FontStyle::kExtraBlack_Weight);
  weight_map_.emplace("ultralight", FontStyle::kExtraLight_Weight);
}

FontScanner::~FontScanner() { FreetypeFace::UnrefFreeTypeLibrary(); }

bool FontScanner::RecognizedFont(std::shared_ptr<Data> stream,
                                 int* num_fonts) const {
  std::lock_guard<std::mutex> lock(library_mutex_);

  FT_StreamRec streamRec;
  UniqueFTFace face(this->OpenFace(std::move(stream), -1, &streamRec));
  if (!face) {
    return false;
  }
  *num_fonts = face->num_faces;
  return true;
}

static std::string LowerString(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    if ((c & 0x80) == 0) {  // is just ascii
      return std::tolower(c);
    } else {
      return static_cast<int>(c);
    }
  });
  return s;
}

bool FontScanner::ScanFont(std::shared_ptr<Data> stream, int ttcIndex,
                           std::string* name, FontStyle* style,
                           bool* is_fixed_pitch, AxisDefinitions* axes) const {
  std::lock_guard<std::mutex> lock(library_mutex_);

  FT_StreamRec streamRec;
  UniqueFTFace face(OpenFace(stream, ttcIndex, &streamRec));
  if (!face) {
    return false;
  }

  int weight = FontStyle::kNormal_Weight;
  int width = FontStyle::kNormal_Width;
  FontStyle::Slant slant = FontStyle::kUpright_Slant;
  if (face->style_flags & FT_STYLE_FLAG_BOLD) {
    weight = FontStyle::kBold_Weight;
  }
  if (face->style_flags & FT_STYLE_FLAG_ITALIC) {
    slant = FontStyle::kItalic_Slant;
  }

  bool hasAxes = face->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS;
  TT_OS2* os2 =
      static_cast<TT_OS2*>(FT_Get_Sfnt_Table(face.get(), ft_sfnt_os2));
  bool hasOs2 = os2 && os2->version != 0xffff;

  PS_FontInfoRec psFontInfo;

  if (hasOs2) {
    weight = os2->usWeightClass;
    width = os2->usWidthClass;

    // OS/2::fsSelection bit 9 indicates oblique.
    if (os2->fsSelection & (1u << 9)) {
      slant = FontStyle::kOblique_Slant;
    }
  }

  if (!hasOs2 && !hasAxes &&
      0 == FT_Get_PS_Font_Info(face.get(), &psFontInfo) && psFontInfo.weight) {
    std::string weight_lc = LowerString(psFontInfo.weight);
    if (weight_map_.find(weight_lc) == weight_map_.end()) {
      LOGI("Do not know weight for: %s (%s) \n", face->family_name,
           psFontInfo.weight);
    } else {
      weight = weight_map_.at(weight_lc);
    }
  }

  if (name != nullptr && face->family_name) {
    *name = face->family_name;
  }
  if (style != nullptr) {
    *style = FontStyle(weight, width, slant);
  }
  if (is_fixed_pitch != nullptr) {
    *is_fixed_pitch = FT_IS_FIXED_WIDTH(face);
  }

  // if (axes != nullptr && !GetAxes(face.get(), axes)) {
  //   return false;
  // }
  return true;
}

FT_Face FontScanner::OpenFace(std::shared_ptr<Data> stream, int ttcIndex,
                              FT_Stream ftStream) const {
  const void* memoryBase = stream->RawData();
  if (memoryBase) {
    FT_Open_Args args;
    memset(&args, 0, sizeof(args));
    args.flags = FT_OPEN_MEMORY;
    args.memory_base = (const FT_Byte*)memoryBase;
    args.memory_size = stream->Size();

    FT_Face face;
    if (FT_Open_Face(global_freetype_library->library(), &args, ttcIndex,
                     &face)) {
      return nullptr;
    }
    return face;
  }
  return nullptr;
}

VariationPosition FontScanner::GetVariationDesignPositionLocked(
    FT_Face face, FT_Library library) {
  VariationPosition position;
  if (!face) {
    return position;
  }
  if (!(face->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS)) {
    return position;
  }

  FT_MM_Var* variations = nullptr;
  if (FT_Get_MM_Var(face, &variations)) {
    FT_Done_MM_Var(library, variations);
    return position;
  }

  std::vector<FT_Fixed> coords(variations->num_axis);
  if (FT_Get_Var_Design_Coordinates(face, variations->num_axis,
                                    coords.data())) {
    FT_Done_MM_Var(library, variations);
    return position;
  }

  for (FT_UInt i = 0; i < variations->num_axis; ++i) {
    position.AddCoordinate(variations->axis[i].tag,
                           FixedDot16ToFloat(coords[i]));
  }

  FT_Done_MM_Var(library, variations);
  return position;
}

std::vector<VariationAxis> FontScanner::GetVariationDesignParametersLocked(
    FT_Face face, FT_Library library) {
  if (!face) {
    return {};
  }
  if (!(face->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS)) {
    return {};
  }

  FT_MM_Var* variations = nullptr;
  if (FT_Get_MM_Var(face, &variations)) {
    FT_Done_MM_Var(library, variations);
    return {};
  }

  std::vector<VariationAxis> axes;
  for (FT_UInt i = 0; i < variations->num_axis; ++i) {
    FourByteTag tag = variations->axis[i].tag;
    float min = FixedDot16ToFloat(variations->axis[i].minimum);
    float def = FixedDot16ToFloat(variations->axis[i].def);
    float max = FixedDot16ToFloat(variations->axis[i].maximum);
    FT_UInt flags = 0;
    bool hidden = !FT_Get_Var_Axis_Flags(variations, i, &flags) &&
                  (flags & FT_VAR_AXIS_FLAG_HIDDEN);

    axes.emplace_back(tag, min, def, max, hidden);
  }

  FT_Done_MM_Var(library, variations);
  return axes;
}

}  // namespace skity
