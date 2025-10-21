/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_PORTS_TYPEFACE_FREETYPE_HPP
#define SRC_TEXT_PORTS_TYPEFACE_FREETYPE_HPP

#include <ft2build.h>

#include <memory>

#include FT_FREETYPE_H
#include <mutex>
#include <skity/geometry/rect.hpp>
#include <skity/text/font_style.hpp>
#include <skity/text/typeface.hpp>
#include <unordered_map>

#include "src/text/ports/freetype_face.hpp"

namespace skity {
class FreetypeFace;
class ScalerContextFreetype;
class AutoFTAccess;

struct FaceData {
  std::shared_ptr<Data> data;
  FontArguments font_args;
};

class FreetypeFaceHolder {
 public:
  static std::unique_ptr<FreetypeFaceHolder> Make(
      std::shared_ptr<Data> stream, const FontArguments& font_args);

  ~FreetypeFaceHolder() = default;
  FreetypeFace* GetFreetypeFace() const { return face_.get(); }

 private:
  explicit FreetypeFaceHolder(std::unique_ptr<FreetypeFace> face)
      : face_(std::move(face)) {}

  std::unique_ptr<FreetypeFace> face_;
};

class TypefaceFreeType : public Typeface {
  friend ScalerContextFreetype;
  friend AutoFTAccess;

 public:
  static constexpr uint32_t kFontFactoryID = SetFourByteTag('f', 'r', 'e', 'e');

  static std::shared_ptr<TypefaceFreeType> Make(std::shared_ptr<Data> stream,
                                                const FontArguments& font_args);

  explicit TypefaceFreeType(const FontStyle& style);

  ~TypefaceFreeType() override = default;

  int OnGetTableTags(FontTableTag tags[]) const override;

  size_t OnGetTableData(FontTableTag, size_t offset, size_t length,
                        void* data) const override;

  void OnCharsToGlyphs(const uint32_t* chars, int count,
                       GlyphID glyphs[]) const override;
  std::shared_ptr<Data> OnGetData() override;
  bool OnContainsColorTable() const override;

  FaceData GetFaceData() const;

  // be used internally
 public:
  bool IsVariationTypeface();

 protected:
  FreetypeFace* GetFTFace() const;
  uint32_t OnGetUPEM() const override;

  std::unique_ptr<ScalerContext> OnCreateScalerContext(
      const ScalerContextDesc* desc) const override;

  VariationPosition OnGetVariationDesignPosition() const override;
  std::vector<VariationAxis> OnGetVariationDesignParameters() const override;

  std::shared_ptr<Typeface> OnMakeVariation(
      const FontArguments& args) const override;

  virtual FaceData OnGetFaceData() const = 0;

  void OnGetFontDescriptor(FontDescriptor& desc) const override;

 private:
  mutable std::once_flag flag_;
  mutable std::unique_ptr<FreetypeFaceHolder> freetype_face_holder_;
  mutable std::mutex C2GCacheMutex_;
  mutable std::unordered_map<Unichar, GlyphID> C2GCache_;
};

class TypefaceFreeTypeData : public TypefaceFreeType {
 public:
  TypefaceFreeTypeData(std::shared_ptr<Data> data,
                       const FontArguments& font_args, const FontStyle& style);

  ~TypefaceFreeTypeData() override = default;

  FaceData OnGetFaceData() const override;

 private:
  std::shared_ptr<Data> data_;
  FontArguments font_args_;
};

}  // namespace skity

#endif  // SRC_TEXT_PORTS_TYPEFACE_FREETYPE_HPP
