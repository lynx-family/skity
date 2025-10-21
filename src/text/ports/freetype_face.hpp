// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_PORTS_FREETYPE_FACE_HPP
#define SRC_TEXT_PORTS_FREETYPE_FACE_HPP

#include <ft2build.h>
#include FT_FREETYPE_H

#include <mutex>
#include <skity/graphic/path.hpp>
#include <skity/io/data.hpp>
#include <skity/text/font_arguments.hpp>
#include <skity/text/font_style.hpp>
#include <string>
#include <unordered_map>

#include "src/utils/function_wrapper.hpp"

namespace skity {
using UniqueFTFace =
    std::unique_ptr<FT_FaceRec,
                    FunctionWrapper<decltype(FT_Done_Face), FT_Done_Face>>;

#define Fixed1 (1 << 16)
#define SK_MaxS32FitsInFloat 2147483520
#define SK_MinS32FitsInFloat -SK_MaxS32FitsInFloat
#define ScalarToFDot6(x) (static_cast<int32_t>((x) * 64))
#define FDot6ToScalar(x) (static_cast<float>((x) * 0.015625f))
#define FDot6Floor(x) ((x) >> 6)
#define FDot6Ceil(x) (((x) + 63) >> 6)
#define FDot6Round(x) (((x) + 32) >> 6)
#define FixedToFDot6(x) ((x) >> 10)

class FreeTypeLibrary {
 public:
  FreeTypeLibrary();
  ~FreeTypeLibrary();

  FT_Library library() { return ft_library_; }

 private:
  FT_Library ft_library_;
};

class FreetypeFace {
 public:
  explicit FreetypeFace(const std::shared_ptr<Data>& stream,
                        const FontArguments& font_args);
  ~FreetypeFace();

  static std::mutex& f_t_mutex() {
    static std::mutex mutex;
    return mutex;
  }

  FT_Face Face() { return ft_face_ ? ft_face_.get() : nullptr; }
  bool Valid() { return !!ft_face_; }

  std::shared_ptr<Data> GetData() { return data_; }

  std::unique_ptr<FreetypeFace> MakeVariation(const FontArguments& args) const;

  FontStyle GetFontStyle();

  FT_Library library();

 private:
  std::shared_ptr<Data> data_;
  UniqueFTFace ft_face_;

  void SetupVariation(const FontArguments& font_args);

  // Private to ref_ft_library and unref_ft_library
  static int library_ref_count_;
  static bool RefFreeTypeLibrary();
  static void UnrefFreeTypeLibrary();

  friend class FontScanner;
};

typedef uint32_t FourByteTag;

class FontScanner {
 public:
  FontScanner();
  ~FontScanner();
  struct AxisDefinition {
    FourByteTag fTag;
    int32_t fMinimum;
    int32_t fDefault;
    int32_t fMaximum;
  };
  using AxisDefinitions = std::array<AxisDefinition, 4>;
  bool RecognizedFont(std::shared_ptr<Data> stream, int* num_fonts) const;
  bool ScanFont(std::shared_ptr<Data> stream, int ttcIndex, std::string* name,
                FontStyle* style, bool* is_fixed_pitch,
                AxisDefinitions* axes) const;

  static VariationPosition GetVariationDesignPositionLocked(FT_Face face,
                                                            FT_Library library);
  static std::vector<VariationAxis> GetVariationDesignParametersLocked(
      FT_Face face, FT_Library library);

 private:
  FT_Face OpenFace(std::shared_ptr<Data> stream, int ttcIndex,
                   FT_Stream ftStream) const;

  std::unordered_map<std::string, int> weight_map_;
  mutable std::mutex library_mutex_;
};

}  // namespace skity

#endif  // SRC_TEXT_PORTS_FREETYPE_FACE_HPP
