// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_SCALER_CONTEXT_HPP
#define SRC_TEXT_SCALER_CONTEXT_HPP

#include <cstdint>
#include <skity/graphic/paint.hpp>
#include <skity/text/font.hpp>
#include <skity/text/typeface.hpp>

#include "src/text/scaler_context_desc.hpp"

namespace skity {

struct StrokeDesc {
  bool operator==(const StrokeDesc& other) const {
    return operator()(*this, other);
  }
  bool operator()(const StrokeDesc& a, const StrokeDesc& b) const {
    return a.is_stroke == b.is_stroke && a.stroke_width == b.stroke_width;
  }

  bool is_stroke;
  float stroke_width;
  Paint::Cap cap;
  Paint::Join join;
  float miter_limit;
};

struct FontDesc {
  bool operator==(const FontDesc& other) const {
    return operator()(*this, other);
  }
  bool operator()(const FontDesc& a, const FontDesc& b) const {
    return a.embolden == b.embolden;
  }
  bool embolden;
  // italic and some other general transform on glyphs
};

// TOOD(jingle) Refactor me. Find a better way to add fields to descriptor, for
// example hash for bytes array
class Descriptor {
 public:
  Descriptor() = default;
  Descriptor(uint32_t font_id, float text_size, float scale,
             const StrokeDesc& stroke_desc, const FontDesc& font_desc)
      : font_id_(font_id),
        text_size_(text_size),
        scale_(scale),
        stroke_desc_(stroke_desc),
        font_desc_(font_desc) {}
  bool operator==(const Descriptor& other) const {
    return operator()(*this, other);
  }
  bool operator()(const Descriptor& a, const Descriptor& b) const {
    return a.font_id_ == b.font_id_ && a.text_size_ == b.text_size_ &&
           a.scale_ == b.scale_ && a.stroke_desc_ == b.stroke_desc_ &&
           a.font_desc_ == b.font_desc_;
  }
  uint32_t font_id_;
  float text_size_;
  float scale_;
  StrokeDesc stroke_desc_;
  FontDesc font_desc_;
};

class ScalerContext {
 public:
  ScalerContext(Typeface* typeface, const ScalerContextDesc* desc);
  virtual ~ScalerContext() = default;

 public:
  const ScalerContextDesc& GetDesc() const { return desc_; }
  Typeface* GetTypeface() { return typeface_; }
  void MakeGlyph(GlyphData* glyph_data);
  void GetImage(GlyphData* glyph, const StrokeDesc& stroke_desc);
  void GetImageInfo(GlyphData* glyph, const StrokeDesc& stroke_desc);
  void GetPath(GlyphData* glyph);
  void GetFontMetrics(FontMetrics* metrics);
  bool IsVertical() { return false; }
  bool IsSubpixel() { return false; }
  uint16_t GetFixedSize() { return this->OnGetFixedSize(); }

 protected:
  //  virtual bool GenerateAdvance(GlyphData* glyph) = 0;
  virtual void GenerateMetrics(GlyphData* glyph) = 0;
  virtual void GenerateImage(GlyphData* glyph,
                             const StrokeDesc& stroke_desc) = 0;
  virtual void GenerateImageInfo(GlyphData* glyph,
                                 const StrokeDesc& stroke_desc) = 0;
  virtual bool GeneratePath(GlyphData* glyph) = 0;
  virtual void GenerateFontMetrics(FontMetrics*) = 0;
  virtual uint16_t OnGetFixedSize() = 0;

 protected:
  Typeface* typeface_;
  ScalerContextDesc desc_;
};
}  // namespace skity

#endif  // SRC_TEXT_SCALER_CONTEXT_HPP
