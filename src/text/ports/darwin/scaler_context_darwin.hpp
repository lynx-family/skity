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

#include <cstddef>
#include <cstdint>
#include <skity/io/data.hpp>
#include <unordered_map>

#include "src/text/ports/darwin/types_darwin.hpp"
#include "src/text/scaler_context.hpp"

namespace skity {

class TypefaceDarwin;

class OffScreenContext final {
 public:
  // All pointers are borrowed and remain valid until the next context rebuild
  // or until this OffScreenContext is destroyed.
  struct Target {
    CGContextRef context = nullptr;
    uint8_t *storage = nullptr;
    // First row of the bottom-aligned active glyph rectangle.
    uint8_t *pixels = nullptr;
    size_t row_bytes = 0;
    uint32_t active_width = 0;
    uint32_t active_height = 0;
    uint32_t context_width = 0;
    uint32_t context_height = 0;
    bool context_was_created = false;

    explicit operator bool() const {
      return context != nullptr && pixels != nullptr;
    }
  };

  OffScreenContext(Color foreground_color);
  ~OffScreenContext();

  Target PrepareContext(uint32_t width, uint32_t height, bool need_color);

  // Returns true only when the corresponding CGContext setter is called.
  bool SetTextDrawingMode(CGTextDrawingMode mode);
  bool SetLineWidth(CGFloat width);
  bool SetLineCap(CGLineCap cap);
  bool SetLineJoin(CGLineJoin join);
  bool SetMiterLimit(CGFloat limit);
  bool SetShouldAntialias(bool should_antialias);

  CGColorRef GetCGColor() const;

 private:
  // CGContext state persists across glyph draws. The valid bits are reset
  // whenever PrepareContext replaces the underlying bitmap context.
  struct DrawState {
    bool text_drawing_mode_valid = false;
    CGTextDrawingMode text_drawing_mode = kCGTextFill;
    bool line_width_valid = false;
    CGFloat line_width = 0;
    bool line_cap_valid = false;
    CGLineCap line_cap = kCGLineCapButt;
    bool line_join_valid = false;
    CGLineJoin line_join = kCGLineJoinMiter;
    bool miter_limit_valid = false;
    CGFloat miter_limit = 0;
    bool should_antialias_valid = false;
    bool should_antialias = false;
  };

  static uint32_t RoundSize(uint32_t dimension);

  static void ClearActiveRect(const Target &target, size_t bytes_per_pixel);

  CGColorSpaceRef GetCGColorSpace(bool need_color) const;

  UniqueCFRef<CGColorSpaceRef> rgb_color_space_;
  UniqueCFRef<CGColorSpaceRef> gray_color_space_;
  UniqueCFRef<CGColorRef> foreground_color_;

  // CGContext borrows pixel_data_. Keep cg_context_ declared after the storage
  // so the context is released before its pixels during destruction.
  std::shared_ptr<Data> pixel_data_;
  UniqueCFRef<CGContextRef> cg_context_;
  uint32_t context_width_ = 0;
  uint32_t context_height_ = 0;
  size_t context_row_bytes_ = 0;
  bool need_color_ = false;
  DrawState draw_state_;

  static CGColorRef ColorToCGColor(CGColorSpaceRef rgbcs, Color bgra) {
    CGFloat components[4];
    components[0] = (CGFloat)ColorGetR(bgra) * (1 / 255.0f);
    components[1] = (CGFloat)ColorGetG(bgra) * (1 / 255.0f);
    components[2] = (CGFloat)ColorGetB(bgra) * (1 / 255.0f);
    components[3] = 1.0f;
    return CGColorCreate(rgbcs, components);
  }
};

class ScalerContextDarwin : public ScalerContext {
 public:
  ScalerContextDarwin(std::shared_ptr<TypefaceDarwin> typeface,
                      const ScalerContextDesc *desc);
  ~ScalerContextDarwin() override;

 protected:
  void GenerateMetrics(GlyphData *glyph) override;

  void GenerateImage(PackedGlyphID id, GlyphData *glyph,
                     const StrokeDesc &stroke_desc) override;

  void GenerateImageInfo(PackedGlyphID id, GlyphData *glyph,
                         const StrokeDesc &stroke_desc) override;

  bool GeneratePath(GlyphData *glyph) override;

  void GenerateFontMetrics(FontMetrics *metrics) override;

  uint16_t OnGetFixedSize() override { return 0.f; }

 private:
  void InitializeCGContext(CGContextRef context, bool is_color);

  void DrawGlyphWithState(CGContextRef context, CGGlyph glyph, CGPoint point,
                          const StrokeDesc *stroke_desc);

  UniqueCTFontRef ct_font_;
  OffScreenContext os_context_;
  CGAffineTransform transform_;
  CGAffineTransform invert_transform_;
  float text_scale_;
  float context_scale_;
};

}  // namespace skity

#endif  // SRC_TEXT_PORTS_DARWIN_SCALER_CONTEXT_DARWIN_HPP
