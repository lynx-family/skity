// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_PORTS_DARWIN_SCALER_CONTEXT_DARWIN_HPP
#define SRC_TEXT_PORTS_DARWIN_SCALER_CONTEXT_DARWIN_HPP

#include <skity/macros.hpp>

#if !defined(SKITY_MACOS) && !defined(SKITY_IOS)
#error "Only IOS or MacOS need this header file"
#endif

#include <CoreText/CoreText.h>

#include <skity/io/data.hpp>
#include <unordered_map>

#include "src/text/ports/darwin/types_darwin.hpp"
#include "src/text/scaler_context.hpp"

namespace skity {

class TypefaceDarwin;

class OffScreenContext final {
 public:
  OffScreenContext() = default;
  ~OffScreenContext() = default;

  void ResizeContext(uint32_t width, uint32_t height, bool need_color);

  CGColorSpaceRef GetCGColorSpace() const;

  void *GetAddr() const;

 private:
  UniqueCFRef<CGColorSpaceRef> cg_color_space_;
  std::shared_ptr<Data> pixel_data_;
};

class ScalerContextDarwin : public ScalerContext {
 public:
  ScalerContextDarwin(TypefaceDarwin *typeface, const ScalerContextDesc *desc);
  ~ScalerContextDarwin() override;

 protected:
  void GenerateMetrics(GlyphData *glyph) override;

  void GenerateImage(GlyphData *glyph, const StrokeDesc &stroke_desc) override;

  void GenerateImageInfo(GlyphData *glyph,
                         const StrokeDesc &stroke_desc) override;

  bool GeneratePath(GlyphData *glyph) override;

  void GenerateFontMetrics(FontMetrics *metrics) override;

  uint16_t OnGetFixedSize() override { return 0.f; }

 private:
  UniqueCTFontRef ct_font_;
  OffScreenContext os_context_;
  CGAffineTransform transform_;
  CGAffineTransform invert_transform_;
  float text_scale_;
  float context_scale_;
};

}  // namespace skity

#endif  // SRC_TEXT_PORTS_DARWIN_SCALER_CONTEXT_DARWIN_HPP
